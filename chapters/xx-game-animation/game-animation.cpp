#include "opal/container/ref.h"
#include "opal/container/scope-ptr.h"
#include "opal/container/string.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/file.h"
#include "rndr/fly-camera.h"
#include "rndr/input-layout-builder.h"
#include "rndr/render-api.h"
#include "rndr/renderer-base.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "assimp-helpers.h"
#include "imgui-wrapper.h"
#include "types.h"

void Run();

struct AppState
{
    f32 delta_seconds = 1 / 60.0f;
};

class SceneRenderer final : public Rndr::RendererBase
{
public:
    SceneRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc, AppState* app_state, Rndr::ProjectionCamera* camera)
        : Rndr::RendererBase(name, desc), m_app_state(app_state), m_camera(camera)
    {
        const Opal::StringUtf8 asset_base = Opal::Paths::Combine(nullptr, ASSETS_ROOT, "game-animation").GetValue();
        const Opal::StringUtf8 model_path = Opal::Paths::Combine(nullptr, asset_base, "woman.gltf").GetValue();
        const Opal::StringUtf8 texture_path = Opal::Paths::Combine(nullptr, asset_base, "woman.png").GetValue();
        const Opal::StringUtf8 vertex_shader_path = "gltf.vert";
        const Opal::StringUtf8 fragment_shader_path = "gltf.frag";

        if (!AssimpHelpers::ReadMeshData(m_mesh_data, model_path, MeshAttributesToLoad::LoadAll))
        {
            RNDR_LOG_ERROR("Failed to load model: %s", model_path.GetData());
            RNDR_HALT("");
        }

        SkeletalMeshData skeletal_mesh;
        AssimpHelpers::ReadAnimationDataFromAssimp(skeletal_mesh, model_path);

        Rndr::Bitmap bitmap = Rndr::File::ReadEntireImage(texture_path, Rndr::PixelFormat::R8G8B8A8_UNORM, true);
        RNDR_ASSERT(bitmap.IsValid());

        m_texture.Initialize(
            desc.graphics_context,
            Rndr::TextureDesc{.width = bitmap.GetWidth(), .height = bitmap.GetHeight(), .pixel_format = bitmap.GetPixelFormat()}, {},
            Opal::ArrayView<const u8>(bitmap.GetData(), bitmap.GetSize2D()));
        RNDR_ASSERT(m_texture.IsValid());

        const Opal::StringUtf8 vertex_shader_source = Rndr::File::ReadShader(asset_base, vertex_shader_path);
        RNDR_ASSERT(!vertex_shader_source.IsEmpty());
        m_vertex_shader.Initialize(desc.graphics_context,
                                   Rndr::ShaderDesc{.type = Rndr::ShaderType::Vertex, .source = vertex_shader_source});
        RNDR_ASSERT(m_vertex_shader.IsValid());

        const Opal::StringUtf8 fragment_shader_source = Rndr::File::ReadShader(asset_base, fragment_shader_path);
        RNDR_ASSERT(!fragment_shader_source.IsEmpty());
        m_fragment_shader.Initialize(desc.graphics_context,
                                     Rndr::ShaderDesc{.type = Rndr::ShaderType::Fragment, .source = fragment_shader_source});
        RNDR_ASSERT(m_fragment_shader.IsValid());

        m_vertex_buffer.Initialize(desc.graphics_context,
                                   Rndr::BufferDesc{.type = Rndr::BufferType::Vertex,
                                                    .size = m_mesh_data.vertex_buffer_data.GetSize(),
                                                    .stride = static_cast<i64>(m_mesh_data.meshes[0].vertex_size)},
                                   Opal::AsBytes(m_mesh_data.vertex_buffer_data));
        RNDR_ASSERT(m_vertex_buffer.IsValid());

        Rndr::Matrix4x4f world_from_model = Math::Identity<f32>();
        m_instance_buffer.Initialize(
            desc.graphics_context,
            Rndr::BufferDesc{.type = Rndr::BufferType::Vertex, .size = sizeof(Rndr::Matrix4x4f), .stride = sizeof(Rndr::Matrix4x4f)},
            Opal::AsBytes(world_from_model));
        RNDR_ASSERT(m_instance_buffer.IsValid());

        m_index_buffer.Initialize(
            desc.graphics_context,
            Rndr::BufferDesc{.type = Rndr::BufferType::Index, .size = m_mesh_data.index_buffer_data.GetSize(), .stride = 4},
            Opal::AsBytes(m_mesh_data.index_buffer_data));
        RNDR_ASSERT(m_index_buffer.IsValid());

        const Rndr::InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                            .AddVertexBuffer(m_vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
                                                            .AppendElement(0, Rndr::PixelFormat::R32G32_FLOAT)
                                                            .AddVertexBuffer(m_instance_buffer, 1, Rndr::DataRepetition::PerInstance)
                                                            .AppendElement(1, Rndr::PixelFormat::R32G32B32A32_FLOAT)
                                                            .AppendElement(1, Rndr::PixelFormat::R32G32B32A32_FLOAT)
                                                            .AppendElement(1, Rndr::PixelFormat::R32G32B32A32_FLOAT)
                                                            .AppendElement(1, Rndr::PixelFormat::R32G32B32A32_FLOAT)
                                                            .AddIndexBuffer(m_index_buffer)
                                                            .Build();

        m_pipeline = Rndr::Pipeline(desc.graphics_context, Rndr::PipelineDesc{.vertex_shader = &m_vertex_shader,
                                                                              .pixel_shader = &m_fragment_shader,
                                                                              .input_layout = input_layout_desc,
                                                                              .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_pipeline.IsValid());

        m_constant_buffer.Initialize(desc.graphics_context, Rndr::BufferDesc{.type = Rndr::BufferType::Constant,
                                                                             .usage = Rndr::Usage::Dynamic,
                                                                             .size = sizeof(Rndr::Matrix4x4f),
                                                                             .stride = sizeof(Rndr::Matrix4x4f)});
        RNDR_ASSERT(m_constant_buffer.IsValid());
    }

    bool Render() override
    {
        Rndr::Matrix4x4f clip_from_world = m_camera->FromWorldToNDC();
        clip_from_world = Math::Transpose(clip_from_world);
        m_desc.graphics_context->UpdateBuffer(m_constant_buffer, Opal::AsBytes(clip_from_world));

        m_desc.graphics_context->BindPipeline(m_pipeline);
        m_desc.graphics_context->BindBuffer(m_constant_buffer, 0);
        m_desc.graphics_context->BindTexture(m_texture, 0);

        m_desc.graphics_context->DrawIndices(Rndr::PrimitiveTopology::Triangle, m_mesh_data.meshes[0].lod_offsets[1]);

        return true;
    }

private:
    Opal::Ref<AppState> m_app_state;
    Opal::Ref<Rndr::ProjectionCamera> m_camera;
    Rndr::Texture m_texture;
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_fragment_shader;
    Rndr::Pipeline m_pipeline;
    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_instance_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Buffer m_constant_buffer;
    MeshData m_mesh_data;
};

class UIRenderer final : public Rndr::RendererBase
{
public:
    UIRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc, Rndr::Window* window, AppState* app_state)
        : Rndr::RendererBase(name, desc), m_window(window), m_app_state(app_state)
    {
        ImGuiWrapper::Init(*window, desc.graphics_context);
    }

    ~UIRenderer() override { ImGuiWrapper::Destroy(); }

    bool Render() override
    {
        ImGuiWrapper::StartFrame();

        ImGui::Begin("Game Animation");
        ImGui::Text("Frame Rate: %.1f FPS", 1.0f / m_app_state->delta_seconds);
        ImGui::End();

        ImGuiWrapper::EndFrame();
        return true;
    }

private:
    Opal::Ref<Rndr::Window> m_window;
    Opal::Ref<AppState> m_app_state;
};

int main()
{
    Rndr::Init(Rndr::RndrDesc{.enable_input_system = true});
    Run();
    Rndr::Destroy();
    return 0;
}

void Run()
{
    Rndr::Window window(Rndr::WindowDesc{.width = 800, .height = 600, .name = "Game Animation"});
    Rndr::GraphicsContext graphics_context(Rndr::GraphicsContextDesc{.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context,
                               Rndr::SwapChainDesc{.width = window.GetWidth(), .height = window.GetHeight(), .enable_vsync = false});
    RNDR_ASSERT(swap_chain.IsValid());

    AppState app_state;

    Rndr::FlyCamera camera(&window, &Rndr::InputSystem::GetCurrentContext(), {.start_position = Rndr::Point3f(0.0f, 0.0f, 15.0f),
                                                                                 .movement_speed = 100,
                                                                                 .rotation_speed = 200});

    Opal::ScopePtr<Rndr::RendererBase> clear_renderer = Opal::MakeDefaultScoped<Rndr::ClearRenderer>(
        "Clear Renderer", Rndr::RendererBaseDesc{.graphics_context = Opal::Ref(&graphics_context), .swap_chain = Opal::Ref(&swap_chain)},
        Rndr::Colors::k_black);
    Opal::ScopePtr<Rndr::RendererBase> scene_renderer = Opal::MakeDefaultScoped<SceneRenderer>(
        "Scene Renderer", Rndr::RendererBaseDesc{.graphics_context = Opal::Ref(&graphics_context), .swap_chain = Opal::Ref(&swap_chain)},
        &app_state, &camera);
    Opal::ScopePtr<Rndr::RendererBase> ui_renderer = Opal::MakeDefaultScoped<UIRenderer>(
        "UI Renderer", Rndr::RendererBaseDesc{.graphics_context = Opal::Ref(&graphics_context), .swap_chain = Opal::Ref(&swap_chain)},
        &window, &app_state);
    Opal::ScopePtr<Rndr::RendererBase> present_renderer = Opal::MakeDefaultScoped<Rndr::PresentRenderer>(
        "Present Renderer",
        Rndr::RendererBaseDesc{.graphics_context = Opal::Ref(&graphics_context), .swap_chain = Opal::Ref(&swap_chain)});

    Rndr::RendererManager renderer_manager;
    renderer_manager.AddRenderer(clear_renderer.Get());
    renderer_manager.AddRenderer(scene_renderer.Get());
    renderer_manager.AddRenderer(ui_renderer.Get());
    renderer_manager.AddRenderer(present_renderer.Get());

    f32 delta_seconds = 1 / 60.0f;
    while (!window.IsClosed())
    {
        const f64 start_time = Opal::GetSeconds();

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

        camera.Update(delta_seconds);

        renderer_manager.Render();

        const f64 end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_time - start_time);
        app_state.delta_seconds = delta_seconds;
    }
}
