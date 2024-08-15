#include <filesystem>

#include <gli/gli.hpp>

#include "opal/container/array.h"
#include "opal/container/ref.h"
#include "opal/container/scope-ptr.h"
#include "opal/container/string.h"
#include "opal/paths.h"

#include "rndr/file.h"
#include "rndr/fly-camera.h"
#include "rndr/input-layout-builder.h"
#include "rndr/input.h"
#include "rndr/render-api.h"
#include "rndr/renderer-base.h"
#include "rndr/rndr.h"
#include "rndr/time.h"
#include "rndr/window.h"

#include "cube-map.h"
#include "mesh.h"

#define GLTF_SAMPLE_ASSETS OPAL_UTF8(ASSETS_ROOT) OPAL_UTF8("/gltf-Sample-Assets/Models")

void Run(const Opal::StringUtf8& asset_path);

int main(int argc, char* argv[])
{
    Rndr::Init({.enable_input_system = true});
    const Opal::StringUtf8 model_root =
        Opal::Paths::Combine(nullptr, GLTF_SAMPLE_ASSETS, OPAL_UTF8("DamagedHelmet"), OPAL_UTF8("glTF")).GetValue();
    Run(model_root);
    Rndr::Destroy();
    return 0;
}

struct InstanceData
{
    Rndr::Matrix4x4f model;
    Rndr::Matrix4x4f normal;
};

struct PerFrameData
{
    Rndr::Matrix4x4f view_projection;
    Rndr::Point3f camera_position;
};

class PbrRenderer : public Rndr::RendererBase
{
public:
    PbrRenderer(const Opal::StringUtf8& name, const Rndr::RendererBaseDesc& desc, const Opal::StringUtf8& asset_path)
        : Rndr::RendererBase(name, desc), m_asset_path(asset_path)
    {
        using namespace Rndr;
        const Opal::StringUtf8 shader_dir = Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("shaders")).GetValue();
        const Opal::StringUtf8 vertex_shader_code = Rndr::File::ReadShader(shader_dir, OPAL_UTF8("basic-pbr.vert"));
        const Opal::StringUtf8 fragment_shader_code = Rndr::File::ReadShader(shader_dir, OPAL_UTF8("basic-pbr.frag"));
        m_vertex_shader = Shader(desc.graphics_context, {.type = ShaderType::Vertex, .source = vertex_shader_code});
        RNDR_ASSERT(m_vertex_shader.IsValid());
        m_fragment_shader = Shader(desc.graphics_context, {.type = ShaderType::Fragment, .source = fragment_shader_code});
        RNDR_ASSERT(m_fragment_shader.IsValid());

        const Opal::StringUtf8 mesh_path = Opal::Paths::Combine(nullptr, m_asset_path, OPAL_UTF8("DamagedHelmet.rndrmesh")).GetValue();
        if (!Mesh::ReadData(m_mesh_data, mesh_path))
        {
            RNDR_LOG_ERROR("Failed to load mesh data from file: %s", mesh_path.GetDataAs<c>());
            exit(1);
        }

        m_vertex_buffer = Rndr::Buffer(
            desc.graphics_context,
            {.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Default, .size = m_mesh_data.vertex_buffer_data.GetSize()},
            Opal::AsBytes(m_mesh_data.vertex_buffer_data));
        RNDR_ASSERT(m_vertex_buffer.IsValid());
        m_index_buffer = Rndr::Buffer(desc.graphics_context,
                                      {.type = Rndr::BufferType::Index,
                                       .usage = Rndr::Usage::Default,
                                       .size = m_mesh_data.index_buffer_data.GetSize(),
                                       .stride = sizeof(uint32_t)},
                                      Opal::AsBytes(m_mesh_data.index_buffer_data));
        RNDR_ASSERT(m_index_buffer.IsValid());
        m_instance_buffer = Rndr::Buffer(
            desc.graphics_context, {.type = Rndr::BufferType::ShaderStorage, .usage = Rndr::Usage::Dynamic, .size = sizeof(InstanceData)});
        RNDR_ASSERT(m_instance_buffer.IsValid());
        Rndr::Matrix4x4f model_transform = Math::Translate(Rndr::Vector3f(0.0f, 0.0f, 0.0f)) * Math::RotateX(90.0f) * Math::Scale(1.0f);
        model_transform = Math::Transpose(model_transform);
        InstanceData instance_data = {.model = model_transform, .normal = model_transform};
        m_desc.graphics_context->UpdateBuffer(m_instance_buffer, Opal::AsBytes(instance_data));
        m_per_frame_buffer = Rndr::Buffer(
            desc.graphics_context, {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = sizeof(PerFrameData)});
        RNDR_ASSERT(m_per_frame_buffer.IsValid());

        const Opal::StringUtf8 albedo_image_path = Opal::Paths::Combine(nullptr, m_asset_path, OPAL_UTF8("Default_albedo.jpg")).GetValue();
        m_albedo_image = LoadImage(Rndr::TextureType::Texture2D, albedo_image_path);
        RNDR_ASSERT(m_albedo_image.IsValid());

        const Opal::StringUtf8 normal_image_path = Opal::Paths::Combine(nullptr, m_asset_path, OPAL_UTF8("Default_normal.jpg")).GetValue();
        m_normal_image = LoadImage(Rndr::TextureType::Texture2D, normal_image_path);
        RNDR_ASSERT(m_normal_image.IsValid());

        const Opal::StringUtf8 metallic_roughness_image_path =
            Opal::Paths::Combine(nullptr, m_asset_path, OPAL_UTF8("Default_metalRoughness.jpg")).GetValue();
        m_metallic_roughness_image = LoadImage(Rndr::TextureType::Texture2D, metallic_roughness_image_path);
        RNDR_ASSERT(m_metallic_roughness_image.IsValid());

        const Opal::StringUtf8 ao_image_path = Opal::Paths::Combine(nullptr, m_asset_path, OPAL_UTF8("Default_ao.jpg")).GetValue();
        m_ao_image = LoadImage(Rndr::TextureType::Texture2D, ao_image_path);
        RNDR_ASSERT(m_ao_image.IsValid());

        const Opal::StringUtf8 emissive_image_path =
            Opal::Paths::Combine(nullptr, m_asset_path, OPAL_UTF8("Default_emissive.jpg")).GetValue();
        m_emissive_image = LoadImage(Rndr::TextureType::Texture2D, emissive_image_path);
        RNDR_ASSERT(m_emissive_image.IsValid());

        const Opal::StringUtf8 env_map_image_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("piazza_bologni_1k.hdr")).GetValue();
        m_env_map_image = LoadImage(Rndr::TextureType::CubeMap, env_map_image_path);
        RNDR_ASSERT(m_env_map_image.IsValid());

        const Opal::StringUtf8 irradiance_map_image_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("piazza_bologni_1k_irradience.hdr")).GetValue();
        m_irradiance_map_image = LoadImage(Rndr::TextureType::CubeMap, irradiance_map_image_path);
        RNDR_ASSERT(m_irradiance_map_image.IsValid());

        const Opal::StringUtf8 brdf_lut_image_path =
            Opal::Paths::Combine(nullptr, OPAL_UTF8(ASSETS_ROOT), OPAL_UTF8("brdf-lut.ktx")).GetValue();
        m_brdf_lut_image = LoadImage(Rndr::TextureType::Texture2D, brdf_lut_image_path);

        const Rndr::InputLayoutDesc input_layout_desc = Rndr::InputLayoutBuilder()
                                                            .AddVertexBuffer(m_vertex_buffer, 1, Rndr::DataRepetition::PerVertex)
                                                            .AddVertexBuffer(m_instance_buffer, 2, Rndr::DataRepetition::PerInstance, 1)
                                                            .AddIndexBuffer(m_index_buffer)
                                                            .Build();
        m_pipeline = Rndr::Pipeline(desc.graphics_context, {.vertex_shader = &m_vertex_shader,
                                                            .pixel_shader = &m_fragment_shader,
                                                            .input_layout = input_layout_desc,
                                                            .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                            .depth_stencil = {.is_depth_enabled = true}});
        RNDR_ASSERT(m_pipeline.IsValid());

        m_command_list = Rndr::CommandList(desc.graphics_context);
        m_command_list.BindBuffer(m_per_frame_buffer, 0);
        m_command_list.BindTexture(m_ao_image, 0);
        m_command_list.BindTexture(m_emissive_image, 1);
        m_command_list.BindTexture(m_albedo_image, 2);
        m_command_list.BindTexture(m_metallic_roughness_image, 3);
        m_command_list.BindTexture(m_normal_image, 4);
        m_command_list.BindTexture(m_env_map_image, 5);
        m_command_list.BindTexture(m_irradiance_map_image, 6);
        m_command_list.BindTexture(m_brdf_lut_image, 7);
        m_command_list.BindPipeline(m_pipeline);
        const MeshDescription mesh_desc = m_mesh_data.meshes[0];
        m_command_list.DrawIndices(Rndr::PrimitiveTopology::Triangle, mesh_desc.GetLodIndicesCount(0), 1, mesh_desc.index_offset);
    }

    bool Render() override
    {
        const Rndr::Matrix4x4f view_projection_transform = Math::Transpose(m_camera_transform);
        PerFrameData per_frame_data = {.view_projection = view_projection_transform, .camera_position = m_camera_position};
        m_desc.graphics_context->UpdateBuffer(m_per_frame_buffer, Opal::AsBytes(per_frame_data));

        //        m_desc.graphics_context->Bind(m_vertex_buffer, 1);
        //        m_desc.graphics_context->Bind(m_instance_buffer, 2);
        m_command_list.Submit();
        return true;
    }

    void SetCameraInfo(const Rndr::Matrix4x4f& transform, const Rndr::Point3f& position)
    {
        m_camera_transform = transform;
        m_camera_position = position;
    }

    Rndr::Texture LoadImage(Rndr::TextureType image_type, const Opal::StringUtf8& image_path)
    {
        using namespace Rndr;

        const bool is_ktx = Opal::Paths::GetExtension(image_path).GetValue() == OPAL_UTF8(".ktx");

        if (is_ktx)
        {
            gli::texture texture = gli::load_ktx(image_path.GetDataAs<c>());
            const TextureDesc image_desc{.width = texture.extent().x,
                                         .height = texture.extent().y,
                                         .array_size = 1,
                                         .type = image_type,
                                         .pixel_format = Rndr::PixelFormat::R16G16_FLOAT,  // TODO: Fix this!
                                         .use_mips = true};
            const SamplerDesc sampler_desc = {.max_anisotropy = 16.0f,
                                              .address_mode_u = ImageAddressMode::Clamp,
                                              .address_mode_v = ImageAddressMode::Clamp,
                                              .address_mode_w = ImageAddressMode::Clamp};
            const Opal::Span<const u8> texture_data{static_cast<uint8_t*>(texture.data(0, 0, 0)), texture.size()};
            return {m_desc.graphics_context, image_desc, sampler_desc, texture_data};
        }
        if (image_type == TextureType::Texture2D)
        {
            constexpr bool k_flip_vertically = true;
            Bitmap bitmap = Rndr::File::ReadEntireImage(image_path, PixelFormat::R8G8B8A8_UNORM, k_flip_vertically);
            RNDR_ASSERT(bitmap.IsValid());
            const TextureDesc image_desc{.width = bitmap.GetWidth(),
                                         .height = bitmap.GetHeight(),
                                         .array_size = 1,
                                         .type = image_type,
                                         .pixel_format = bitmap.GetPixelFormat(),
                                         .use_mips = true};
            const SamplerDesc sampler_desc = {.max_anisotropy = 16.0f};
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
            const SamplerDesc sampler_desc = {.address_mode_u = ImageAddressMode::Clamp,
                                              .address_mode_v = ImageAddressMode::Clamp,
                                              .address_mode_w = ImageAddressMode::Clamp};
            const Opal::Span<const u8> bitmap_data{cube_map_bitmap.GetData(), cube_map_bitmap.GetSize3D()};
            return {m_desc.graphics_context, image_desc, sampler_desc, bitmap_data};
        }
        return {};
    }

private:
    Opal::StringUtf8 m_asset_path;

    Rndr::Shader m_vertex_shader;
    Rndr::Shader m_fragment_shader;

    MeshData m_mesh_data;
    Rndr::Buffer m_vertex_buffer;
    Rndr::Buffer m_index_buffer;
    Rndr::Buffer m_instance_buffer;
    Rndr::Buffer m_per_frame_buffer;

    Rndr::Texture m_albedo_image;
    Rndr::Texture m_normal_image;
    Rndr::Texture m_metallic_roughness_image;
    Rndr::Texture m_ao_image;
    Rndr::Texture m_emissive_image;

    Rndr::Texture m_env_map_image;
    Rndr::Texture m_irradiance_map_image;
    Rndr::Texture m_brdf_lut_image;

    Rndr::Pipeline m_pipeline;
    Rndr::CommandList m_command_list;

    Rndr::Matrix4x4f m_camera_transform;
    Rndr::Point3f m_camera_position;
};

void Run(const Opal::StringUtf8& asset_path)
{
    using namespace Rndr;
    Window window({.width = 1920, .height = 1080, .name = "PBR Shading"});
    GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});

    FlyCamera fly_camera(&window, &InputSystem::GetCurrentContext(),
                         {.start_position = Point3f(0.0f, 0.0f, 5.0f),
                          .movement_speed = 10,
                          .rotation_speed = 100,
                          .projection_desc = {.near = 0.05f, .far = 5000.0f}});

    SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    const RendererBaseDesc renderer_desc{.graphics_context = Opal::Ref{graphics_context}, .swap_chain = Opal::Ref{swap_chain}};
    RendererManager renderer_manager;
    const Opal::ScopePtr<RendererBase> clear_renderer =
        Opal::MakeDefaultScoped<ClearRenderer>(OPAL_UTF8("Clear"), renderer_desc, Colors::k_white);
    const Opal::ScopePtr<PbrRenderer> pbr_renderer = Opal::MakeDefaultScoped<PbrRenderer>(OPAL_UTF8("PBR"), renderer_desc, asset_path);
    const Opal::ScopePtr<RendererBase> present_renderer = Opal::MakeDefaultScoped<PresentRenderer>(OPAL_UTF8("Present"), renderer_desc);
    renderer_manager.AddRenderer(clear_renderer.Get());
    renderer_manager.AddRenderer(pbr_renderer.Get());
    renderer_manager.AddRenderer(present_renderer.Get());

    float delta_seconds = 1 / 60.0f;
    while (!window.IsClosed())
    {
        const Timestamp start_time = GetTimestamp();

        window.ProcessEvents();
        InputSystem::ProcessEvents(delta_seconds);

        fly_camera.Update(delta_seconds);
        pbr_renderer->SetCameraInfo(fly_camera.FromWorldToNDC(), fly_camera.GetPosition());

        renderer_manager.Render();

        const Timestamp end_time = GetTimestamp();
        delta_seconds = static_cast<float>(GetDuration(start_time, end_time));
    }
}