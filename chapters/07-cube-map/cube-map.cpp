
#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <imgui.h>

#include "opal/container/string.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/file.h"
#include "rndr/input-layout-builder.h"
#include "rndr/render-api.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "types.h"

void Run();

/**
 * In this example you will learn how to:
 *      1. Read and save HDR images.
 *      2. How to load shaders with include statements that need to be resolved.
 *      3. How to convert equirectangular maps to cube maps.
 *      4. How to render a cube map.
 *      5. How to do basic reflections.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

struct PerFrameData
{
    Rndr::Matrix4x4f model;
    Rndr::Matrix4x4f mvp;
    Rndr::Vector4f camera_pos;
};
struct VertexData
{
    Rndr::Point3f pos;
    Rndr::Normal3f normal;
    Rndr::Point2f tc;
};
constexpr size_t k_per_frame_size = sizeof(PerFrameData);

bool LoadMesh(const Opal::StringUtf8& file_path, Opal::Array<VertexData>& vertices, Opal::Array<uint32_t>& indices);
Rndr::Bitmap ConvertEquirectangularMapToVerticalCross(const Rndr::Bitmap& input_bitmap);
Rndr::Bitmap ConvertVerticalCrossToCubeMapFaces(const Rndr::Bitmap& in_bitmap);

void Run()
{
    Opal::StringUtf8 assets_root;
    assets_root.Append(reinterpret_cast<const c8*>(ASSETS_ROOT));

    Opal::Array<VertexData> vertices;
    Opal::Array<uint32_t> indices;
    Opal::StringUtf8 file_path = Opal::Paths::Combine(nullptr, assets_root, u8"duck.gltf").GetValue();
    if (!LoadMesh(file_path, vertices, indices))
    {
        return;
    }

    bool vertical_sync = true;

    Rndr::Window window({.width = 1024, .height = 768, .name = "Cube Map Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight(), .enable_vsync = vertical_sync});
    RNDR_ASSERT(swap_chain.IsValid());

    // Read shaders for duck model from files.
    const Opal::StringUtf8 vertex_shader_path = Opal::Paths::Combine(nullptr, u8"shaders", u8"cube-map-duck-vert.glsl").GetValue();
    const Opal::StringUtf8 frag_shader_path = Opal::Paths::Combine(nullptr, u8"shaders", u8"cube-map-duck-frag.glsl").GetValue();

    const Opal::StringUtf8 model_vertex_shader_code = Rndr::File::ReadShader(assets_root, vertex_shader_path);
    const Opal::StringUtf8 model_pixel_shader_code = Rndr::File::ReadShader(assets_root, frag_shader_path);

    // Create shaders for a model.
    Rndr::Shader model_vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = model_vertex_shader_code});
    RNDR_ASSERT(model_vertex_shader.IsValid());
    Rndr::Shader model_pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = model_pixel_shader_code});
    RNDR_ASSERT(model_pixel_shader.IsValid());

    // Setup vertex buffer for the model.
    constexpr size_t k_stride = sizeof(VertexData);
    const Rndr::Buffer model_vertex_buffer(
        graphics_context,
        {.type = Rndr::BufferType::ShaderStorage, .size = static_cast<uint32_t>(k_stride * vertices.GetSize()), .stride = k_stride},
        Opal::AsBytes(vertices));
    RNDR_ASSERT(model_vertex_buffer.IsValid());

    // Setup index buffer.
    const Rndr::Buffer model_index_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Index, .size = static_cast<uint32_t>(sizeof(uint32_t) * indices.GetSize()), .stride = sizeof(uint32_t)},
        Opal::AsBytes(indices));
    RNDR_ASSERT(model_index_buffer.IsValid());

    // Configure input layout.
    Rndr::InputLayoutBuilder builder;
    const Rndr::InputLayoutDesc input_layout_desc =
        builder.AddVertexBuffer(model_vertex_buffer, 1, Rndr::DataRepetition::PerVertex).AddIndexBuffer(model_index_buffer).Build();

    // Configure the pipeline.
    const Rndr::Pipeline model_pipeline(graphics_context, {.vertex_shader = &model_vertex_shader,
                                                           .pixel_shader = &model_pixel_shader,
                                                           .input_layout = input_layout_desc,
                                                           .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(model_pipeline.IsValid());

    // Load model albedo texture.
    const Opal::StringUtf8 model_texture_path = Opal::Paths::Combine(nullptr, assets_root, u8"duck-base-color.png").GetValue();
    Rndr::Bitmap model_image = Rndr::File::ReadEntireImage(model_texture_path, Rndr::PixelFormat::R8G8B8_UNORM);
    RNDR_ASSERT(model_image.IsValid());
    const Rndr::Texture mesh_albedo(graphics_context,
                                    {.width = model_image.GetWidth(),
                                     .height = model_image.GetHeight(),
                                     .array_size = model_image.GetDepth(),
                                     .pixel_format = model_image.GetPixelFormat()},
                                    {}, Opal::Span<const u8>(model_image.GetData(), model_image.GetSize3D()));
    RNDR_ASSERT(mesh_albedo.IsValid());

    // Create a buffer to store per-frame data.
    Rndr::Buffer per_frame_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;

    // Handle window resizing.
    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    // Equirectangular image to vertical cross
    const Opal::StringUtf8 equirectangular_path = Opal::Paths::Combine(nullptr, assets_root, u8"piazza_bologni_1k.hdr").GetValue();
    const Rndr::Bitmap equirectangular_image = Rndr::File::ReadEntireImage(equirectangular_path, Rndr::PixelFormat::R32G32B32_FLOAT);
    RNDR_ASSERT(equirectangular_image.IsValid());
    const Rndr::Bitmap vertical_cross_image = ConvertEquirectangularMapToVerticalCross(equirectangular_image);
    RNDR_ASSERT(vertical_cross_image.IsValid());
    Rndr::File::SaveImage(vertical_cross_image, u8"vertical_cross.hdr");

    // Vertical cross to array of 6 cube map faces in the GPU memory
    Rndr::Bitmap cube_map_bitmap = ConvertVerticalCrossToCubeMapFaces(vertical_cross_image);
    RNDR_ASSERT(cube_map_bitmap.IsValid());
    Rndr::File::SaveImage(cube_map_bitmap, u8"cube_map.hdr");
    const Opal::Span<const u8> cube_map_data{cube_map_bitmap.GetData(), static_cast<size_t>(cube_map_bitmap.GetSize3D())};
    const Rndr::Texture cube_map_image{graphics_context,
                                       {.width = cube_map_bitmap.GetWidth(),
                                        .height = cube_map_bitmap.GetHeight(),
                                        .array_size = cube_map_bitmap.GetDepth(),
                                        .type = Rndr::TextureType::CubeMap,
                                        .pixel_format = cube_map_bitmap.GetPixelFormat()},
                                       {.address_mode_u = Rndr::ImageAddressMode::Clamp,
                                        .address_mode_v = Rndr::ImageAddressMode::Clamp,
                                        .address_mode_w = Rndr::ImageAddressMode::Clamp},
                                       cube_map_data};
    RNDR_ASSERT(cube_map_image.IsValid());

    const Opal::StringUtf8 cube_map_vertex_shader_path = Opal::Paths::Combine(nullptr, u8"shaders", u8"cube-map-vert.glsl").GetValue();
    const Opal::StringUtf8 cube_map_vertex_shader_code = Rndr::File::ReadShader(assets_root, cube_map_vertex_shader_path);
    const Opal::StringUtf8 cube_map_pixel_shader_path = Opal::Paths::Combine(nullptr, u8"shaders", u8"cube-map-frag.glsl").GetValue();
    const Opal::StringUtf8 cube_map_pixel_shader_code = Rndr::File::ReadShader(assets_root, cube_map_pixel_shader_path);
    Rndr::Shader cube_map_vertex_shader{graphics_context, {.type = Rndr::ShaderType::Vertex, .source = cube_map_vertex_shader_code}};
    Rndr::Shader cube_map_pixel_shader{graphics_context, {.type = Rndr::ShaderType::Fragment, .source = cube_map_pixel_shader_code}};
    RNDR_ASSERT(cube_map_vertex_shader.IsValid());
    RNDR_ASSERT(cube_map_pixel_shader.IsValid());

    // Setup pipeline for rendering the cube map.
    const Rndr::Pipeline cube_map_pipeline{graphics_context,
                                           {.vertex_shader = &cube_map_vertex_shader,
                                            .pixel_shader = &cube_map_pixel_shader,
                                            .rasterizer = {.fill_mode = Rndr::FillMode::Solid, .cull_face = Rndr::Face::None},
                                            .depth_stencil = {.is_depth_enabled = true}}};
    RNDR_ASSERT(cube_map_pipeline.IsValid());

    // Bind stuff that stay the same across the frames.
    graphics_context.BindSwapChainFrameBuffer(swap_chain);
    graphics_context.BindBuffer(per_frame_buffer, 0);
    graphics_context.BindTexture(mesh_albedo, 0);
    graphics_context.BindTexture(cube_map_image, 1);

    const int32_t index_count = static_cast<int32_t>(indices.GetSize());
    while (!window.IsClosed())
    {
        window.ProcessEvents();

        // Clear the screen and draw the mesh.
        graphics_context.ClearColor(k_clear_color);
        graphics_context.ClearDepth(1.0f);

        // Setup transform that rotates the model around the Y axis.
        const float ratio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
        const float angle = static_cast<float>(std::fmod(10 * Opal::GetSeconds(), 360.0));
        const Rndr::Matrix4x4f t = Math::Translate(Rndr::Vector3f(0.0f, -0.5f, -1.5f)) *
                                   Math::Rotate(angle, Rndr::Vector3f(0.0f, 1.0f, 0.0f)) * Math::RotateX(-90.0f);
        const Rndr::Matrix4x4f p = Math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        Rndr::Matrix4x4f mvp = p * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.mvp = mvp};

        // Draw the model.
        graphics_context.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));
        graphics_context.BindPipeline(model_pipeline);
        graphics_context.DrawIndices(Rndr::PrimitiveTopology::Triangle, index_count);

        // Draw cube map.
        const Rndr::Matrix4x4f cube_map_model = Math::Scale(2.0f);
        const Rndr::Matrix4x4f cube_map_mvp = Math::Transpose(p * cube_map_model);
        per_frame_data.mvp = cube_map_mvp;
        per_frame_data.model = cube_map_model;
        per_frame_data.camera_pos = Rndr::Vector4f();
        graphics_context.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));
        graphics_context.BindPipeline(cube_map_pipeline);
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, 36);

        graphics_context.Present(swap_chain);
    }
}

bool LoadMesh(const Opal::StringUtf8& file_path, Opal::Array<VertexData>& vertices, Opal::Array<uint32_t>& indices)
{
    const c* file_path_raw = reinterpret_cast<const c*>(file_path.GetData());
    const aiScene* scene = aiImportFile(file_path_raw, aiProcess_Triangulate);
    if ((scene == nullptr) || !scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Unable to load %s", file_path_raw);
        return false;
    }

    const aiMesh* mesh = scene->mMeshes[0];
    for (unsigned i = 0; i != mesh->mNumVertices; i++)
    {
        const aiVector3D v = mesh->mVertices[i];
        const aiVector3D n = mesh->mNormals[i];
        const aiVector3D t = mesh->mTextureCoords[0][i];
        vertices.PushBack({.pos = Rndr::Point3f(v.x, v.y, v.z), .normal = Rndr::Normal3f(n.x, n.y, n.z), .tc = Rndr::Point2f(t.x, t.y)});
    }
    for (unsigned i = 0; i != mesh->mNumFaces; i++)
    {
        for (unsigned j = 0; j != 3; j++)
        {
            indices.PushBack(mesh->mFaces[i].mIndices[j]);
        }
    }
    aiReleaseImport(scene);
    return true;
}

// Represents faces in a resulting image.
enum class CubeMapFace
{
    PositiveX = 0,
    NegativeX,
    PositiveY,
    NegativeY,
    PositiveZ,
    NegativeZ,
    EnumCount
};

Rndr::Vector3f FaceCoordinatesToXYZ(int x_result, int y_result, CubeMapFace face_id, int face_size)
{
    const float inv_face_size = 1.0f / static_cast<float>(face_size);
    const float x = 2.0f * static_cast<float>(x_result) * inv_face_size;
    const float y = 2.0f * static_cast<float>(y_result) * inv_face_size;
    switch (face_id)
    {
        case CubeMapFace::PositiveX:
            return {1.0f - x, 1.0f, 1 - y};
        case CubeMapFace::NegativeX:
            return {x - 1.0f, -1.0f, 1.0f - y};
        case CubeMapFace::PositiveY:
            return {y - 1.0f, x - 1.0f, 1.0f};
        case CubeMapFace::NegativeY:
            return {1.0f - y, x - 1.0f, -1.0f};
        case CubeMapFace::PositiveZ:
            return {-1.0f, x - 1.0f, y - 1.0f};
        case CubeMapFace::NegativeZ:
            return {1.0f, x - 1.0f, 1.0f - y};
        default:
            RNDR_ASSERT(false);
            break;
    }
    return {};
}

Rndr::Bitmap ConvertEquirectangularMapToVerticalCross(const Rndr::Bitmap& input_bitmap)
{
    const int face_size = input_bitmap.GetWidth() / 4;

    const int w = face_size * 3;
    const int h = face_size * 4;

    Rndr::Bitmap result(w, h, 1, input_bitmap.GetPixelFormat());

    // We are trying to get a vertical cross in a coordinate space where we are at origin looking
    // down the negative Z axis, and it's a right-handed coordinate system. The faces are arranged
    // as follows:
    //
    //   +----+----+----+
    //   |    | +Y |    |
    //   | -X | -Z | +X |
    //   |    | -Y |    |
    //   |    | +Z |    |
    //   +----+----+----+
    //
    // To get these planes from the equirectangular map we are using spherical coordinates where the
    // radius is one. The angles are theta and phi. Theta is the angle between the projection of the
    // vector onto the XY plane and the positive X axis. Phi is the angle between the vector and the
    // positive Z axis. The angles are in radians. The coordinate system is then left-handed. We are
    // looking down the positive X axis, the positive Y axis is to the right, and the positive Z
    // axis is up. So in this coordinate system the plane we are looking at is positive X plane
    // where resulting image X coordinate is growing in the direction of positive Y axis, and the
    // resulting image Y coordinate is growing in the direction of negative Z axis.

    constexpr int k_face_count = static_cast<int>(CubeMapFace::EnumCount);
    const Opal::StackArray<Vector2i, k_face_count> k_face_offsets = {
        Vector2i(2 * face_size, face_size), Vector2i(0, face_size),        Vector2i(face_size, 0), Vector2i(face_size, 2 * face_size),
        Vector2i(face_size, 3 * face_size), Vector2i(face_size, face_size)};

    const int clamp_w = input_bitmap.GetWidth() - 1;
    const int clamp_h = input_bitmap.GetHeight() - 1;

    for (int face = 0; face < k_face_count; face++)
    {
        for (int x_result = 0; x_result < face_size; x_result++)
        {
            for (int y_result = 0; y_result < face_size; y_result++)
            {
                const CubeMapFace face_id = static_cast<CubeMapFace>(face);
                const Rndr::Vector3f point = FaceCoordinatesToXYZ(x_result, y_result, face_id, face_size);
                const float plane_distance = Math::Sqrt(point.x * point.x + point.y * point.y);
                const float theta = std::atan2(point.y, point.x);
                const float phi = std::atan2(point.z, plane_distance);
                //	float point source coordinates
                const float face_size_float = static_cast<float>(face_size);
                const float uf = 2.0f * face_size_float * (theta + Math::k_pi_float) / Math::k_pi_float;
                const float vf = 2.0f * face_size_float * (Math::k_pi_float / 2.0f - phi) / Math::k_pi_float;
                // 4-samples for bilinear interpolation
                const int32_t u1 = std::clamp(static_cast<int32_t>(Math::Floor(uf)), 0, clamp_w);
                const int32_t v1 = std::clamp(static_cast<int32_t>(Math::Floor(vf)), 0, clamp_h);
                const int32_t u2 = std::clamp(u1 + 1, 0, clamp_w);
                const int32_t v2 = std::clamp(v1 + 1, 0, clamp_h);
                // fractional part
                const float s = uf - static_cast<float>(u1);
                const float t = vf - static_cast<float>(v1);
                // fetch 4-samples
                const Rndr::Vector4f a = input_bitmap.GetPixel(u1, v1);
                const Rndr::Vector4f b = input_bitmap.GetPixel(u2, v1);
                const Rndr::Vector4f c = input_bitmap.GetPixel(u1, v2);
                const Rndr::Vector4f d = input_bitmap.GetPixel(u2, v2);
                // bilinear interpolation
                const Rndr::Vector4f color = a * (1 - s) * (1 - t) + b * (s) * (1 - t) + c * (1 - s) * t + d * (s) * (t);
                result.SetPixel(x_result + k_face_offsets[face].x, y_result + k_face_offsets[face].y, 0, color);
            }
        };
    }

    return result;
}

Rndr::Bitmap ConvertVerticalCrossToCubeMapFaces(const Rndr::Bitmap& in_bitmap)
{
    const int face_width = in_bitmap.GetWidth() / 3;
    const int face_height = in_bitmap.GetHeight() / 4;

    constexpr int k_face_count = static_cast<int>(CubeMapFace::EnumCount);
    Rndr::Bitmap cube_map_bitmap(face_width, face_height, k_face_count, in_bitmap.GetPixelFormat());

    const uint8_t* src = in_bitmap.GetData();
    const size_t pixel_size = in_bitmap.GetPixelSize();
    uint8_t* dst = cube_map_bitmap.GetData();
    for (int face = 0; face < k_face_count; ++face)
    {
        for (int j = 0; j < face_height; ++j)
        {
            for (int i = 0; i < face_width; ++i)
            {
                int x = 0;
                int y = 0;
                const CubeMapFace face_id = static_cast<CubeMapFace>(face);
                switch (face_id)
                {
                    case CubeMapFace::PositiveX:
                        x = i;
                        y = face_height + j;
                        break;
                    case CubeMapFace::NegativeX:
                        x = 2 * face_width + i;
                        y = 1 * face_height + j;
                        break;
                    case CubeMapFace::PositiveY:
                        x = 2 * face_width - (i + 1);
                        y = 1 * face_height - (j + 1);
                        break;
                    case CubeMapFace::NegativeY:
                        x = 2 * face_width - (i + 1);
                        y = 3 * face_height - (j + 1);
                        break;
                    case CubeMapFace::PositiveZ:
                        x = 2 * face_width - (i + 1);
                        y = in_bitmap.GetHeight() - (j + 1);
                        break;
                    case CubeMapFace::NegativeZ:
                        x = face_width + i;
                        y = face_height + j;
                        break;
                    default:
                        break;
                }

                memcpy(dst, src + (y * in_bitmap.GetWidth() + x) * pixel_size, pixel_size);
                dst += pixel_size;
            }
        }
    }

    return cube_map_bitmap;
}
