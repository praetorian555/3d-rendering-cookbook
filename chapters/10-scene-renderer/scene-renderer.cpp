#include <filesystem>

#include <gli/gli.hpp>

#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/file.h"
#include "rndr/fly-camera.h"
#include "rndr/frames-per-second-counter.h"
#include "rndr/input-layout-builder.h"
#include "rndr/input.h"
#include "rndr/render-api.h"
#include "rndr/renderer-base.h"
#include "rndr/rndr.h"
#include "rndr/trace.h"
#include "rndr/window.h"

#include "cube-map.h"
#include "scene.h"

void Run();

int main()
{
    Rndr::Init({.enable_input_system = true, .enable_cpu_tracer = true});
    Run();
    Rndr::Destroy();
    return 0;
}

struct PerFrameData
{
    Rndr::Matrix4x4f view_projection;
    Rndr::Point3f camera_position_world;
};

struct ModelData
{
    Rndr::Matrix4x4f model_transform;
    Rndr::Matrix4x4f normal_transform;
};

class SceneRenderer : public Rndr::RendererBase
{
public:
    SceneRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc) : Rndr::RendererBase(name, desc)
    {
        using namespace Rndr;

        const Opal::StringUtf8 k_asset_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("Bistro"), OPAL_UTF8("OutExterior")).GetValue();
        const Opal::StringUtf8 k_scene_path = Opal::Paths::Combine(nullptr, k_asset_path, OPAL_UTF8("exterior.rndrscene")).GetValue();
        const Opal::StringUtf8 k_mesh_path = Opal::Paths::Combine(nullptr, k_asset_path, OPAL_UTF8("exterior.rndrmesh")).GetValue();
        const Opal::StringUtf8 k_mat_path = Opal::Paths::Combine(nullptr, k_asset_path, OPAL_UTF8("exterior.rndrmat")).GetValue();
        const bool is_data_loaded = Scene::ReadScene(m_scene_data, k_scene_path, k_mesh_path, k_mat_path, desc.graphics_context);
        if (!is_data_loaded)
        {
            RNDR_HALT("Failed to load mesh data from file!");
            return;
        }

        // Setup shaders
        const Opal::StringUtf8 shader_dir = Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("shaders")).GetValue();
        const Opal::StringUtf8 vertex_shader_code = Rndr::File::ReadShader(shader_dir, OPAL_UTF8("material-pbr.vert"));
        const Opal::StringUtf8 fragment_shader_code = Rndr::File::ReadShader(shader_dir, OPAL_UTF8("material-pbr.frag"));
        m_vertex_shader = Shader(desc.graphics_context, {.type = ShaderType::Vertex, .source = vertex_shader_code});
        RNDR_ASSERT(m_vertex_shader.IsValid());
        m_pixel_shader =
            Shader(desc.graphics_context, {.type = ShaderType::Fragment, .source = fragment_shader_code, .defines = {u8"USE_PBR"}});
        RNDR_ASSERT(m_pixel_shader.IsValid());

        // Setup vertex buffer
        m_vertex_buffer = Rndr::Buffer(desc.graphics_context,
                                       {.type = Rndr::BufferType::ShaderStorage,
                                        .usage = Rndr::Usage::Default,
                                        .size = m_scene_data.mesh_data.vertex_buffer_data.GetSize()},
                                       Opal::AsBytes(m_scene_data.mesh_data.vertex_buffer_data));
        RNDR_ASSERT(m_vertex_buffer.IsValid());

        // Setup index buffer
        m_index_buffer = Buffer(desc.graphics_context,
                                {.type = BufferType::Index,
                                 .usage = Usage::Default,
                                 .size = m_scene_data.mesh_data.index_buffer_data.GetSize(),
                                 .stride = sizeof(uint32_t)},
                                Opal::AsBytes(m_scene_data.mesh_data.index_buffer_data));
        RNDR_ASSERT(m_index_buffer.IsValid());

        // Setup model transforms buffer
        Opal::Array<ModelData> model_transforms_data(m_scene_data.shapes.GetSize());
        for (int i = 0; i < m_scene_data.shapes.GetSize(); i++)
        {
            const MeshDrawData& shape = m_scene_data.shapes[i];
            const Matrix4x4f model_transform = m_scene_data.scene_description.world_transforms[shape.transform_index];
            const Matrix4x4f normal_transform = Math::Transpose(Math::Inverse(model_transform));
            model_transforms_data[i] = {.model_transform = model_transform, .normal_transform = normal_transform};
        }
        m_model_transforms_buffer =
            Buffer(desc.graphics_context, Opal::Span<const ModelData>(model_transforms_data), BufferType::ShaderStorage, Usage::Dynamic);
        RNDR_ASSERT(m_model_transforms_buffer.IsValid());

        m_material_buffer = Buffer(desc.graphics_context, Opal::Span<const MaterialDescription>(m_scene_data.materials),
                                   BufferType::ShaderStorage, Usage::Dynamic);
        RNDR_ASSERT(m_material_buffer.IsValid());

        // Setup buffer that will be updated every frame with camera info
        constexpr size_t k_per_frame_size = sizeof(PerFrameData);
        m_per_frame_buffer = Rndr::Buffer(
            m_desc.graphics_context,
            {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
        RNDR_ASSERT(m_per_frame_buffer.IsValid());

        // Describe what buffers are bound to what slots. No need to describe data layout since we are using vertex pulling.
        const Rndr::InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                            .AddShaderStorage(m_vertex_buffer, 1)
                                                            .AddShaderStorage(m_model_transforms_buffer, 2)
                                                            .AddShaderStorage(m_material_buffer, 3)
                                                            .AddIndexBuffer(m_index_buffer)
                                                            .Build();

        // Setup pipeline object.
        m_pipeline = Pipeline(desc.graphics_context, {.vertex_shader = &m_vertex_shader,
                                                      .pixel_shader = &m_pixel_shader,
                                                      .input_layout = input_layout_desc,
                                                      .rasterizer = {.fill_mode = FillMode::Solid},
                                                      .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_pipeline.IsValid());

        const Opal::StringUtf8 env_map_image_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("piazza_bologni_1k.hdr")).GetValue();
        m_env_map_image = LoadImage(TextureType::CubeMap, env_map_image_path);
        RNDR_ASSERT(m_env_map_image.IsValid());

        const Opal::StringUtf8 irradiance_map_image_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("piazza_bologni_1k_irradience.hdr")).GetValue();
        m_irradiance_map_image = LoadImage(TextureType::CubeMap, irradiance_map_image_path);
        RNDR_ASSERT(m_irradiance_map_image.IsValid());

        const Opal::StringUtf8 brdf_lut_image_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("brdf-lut.ktx")).GetValue();
        m_brdf_lut_image = LoadImage(TextureType::Texture2D, brdf_lut_image_path);

        // Setup draw commands based on the mesh data
        Opal::Array<DrawIndicesData> draw_commands;
        if (!Mesh::GetDrawCommands(draw_commands, m_scene_data.shapes, m_scene_data.mesh_data))
        {
            RNDR_HALT("Failed to get draw commands from mesh data!");
            return;
        }
        const Opal::Span<DrawIndicesData> draw_commands_span(draw_commands);

        // Create a command list
        m_command_list = CommandList(m_desc.graphics_context);
        m_command_list.Bind(*m_desc.swap_chain);
        m_command_list.Bind(m_pipeline);
        m_command_list.BindConstantBuffer(m_per_frame_buffer, 0);
        m_command_list.Bind(m_env_map_image, 5);
        m_command_list.Bind(m_irradiance_map_image, 6);
        m_command_list.Bind(m_brdf_lut_image, 7);
        m_command_list.DrawIndicesMulti(m_pipeline, PrimitiveTopology::Triangle, draw_commands_span);
    }

    bool Render() override
    {
        RNDR_CPU_EVENT_SCOPED("Mesh rendering");

        // Rotate the mesh
        const Rndr::Matrix4x4f t = Math::Scale(0.1f);
        Rndr::Matrix4x4f mvp = m_camera_transform * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.view_projection = mvp, .camera_position_world = m_camera_position};
        m_desc.graphics_context->UpdateBuffer(m_per_frame_buffer, Opal::AsBytes(per_frame_data));

        m_command_list.Submit();

        return true;
    }

    void SetCameraTransform(const Rndr::Matrix4x4f& transform, const Rndr::Point3f& position)
    {
        m_camera_transform = transform;
        m_camera_position = position;
    }

    Rndr::Texture LoadImage(Rndr::TextureType image_type, const Opal::StringUtf8& image_path)
    {
        using namespace Rndr;
        constexpr bool k_flip_vertically = true;

        const bool is_ktx = Opal::Paths::GetExtension(image_path).GetValue() == u8".ktx";

        if (is_ktx)
        {
            gli::texture texture = gli::load_ktx(image_path.GetDataAs<c>());
            const TextureDesc image_desc{.width = texture.extent().x,
                                         .height = texture.extent().y,
                                         .array_size = 1,
                                         .type = image_type,
                                         .pixel_format = Rndr::PixelFormat::R16G16_FLOAT,  // TODO: Fix this!
                                         .use_mips = true};

            const SamplerDesc sampler_desc{.max_anisotropy = 16.0f,
                                           .address_mode_u = ImageAddressMode::Clamp,
                                           .address_mode_v = ImageAddressMode::Clamp,
                                           .address_mode_w = ImageAddressMode::Clamp,
                                           .border_color = Rndr::Colors::k_black};
            const Opal::Span<const u8> texture_data{static_cast<uint8_t*>(texture.data(0, 0, 0)), texture.size()};
            return {m_desc.graphics_context, image_desc, sampler_desc, texture_data};
        }
        if (image_type == TextureType::Texture2D)
        {
            Bitmap bitmap = Rndr::File::ReadEntireImage(image_path, PixelFormat::R8G8B8A8_UNORM, k_flip_vertically);
            RNDR_ASSERT(bitmap.IsValid());
            const TextureDesc image_desc{.width = bitmap.GetWidth(),
                                         .height = bitmap.GetHeight(),
                                         .array_size = 1,
                                         .type = image_type,
                                         .pixel_format = bitmap.GetPixelFormat(),
                                         .use_mips = true};
            const SamplerDesc sampler_desc{.max_anisotropy = 16.0f, .border_color = Rndr::Colors::k_black};
            const Opal::Span<const u8> bitmap_data{bitmap.GetData(), bitmap.GetSize3D()};
            return {m_desc.graphics_context, image_desc, sampler_desc, bitmap_data};
        }
        if (image_type == TextureType::CubeMap)
        {
            const Bitmap equirectangular_bitmap = Rndr::File::ReadEntireImage(image_path, PixelFormat::R32G32B32_FLOAT);
            RNDR_ASSERT(equirectangular_bitmap.IsValid());
            const bool is_equirectangular = equirectangular_bitmap.GetWidth() == 2 * equirectangular_bitmap.GetHeight();
            Bitmap vertical_cross_bitmap;
            if (is_equirectangular)
            {
                if (!CubeMap::ConvertEquirectangularMapToVerticalCross(equirectangular_bitmap, vertical_cross_bitmap))
                {
                    RNDR_HALT("Failed to convert equirectangular map to vertical cross!");
                    return {};
                }
            }
            else
            {
                vertical_cross_bitmap = equirectangular_bitmap;
            }
            Bitmap cube_map_bitmap;
            if (!CubeMap::ConvertVerticalCrossToCubeMapFaces(vertical_cross_bitmap, cube_map_bitmap))
            {
                RNDR_HALT("Failed to convert vertical cross to cube map faces!");
                return {};
            }
            const TextureDesc image_desc{.width = cube_map_bitmap.GetWidth(),
                                         .height = cube_map_bitmap.GetHeight(),
                                         .array_size = cube_map_bitmap.GetDepth(),
                                         .type = image_type,
                                         .pixel_format = cube_map_bitmap.GetPixelFormat(),
                                         .use_mips = true};
            const SamplerDesc sampler_desc{.address_mode_u = ImageAddressMode::Clamp,
                                           .address_mode_v = ImageAddressMode::Clamp,
                                           .address_mode_w = ImageAddressMode::Clamp,
                                           .border_color = Rndr::Colors::k_black};
            const Opal::Span<const u8> bitmap_data{cube_map_bitmap.GetData(), cube_map_bitmap.GetSize3D()};
            return {m_desc.graphics_context, image_desc, sampler_desc, bitmap_data};
        }
        return {};
    }

private:
    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_pixel_shader;

    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Buffer m_model_transforms_buffer;
    Rndr::Buffer m_material_buffer;

    Rndr::Texture m_env_map_image;
    Rndr::Texture m_irradiance_map_image;
    Rndr::Texture m_brdf_lut_image;

    Rndr::Buffer m_per_frame_buffer;
    Rndr::Pipeline m_pipeline;
    Rndr::CommandList m_command_list;

    SceneDrawData m_scene_data;
    Rndr::Matrix4x4f m_camera_transform;
    Rndr::Point3f m_camera_position;
};

void Run()
{
    Rndr::Window window({.width = 1600, .height = 1200, .name = "Scene Renderer Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle(), .enable_bindless_textures = true});
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

    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;
    const Opal::ScopePtr<Rndr::RendererBase> clear_renderer =
        Opal::MakeDefaultScoped<Rndr::ClearRenderer>(u8"Clear the screen", renderer_desc, k_clear_color);
    const Opal::ScopePtr<Rndr::RendererBase> present_renderer =
        Opal::MakeDefaultScoped<Rndr::PresentRenderer>(u8"Present the back buffer", renderer_desc);
    const Opal::ScopePtr<SceneRenderer> mesh_renderer = Opal::MakeDefaultScoped<SceneRenderer>(u8"Render a mesh", renderer_desc);

    Rndr::FlyCamera fly_camera(&window, &Rndr::InputSystem::GetCurrentContext(),
                               {.start_position = Rndr::Point3f(-20.0f, 15.0f, 20.0f),
                                .movement_speed = 100,
                                .rotation_speed = 200,
                                .projection_desc = {.near = 0.5f, .far = 5000.0f}});

    Rndr::RendererManager renderer_manager;
    renderer_manager.AddRenderer(clear_renderer.Get());
    renderer_manager.AddRenderer(mesh_renderer.Get());
    renderer_manager.AddRenderer(present_renderer.Get());

    Rndr::FramesPerSecondCounter fps_counter(0.1f);
    f32 delta_seconds = 0.033f;
    while (!window.IsClosed())
    {
        RNDR_CPU_EVENT_SCOPED("Frame");

        const f64 start_time = Opal::GetSeconds();

        fps_counter.Update(delta_seconds);

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

        fly_camera.Update(delta_seconds);
        mesh_renderer->SetCameraTransform(fly_camera.FromWorldToNDC(), fly_camera.GetPosition());

        renderer_manager.Render();

        const f64 end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(start_time - end_time);
    }
}