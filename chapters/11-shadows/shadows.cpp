#include <imgui.h>

#include "opal/container/string.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/file.h"
#include "rndr/fly-camera.h"
#include "rndr/frames-per-second-counter.h"
#include "rndr/input-layout-builder.h"
#include "rndr/input.h"
#include "rndr/platform/opengl-frame-buffer.h"
#include "rndr/render-api.h"
#include "rndr/renderer-base.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "assimp-helpers.h"
#include "imgui-wrapper.h"
#include "mesh.h"
#include "types.h"

void Run();

int main()
{
    Rndr::Init({.enable_input_system = true});
    Run();
    Rndr::Destroy();
    return 0;
}

struct PerFrameData
{
};

struct GameState
{
    f32 light_fov = 60.0f;
    f32 light_distance = 12.0f;
    f32 light_inner_angle = 10.0f;
    f32 light_near = 1.0f;
    f32 light_far = 20.0f;
    f32 light_x_angle = -1.0f;
    f32 light_y_angle = -2.0f;

    // Set by shadow renderer
    Rndr::Matrix4x4f light_clip_from_world;
    Rndr::Point3f light_position;
};

class MeshContainer
{
public:
    explicit MeshContainer(Rndr::GraphicsContext* graphics_context) : m_graphics_context(graphics_context)
    {
        // Setup vertex and index buffers
        const Opal::StringUtf8 mesh_file_path = Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("duck.gltf")).GetValue();
        [[maybe_unused]] bool status = AssimpHelpers::ReadMeshData(m_mesh_data, mesh_file_path, MeshAttributesToLoad::LoadAll);
        RNDR_ASSERT(status);
        Rndr::ErrorCode err;
        err = Mesh::AddPlaneXZ(m_mesh_data, Rndr::Point3f(0.0f, 0.0f, 0.0f), 20.0f, MeshAttributesToLoad::LoadAll);
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        err = m_vertex_buffer.Initialize(
            m_graphics_context, Rndr::BufferDesc{.type = Rndr::BufferType::ShaderStorage, .size = m_mesh_data.vertex_buffer_data.GetSize()},
            Opal::AsBytes(m_mesh_data.vertex_buffer_data));
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        m_model_matrices.PushBack(Math::Identity<f32>() * Math::RotateY(-90.0f) * Math::RotateX(-90.0f) * Math::Scale(4.0f));
        m_model_matrices[0] = Math::Transpose(m_model_matrices[0]);
        m_model_matrices.PushBack(Math::Identity<f32>());
        m_model_matrices[1] = Math::Transpose(m_model_matrices[1]);

        err = m_model_buffer.Initialize(
            m_graphics_context,
            Rndr::BufferDesc{.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = sizeof(Rndr::Matrix4x4f)});
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        err = m_index_buffer.Initialize(
            m_graphics_context,
            Rndr::BufferDesc{.type = Rndr::BufferType::Index, .size = m_mesh_data.index_buffer_data.GetSize(), .stride = 4},
            Opal::AsBytes(m_mesh_data.index_buffer_data));
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        // Setup input layout
        m_input_layout_desc = Rndr::InputLayoutBuilder()
                                  .AddShaderStorage(m_vertex_buffer, 1)
                                  .AddShaderStorage(m_model_buffer, 2)
                                  .AddIndexBuffer(m_index_buffer)
                                  .Build();

        const Opal::StringUtf8 albedo_texture_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("duck-base-color.png")).GetValue();
        Rndr::Bitmap bitmap = Rndr::File::ReadEntireImage(albedo_texture_path, Rndr::PixelFormat::R8G8B8A8_UNORM, true);
        RNDR_ASSERT(bitmap.IsValid());
        err = m_albedo_texture.Initialize(
            m_graphics_context,
            {.width = bitmap.GetWidth(), .height = bitmap.GetHeight(), .pixel_format = bitmap.GetPixelFormat(), .use_mips = true}, {},
            Opal::Span<const u8>(bitmap.GetData(), bitmap.GetSize2D()));
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        const Opal::StringUtf8 brick_texture_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("brick-wall.jpg")).GetValue();
        bitmap = Rndr::File::ReadEntireImage(brick_texture_path, Rndr::PixelFormat::R8G8B8A8_UNORM, true);
        RNDR_ASSERT(bitmap.IsValid());
        err = m_brick_texture.Initialize(
            m_graphics_context,
            {.width = bitmap.GetWidth(), .height = bitmap.GetHeight(), .pixel_format = bitmap.GetPixelFormat(), .use_mips = true}, {},
            Opal::Span<const u8>(bitmap.GetData(), bitmap.GetSize2D()));
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);
    }

    [[nodiscard]] const Rndr::InputLayoutDesc& GetInputLayoutDesc() const { return m_input_layout_desc; }

    void Draw()
    {
        m_graphics_context->UpdateBuffer(m_model_buffer, Opal::AsBytes(m_model_matrices[0]));
        m_graphics_context->BindTexture(m_albedo_texture, 0);
        m_graphics_context->DrawIndices(Rndr::PrimitiveTopology::Triangle, m_mesh_data.meshes[0].lod_offsets[1], 1, 0);
        m_graphics_context->UpdateBuffer(m_model_buffer, Opal::AsBytes(m_model_matrices[1]));
        m_graphics_context->BindTexture(m_brick_texture, 0);
        m_graphics_context->DrawIndices(Rndr::PrimitiveTopology::Triangle, m_mesh_data.meshes[1].lod_offsets[1], 1,
                                        static_cast<i32>(m_mesh_data.meshes[1].index_offset));
    }

private:
    Opal::Ref<Rndr::GraphicsContext> m_graphics_context;
    MeshData m_mesh_data;
    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_model_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Texture m_albedo_texture;
    Rndr::Texture m_brick_texture;
    Rndr::InputLayoutDesc m_input_layout_desc;
    Opal::Array<Rndr::Matrix4x4f> m_model_matrices;
};

class ShadowRenderer : public Rndr::RendererBase
{
public:
    ShadowRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc, MeshContainer* mesh_container, GameState* game_state)
        : RendererBase(name, desc), m_mesh_container(mesh_container), m_game_state(game_state)
    {
        // Setup shaders
        const Opal::StringUtf8 shader_dir = Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("shaders")).GetValue();
        const Opal::StringUtf8 vertex_shader_contents = Rndr::File::ReadShader(shader_dir, u8"shadow.vert");
        const Opal::StringUtf8 pixel_shader_contents = Rndr::File::ReadShader(shader_dir, u8"shadow.frag");
        Rndr::ErrorCode err =
            m_vertex_shader.Initialize(m_desc.graphics_context, {.type = Rndr::ShaderType::Vertex, .source = vertex_shader_contents});
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);
        err = m_pixel_shader.Initialize(m_desc.graphics_context, {.type = Rndr::ShaderType::Fragment, .source = pixel_shader_contents});
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        // Setup pipeline
        m_pipeline = Rndr::Pipeline(m_desc.graphics_context,
                                    {
                                        .vertex_shader = &m_vertex_shader,
                                        .pixel_shader = &m_pixel_shader,
                                        .input_layout = m_mesh_container->GetInputLayoutDesc(),
                                        .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                        //                                                                 .blend = {.is_enabled = false},
                                        .depth_stencil = {.is_depth_enabled = true},
                                    });
        RNDR_ASSERT(m_pipeline.IsValid());

        // Setup frame buffer
        m_frame_buffer = Rndr::FrameBuffer(
            m_desc.graphics_context,
            Rndr::FrameBufferDesc{
                .color_attachments = {{.width = 1024, .height = 1024, .pixel_format = Rndr::PixelFormat::R8G8B8A8_UNORM}},
                .color_attachment_samplers = {{}},
                .use_depth_stencil = true,
                .depth_stencil_attachment = {.width = 1024, .height = 1024, .pixel_format = Rndr::PixelFormat::D24_UNORM_S8_UINT}});

        m_per_frame_buffer.Initialize(
            m_desc.graphics_context,
            Rndr::BufferDesc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = sizeof(Rndr::Matrix4x4f)});
    }

    bool Render() override
    {
        const Rndr::Point3f light_position = Math::RotateY(Math::Degrees(m_game_state->light_y_angle)) *
                                             Math::RotateX(Math::Degrees(m_game_state->light_x_angle)) *
                                             Rndr::Point3f(0, 0, m_game_state->light_distance);
        const Rndr::Matrix4x4f light_view = Math::LookAt_RH(light_position, Rndr::Point3f(0, 0, 0), Rndr::Vector3f(0, 1, 0));
        const Rndr::Matrix4x4f light_projection =
            Math::Perspective_RH_N1(m_game_state->light_fov, 1.0f, m_game_state->light_near, m_game_state->light_far);
        Rndr::Matrix4x4f mvp = light_projection * light_view;
        m_game_state->light_clip_from_world = mvp;
        m_game_state->light_position = light_position;
        mvp = Math::Transpose(mvp);  // OpenGL expects column-major matrices

        m_desc.graphics_context->UpdateBuffer(m_per_frame_buffer, Opal::AsBytes(mvp));
        m_desc.graphics_context->ClearFrameBufferColorAttachment(m_frame_buffer, 0, Rndr::Colors::k_black);
        m_desc.graphics_context->ClearFrameBufferDepthStencilAttachment(m_frame_buffer, 1.0f, 0);
        m_desc.graphics_context->BindFrameBuffer(m_frame_buffer);
        m_desc.graphics_context->BindPipeline(m_pipeline);
        m_desc.graphics_context->BindBuffer(m_per_frame_buffer, 0);
        m_mesh_container->Draw();
        m_desc.graphics_context->BindSwapChainFrameBuffer(m_desc.swap_chain);
        return true;
    }

    const Rndr::FrameBuffer* GetFrameBuffer() const { return &m_frame_buffer; }

private:
    Opal::Ref<MeshContainer> m_mesh_container;
    Opal::Ref<GameState> m_game_state;
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_pixel_shader;
    Rndr::Pipeline m_pipeline;
    Rndr::FrameBuffer m_frame_buffer;
    Rndr::Buffer m_per_frame_buffer;
};

class SceneRenderer : public Rndr::RendererBase
{
public:
    SceneRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc, MeshContainer* mesh_container, GameState* game_state,
                  const Rndr::Texture* shadow_texture, Rndr::ProjectionCamera* camera)
        : RendererBase(name, desc),
          m_mesh_container(mesh_container),
          m_game_state(game_state),
          m_camera(camera),
          m_shadow_texture(shadow_texture)
    {
        // Setup shaders
        const Opal::StringUtf8 shader_dir = Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("shaders")).GetValue();
        const Opal::StringUtf8 vertex_shader_contents = Rndr::File::ReadShader(shader_dir, u8"scene-shadow.vert");
        const Opal::StringUtf8 pixel_shader_contents = Rndr::File::ReadShader(shader_dir, u8"scene-shadow.frag");
        Rndr::ErrorCode err =
            m_vertex_shader.Initialize(m_desc.graphics_context, {.type = Rndr::ShaderType::Vertex, .source = vertex_shader_contents});
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);
        err = m_pixel_shader.Initialize(m_desc.graphics_context, {.type = Rndr::ShaderType::Fragment, .source = pixel_shader_contents});
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        // Setup pipeline
        m_pipeline = Rndr::Pipeline(m_desc.graphics_context, {
                                                                 .vertex_shader = &m_vertex_shader,
                                                                 .pixel_shader = &m_pixel_shader,
                                                                 .input_layout = m_mesh_container->GetInputLayoutDesc(),
                                                                 .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                                 .depth_stencil = {.is_depth_enabled = true},
                                                             });
        RNDR_ASSERT(m_pipeline.IsValid());

        m_per_frame_buffer.Initialize(
            m_desc.graphics_context,
            Rndr::BufferDesc{.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = sizeof(PerFrameData)});
    }

    struct PerFrameData
    {
        Rndr::Matrix4x4f clip_from_world;
        Rndr::Matrix4x4f light_clip_from_world;
        Rndr::Point4f camera_position;
        Rndr::Vector4f light_angles;  // cos(inner), cos(outer), 0, 0
        Rndr::Point4f light_position;
    };

    bool Render() override
    {
        PerFrameData per_frame_data;
        per_frame_data.clip_from_world = Math::Transpose(m_camera->FromWorldToNDC());
        per_frame_data.light_clip_from_world = Math::Transpose(m_game_state->light_clip_from_world);
        per_frame_data.camera_position =
            Rndr::Point4f(m_camera->GetPosition().x, m_camera->GetPosition().y, m_camera->GetPosition().z, 1.0f);
        per_frame_data.light_angles =
            Rndr::Vector4f(Math::Cos(Math::Radians(0.5f * m_game_state->light_fov)),
                           Math::Cos(Math::Radians(0.5f * (m_game_state->light_fov - m_game_state->light_inner_angle))), 1.0f, 1.0f);
        per_frame_data.light_position =
            Rndr::Point4f(m_game_state->light_position.x, m_game_state->light_position.y, m_game_state->light_position.z, 1.0f);

        m_desc.graphics_context->UpdateBuffer(m_per_frame_buffer, Opal::AsBytes(per_frame_data));
        m_desc.graphics_context->BindSwapChainFrameBuffer(m_desc.swap_chain);
        m_desc.graphics_context->BindPipeline(m_pipeline);
        m_desc.graphics_context->BindBuffer(m_per_frame_buffer, 0);
        m_desc.graphics_context->BindTexture(*m_shadow_texture, 1);
        m_mesh_container->Draw();
        return true;
    }

private:
    Opal::Ref<MeshContainer> m_mesh_container;
    Opal::Ref<GameState> m_game_state;
    Opal::Ref<Rndr::ProjectionCamera> m_camera;
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_pixel_shader;
    Rndr::Pipeline m_pipeline;
    Rndr::Buffer m_per_frame_buffer;
    Opal::Ref<const Rndr::Texture> m_shadow_texture;
};

class PostProcessRenderer : public Rndr::RendererBase
{
public:
    PostProcessRenderer(const Rndr::RendererBaseDesc& desc, bool use_full_screen_triangle = false)
        : RendererBase(u8"Post Process Renderer", desc), m_use_full_screen_triangle(use_full_screen_triangle)
    {
        // Setup shaders
        const Opal::StringUtf8 shader_dir = Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("shaders")).GetValue();
        Opal::StringUtf8 vertex_shader_contents;
        if (m_use_full_screen_triangle)
        {
            vertex_shader_contents = Rndr::File::ReadShader(shader_dir, u8"full-screen-triangle.vert");
        }
        else
        {
            vertex_shader_contents = Rndr::File::ReadShader(shader_dir, u8"full-screen-quad.vert");
        }
        const Opal::StringUtf8 pixel_shader_contents =
            u8R"(
            #version 450
            layout(location = 0) in vec2 uv;
            layout(location = 0) out vec4 frag_color;
            void main()
            {
                frag_color = vec4(uv, 0.0, 1.0);
            }
        )";
        Rndr::ErrorCode err =
            m_vertex_shader.Initialize(m_desc.graphics_context, {.type = Rndr::ShaderType::Vertex, .source = vertex_shader_contents});
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);
        err = m_pixel_shader.Initialize(m_desc.graphics_context, {.type = Rndr::ShaderType::Fragment, .source = pixel_shader_contents});
        RNDR_ASSERT(err == Rndr::ErrorCode::Success);

        // Setup pipeline
        m_pipeline = Rndr::Pipeline(m_desc.graphics_context, {
                                                                 .vertex_shader = &m_vertex_shader,
                                                                 .pixel_shader = &m_pixel_shader,
                                                                 .input_layout = Rndr::InputLayoutBuilder().Build(),
                                                                 .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                                 .depth_stencil = {.is_depth_enabled = false},
                                                             });
        RNDR_ASSERT(m_pipeline.IsValid());
    }

    bool Render() override
    {
        m_desc.graphics_context->BindPipeline(m_pipeline);
        if (m_use_full_screen_triangle)
        {
            m_desc.graphics_context->DrawVertices(Rndr::PrimitiveTopology::Triangle, 3);
        }
        else
        {
            m_desc.graphics_context->DrawVertices(Rndr::PrimitiveTopology::Triangle, 6);
        }
        return true;
    }

private:
    bool m_use_full_screen_triangle = false;

    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_pixel_shader;
    Rndr::Pipeline m_pipeline;
};

class UIRenderer : public Rndr::RendererBase
{
public:
    UIRenderer(const Rndr::RendererBaseDesc& desc, Rndr::Window& window, GameState* game_state,
               const Rndr::FrameBuffer* shadow_frame_buffer)
        : RendererBase(u8"UI Renderer", desc), m_game_state(game_state), m_shadow_frame_buffer(shadow_frame_buffer)
    {
        ImGuiWrapper::Init(window, *desc.graphics_context);
    }

    ~UIRenderer() override { ImGuiWrapper::Destroy(); }

    bool Render() override
    {
        ImGuiWrapper::StartFrame();

        ImGui::Begin("Control", nullptr);
        ImGui::Text("Light parameters", nullptr);
        ImGui::SliderFloat("Proj::Light angle", &m_game_state->light_fov, 15.0f, 170.0f);
        ImGui::SliderFloat("Proj::Light inner angle", &m_game_state->light_inner_angle, 1.0f, 15.0f);
        ImGui::SliderFloat("Proj::Near", &m_game_state->light_near, 0.1f, 5.0f);
        ImGui::SliderFloat("Proj::Far", &m_game_state->light_far, 0.1f, 100.0f);
        ImGui::SliderFloat("Pos::Dist", &m_game_state->light_distance, 0.5f, 100.0f);
        ImGui::SliderFloat("Pos::AngleX", &m_game_state->light_x_angle, -3.15f, +3.15f);
        ImGui::SliderFloat("Pos::AngleY", &m_game_state->light_y_angle, -3.15f, +3.15f);
        ImGui::End();

        ImGuiWrapper::TextureWindow("Color", m_shadow_frame_buffer->GetColorAttachment(0));
        ImGuiWrapper::TextureWindow("Depth", m_shadow_frame_buffer->GetDepthStencilAttachment());

        ImGuiWrapper::EndFrame();
        return true;
    }

private:
    Opal::Ref<GameState> m_game_state;
    Opal::Ref<const Rndr::FrameBuffer> m_shadow_frame_buffer;
};

void Run()
{
    Rndr::Window window({.width = 1600, .height = 1200, .name = "Shadows Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight(), .enable_vsync = false});
    RNDR_ASSERT(swap_chain.IsValid());

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    Opal::Array<Rndr::InputBinding> exit_bindings;
    exit_bindings.PushBack({Rndr::InputPrimitive::Keyboard_Esc, Rndr::InputTrigger::ButtonReleased});
    Rndr::InputSystem::GetCurrentContext().AddAction(
        Rndr::InputAction(u8"Exit"),
        Rndr::InputActionData{.callback = [&window](Rndr::InputPrimitive, Rndr::InputTrigger, float) { window.Close(); },
                              .native_window = window.GetNativeWindowHandle(),
                              .bindings = Opal::Span<Rndr::InputBinding>(exit_bindings)});

    const Rndr::RendererBaseDesc renderer_desc = {.graphics_context = Opal::Ref{graphics_context}, .swap_chain = Opal::Ref{swap_chain}};

    MeshContainer mesh_container(&graphics_context);
    GameState game_state;

    Rndr::FlyCamera fly_camera(&window, &Rndr::InputSystem::GetCurrentContext(),
                               {.start_position = Rndr::Point3f(30.0f, 15.0f, 0.0f),
                                .movement_speed = 100,
                                .rotation_speed = 200,
                                .projection_desc = {.near = 0.5f, .far = 5000.0f}});

    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_black;
    const Opal::ScopePtr<Rndr::RendererBase> clear_renderer =
        Opal::MakeDefaultScoped<Rndr::ClearRenderer>(u8"Clear the screen", renderer_desc, k_clear_color);
    const Opal::ScopePtr<Rndr::RendererBase> shadow_renderer =
        Opal::MakeDefaultScoped<ShadowRenderer>(u8"Render shadows", renderer_desc, &mesh_container, &game_state);
    //    const Opal::ScopePtr<Rndr::RendererBase> post_process_renderer = Opal::MakeDefaultScoped<PostProcessRenderer>(renderer_desc,
    //    true);
    const ShadowRenderer* shadow_renderer_ptr = static_cast<ShadowRenderer*>(shadow_renderer.Get());
    const Opal::ScopePtr<Rndr::RendererBase> scene_renderer =
        Opal::MakeDefaultScoped<SceneRenderer>(u8"Render the scene", renderer_desc, &mesh_container, &game_state,
                                               &shadow_renderer_ptr->GetFrameBuffer()->GetDepthStencilAttachment(), &fly_camera);
    const Opal::ScopePtr<Rndr::RendererBase> ui_renderer =
        Opal::MakeDefaultScoped<UIRenderer>(renderer_desc, window, &game_state, shadow_renderer_ptr->GetFrameBuffer());
    const Opal::ScopePtr<Rndr::RendererBase> present_renderer =
        Opal::MakeDefaultScoped<Rndr::PresentRenderer>(u8"Present the back buffer", renderer_desc);

    Rndr::RendererManager renderer_manager;
    renderer_manager.AddRenderer(clear_renderer.Get());
    renderer_manager.AddRenderer(shadow_renderer.Get());
    renderer_manager.AddRenderer(scene_renderer.Get());
    renderer_manager.AddRenderer(ui_renderer.Get());
    renderer_manager.AddRenderer(present_renderer.Get());

    Rndr::FramesPerSecondCounter fps_counter(0.1f);
    f32 delta_seconds = 0.033f;
    while (!window.IsClosed())
    {
        const f64 start_time = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

        fly_camera.Update(delta_seconds);

        renderer_manager.Render();

        const f64 end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_time - start_time);
    }
}
