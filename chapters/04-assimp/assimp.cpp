#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <assimp/version.h>

#include "opal/container/in-place-array.h"
#include "opal/container/string.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/input-layout-builder.h"
#include "rndr/math.h"
#include "rndr/render-api.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "types.h"

void Run();

/**
 * In this example you will learn how to:
 *     1. Load a mesh from a file using Assimp.
 *     2. Render a mesh using just vertices, with no index buffers.
 *     3. Update uniform buffers per frame.
 *     4. Render wireframes.
 *     5. Use math transformations.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

const char8* const g_shader_code_vertex =
    R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
	uniform int isWireframe;
};
layout (location=0) in vec3 pos;
layout (location=0) out vec3 color;
void main()
{
	gl_Position = MVP * vec4(pos, 1.0);
	color = isWireframe > 0 ? vec3(0.0f) : pos.xyz;
}
)";

const char8* const g_shader_code_fragment =
    R"(
#version 460 core
layout (location=0) in vec3 color;
layout (location=0) out vec4 out_FragColor;
void main()
{
	out_FragColor = vec4(color, 1.0);
};
)";

struct PerFrameData
{
    Rndr::Matrix4x4f mvp;
    int is_wire_frame;
};
constexpr size_t k_per_frame_size = sizeof(PerFrameData);

void Run()
{
    const Opal::StringUtf8 file_path = Opal::Paths::Combine(nullptr, ASSETS_ROOT, "duck.gltf").GetValue();
    const aiScene* scene = aiImportFile(file_path.GetData(), aiProcess_Triangulate);
    if (scene == nullptr || !scene->HasMeshes())
    {
        RNDR_LOG_ERROR("Failed to load mesh from file with error: %s", aiGetErrorString());
        RNDR_ASSERT(false);
        return;
    }
    RNDR_ASSERT(scene->HasMeshes());
    const aiMesh* mesh = scene->mMeshes[0];
    Opal::DynamicArray<Rndr::Point3f> positions;
    for (unsigned int i = 0; i != mesh->mNumFaces; i++)
    {
        const aiFace& face = mesh->mFaces[i];
        Opal::InPlaceArray<size_t, 3> idx{face.mIndices[0], face.mIndices[1], face.mIndices[2]};
        for (int j = 0; j != 3; j++)
        {
            const aiVector3D v = mesh->mVertices[idx[j]];
            positions.PushBack(Rndr::Point3f(v.x, v.y, v.z));  // NOLINT
        }
    }
    aiReleaseImport(scene);

    Rndr::Window window({.width = 800, .height = 600, .name = "Assimp Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    RNDR_ASSERT(swap_chain.IsValid());
    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
    RNDR_ASSERT(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
    RNDR_ASSERT(pixel_shader.IsValid());

    constexpr size_t k_stride = sizeof(Rndr::Point3f);
    const Rndr::Buffer vertex_buffer(graphics_context,
                                     {.type = Rndr::BufferType::Vertex,
                                      .usage = Rndr::Usage::Default,
                                      .size = static_cast<uint32_t>(k_stride * positions.GetSize()),
                                      .stride = k_stride},
                                     Opal::AsBytes(positions));
    RNDR_ASSERT(vertex_buffer.IsValid());
    Rndr::InputLayoutBuilder builder;
    const Rndr::InputLayoutDesc input_layout_desc = builder.AddVertexBuffer(vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                        .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
                                                        .Build();

    const Rndr::Pipeline solid_pipeline(graphics_context, {.vertex_shader = &vertex_shader,
                                                           .pixel_shader = &pixel_shader,
                                                           .input_layout = input_layout_desc,
                                                           .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(solid_pipeline.IsValid());
    const Rndr::Pipeline wireframe_pipeline(
        graphics_context, {.vertex_shader = &vertex_shader,
                           .pixel_shader = &pixel_shader,
                           .input_layout = input_layout_desc,
                           .rasterizer = {.fill_mode = Rndr::FillMode::Wireframe, .depth_bias = -1.0, .slope_scaled_depth_bias = -1.0},
                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(wireframe_pipeline.IsValid());
    Rndr::Buffer per_frame_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    const int32_t vertex_count = static_cast<int32_t>(positions.GetSize());
    while (!window.IsClosed())
    {
        window.ProcessEvents();

        const float ratio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
        const float angle = static_cast<float>(std::fmod(10 * Opal::GetSeconds(), 360.0));
        const Rndr::Matrix4x4f t = Math::Translate(Rndr::Vector3f(0.0f, -0.5f, -1.5f)) *
                                   Math::Rotate(angle, Rndr::Vector3f(0.0f, 1.0f, 0.0f)) * Math::RotateX(-90.0f);
        const Rndr::Matrix4x4f p = Math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        Rndr::Matrix4x4f mvp = p * t;
        mvp = Math::Transpose(mvp);
        PerFrameData per_frame_data = {.mvp = mvp, .is_wire_frame = 0};

        graphics_context.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));

        graphics_context.ClearColor(k_clear_color);
        graphics_context.ClearDepth(1.0f);
        graphics_context.BindSwapChainFrameBuffer(swap_chain);
        graphics_context.BindPipeline(solid_pipeline);
        graphics_context.BindBuffer(per_frame_buffer, 0);
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, vertex_count);

        graphics_context.BindPipeline(wireframe_pipeline);
        per_frame_data.is_wire_frame = 1;
        graphics_context.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));
        graphics_context.DrawVertices(Rndr::PrimitiveTopology::Triangle, vertex_count);

        graphics_context.Present(swap_chain);
    }
}