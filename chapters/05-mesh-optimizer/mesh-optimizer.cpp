#include "rndr/rndr.h"

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>
#include <meshoptimizer.h>

void Run();

/**
 * In this example you will learn how to:
 *      1. Use mesh optimizer library to optimize the mesh.
 *      2. Use mesh optimizer library to create an LOD mesh.
 *      3. Use geometry shader to draw wireframe.
 *      4. Use RNDR_TRACE functionality to track performance.
 */
int main()
{
    Rndr::Init({.enable_cpu_tracer = true});
    Run();
    Rndr::Destroy();
}

const char* const g_shader_code_vertex = R"(
#version 460 core
layout(std140, binding = 0) uniform PerFrameData
{
	uniform mat4 MVP;
};
layout (location=0) in vec3 pos;
layout (location=0) out vec3 color;
void main()
{
	gl_Position = MVP * vec4(pos, 1.0);
	color = pos.xyz;
}
)";

static const char* const g_shader_code_geometry = R"(
#version 460 core
layout( triangles ) in;
layout( triangle_strip, max_vertices = 3 ) out;
layout (location=0) in vec3 color[];
layout (location=0) out vec3 colors;
layout (location=1) out vec3 barycoords;
void main()
{
	const vec3 bc[3] = vec3[]
	(
		vec3(1.0, 0.0, 0.0),
		vec3(0.0, 1.0, 0.0),
		vec3(0.0, 0.0, 1.0)
	);
	for ( int i = 0; i < 3; i++ )
	{
		gl_Position = gl_in[i].gl_Position;
		colors = color[i];
		barycoords = bc[i];
		EmitVertex();
	}
	EndPrimitive();
}
)";

const char* const g_shader_code_fragment = R"(
#version 460 core
layout (location=0) in vec3 colors;
layout (location=1) in vec3 barycoords;
layout (location=0) out vec4 out_FragColor;
float edgeFactor(float thickness)
{
	vec3 a3 = smoothstep( vec3( 0.0 ), fwidth(barycoords) * thickness, barycoords);
	return min( min( a3.x, a3.y ), a3.z );
}
void main()
{
	out_FragColor = vec4( mix( vec3(0.0), colors, edgeFactor(1.0) ), 1.0 );
};
)";

struct PerFrameData
{
    Rndr::Matrix4x4f mvp;
};
constexpr size_t k_per_frame_size = sizeof(PerFrameData);

bool LoadMeshAndGenerateLOD(const Rndr::String& file_path, Opal::Array<Rndr::Point3f>& vertices, Opal::Array<uint32_t>& indices,
                            Opal::Array<uint32_t>& lod_indices);

void Run()
{
    Opal::Array<Rndr::Point3f> positions;
    Opal::Array<uint32_t> indices;
    Opal::Array<uint32_t> indices_lod;
    [[maybe_unused]] const bool success = LoadMeshAndGenerateLOD(ASSETS_ROOT "duck.gltf", positions, indices, indices_lod);
    if (!success)
    {
        RNDR_LOG_ERROR("Failed to load a mesh!");
        exit(2);
    }

    Rndr::Window window({.width = 1024, .height = 768, .name = "Mesh Optimizer Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    RNDR_ASSERT(swap_chain.IsValid());

    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
    RNDR_ASSERT(vertex_shader.IsValid());
    Rndr::Shader geometry_shader(graphics_context, {.type = Rndr::ShaderType::Geometry, .source = g_shader_code_geometry});
    RNDR_ASSERT(geometry_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
    RNDR_ASSERT(pixel_shader.IsValid());

    const uint32_t size_indices = static_cast<uint32_t>(sizeof(uint32_t) * indices.GetSize());
    const uint32_t size_indices_lod = static_cast<uint32_t>(sizeof(uint32_t) * indices_lod.GetSize());
    const uint32_t size_vertices = static_cast<uint32_t>(sizeof(Rndr::Point3f) * positions.GetSize());
    const uint32_t start_indices = 0;
    const uint32_t start_indices_lod = size_indices;

    const Rndr::Buffer vertex_buffer(graphics_context,
                                     {.type = Rndr::BufferType::Vertex,
                                      .usage = Rndr::Usage::Dynamic,
                                      .size = size_vertices,
                                      .stride = sizeof(Rndr::Point3f),
                                      .offset = 0},
                                     Opal::AsBytes(positions));
    RNDR_ASSERT(vertex_buffer.IsValid());

    Rndr::Buffer index_buffer(graphics_context, {.type = Rndr::BufferType::Index,
                                                 .usage = Rndr::Usage::Dynamic,
                                                 .size = size_indices + size_indices_lod,
                                                 .stride = sizeof(uint32_t),
                                                 .offset = 0});
    RNDR_ASSERT(index_buffer.IsValid());
    graphics_context.UpdateBuffer(index_buffer, Opal::AsBytes(indices), start_indices);
    graphics_context.UpdateBuffer(index_buffer, Opal::AsBytes(indices_lod), start_indices_lod);

    Rndr::InputLayoutBuilder builder;
    const Rndr::InputLayoutDesc input_layout_desc = builder.AddVertexBuffer(vertex_buffer, 0, Rndr::DataRepetition::PerVertex)
                                                        .AppendElement(0, Rndr::PixelFormat::R32G32B32_FLOAT)
                                                        .AddIndexBuffer(index_buffer)
                                                        .Build();

    const Rndr::Pipeline solid_pipeline(graphics_context, {.vertex_shader = &vertex_shader,
                                                           .pixel_shader = &pixel_shader,
                                                           .geometry_shader = &geometry_shader,
                                                           .input_layout = input_layout_desc,
                                                           .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(solid_pipeline.IsValid());
    Rndr::Buffer per_frame_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;

    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    graphics_context.Bind(swap_chain);
    graphics_context.Bind(solid_pipeline);
    graphics_context.Bind(per_frame_buffer, 0);
    while (!window.IsClosed())
    {
        RNDR_TRACE_SCOPED(Main_loop);

        RNDR_TRACE_START(Process_events);
        window.ProcessEvents();
        RNDR_TRACE_END(Process_events);

        const float ratio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
        const float angle = static_cast<float>(std::fmod(10 * Rndr::GetSystemTime(), 360.0));
        const Rndr::Matrix4x4f t1 = Math::Translate(Rndr::Vector3f(-0.5f, -0.5f, -1.5f)) *
                                    Math::Rotate(angle, Rndr::Vector3f(0.0f, 1.0f, 0.0f)) * Math::RotateX(-90.0f);
        const Rndr::Matrix4x4f t2 = Math::Translate(Rndr::Vector3f(0.5f, -0.5f, -1.5f)) *
                                    Math::Rotate(angle, Rndr::Vector3f(0.0f, 1.0f, 0.0f)) * Math::RotateX(-90.0f);
        const Rndr::Matrix4x4f p = Math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        Rndr::Matrix4x4f mvp1 = p * t1;
        mvp1 = Math::Transpose(mvp1);
        Rndr::Matrix4x4f mvp2 = p * t2;
        mvp2 = Math::Transpose(mvp2);

        graphics_context.ClearColor(k_clear_color);
        graphics_context.ClearDepth(1.0f);

        PerFrameData per_frame_data = {.mvp = mvp1};
        graphics_context.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));
        graphics_context.DrawIndices(Rndr::PrimitiveTopology::Triangle, static_cast<int32_t>(indices.GetSize()));

        per_frame_data.mvp = mvp2;
        graphics_context.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));
        graphics_context.DrawIndices(Rndr::PrimitiveTopology::Triangle, static_cast<int32_t>(indices_lod.GetSize()), 1,
                                     static_cast<int32_t>(indices.GetSize()));

        graphics_context.Present(swap_chain);
    }
}

bool LoadMeshAndGenerateLOD(const Rndr::String& file_path, Opal::Array<Rndr::Point3f>& positions, Opal::Array<uint32_t>& indices,
                            Opal::Array<uint32_t>& lod_indices)
{
    const aiScene* scene = aiImportFile(file_path.c_str(), aiProcess_Triangulate);
    if (scene == nullptr || !scene->HasMeshes())
    {
        return false;
    }
    const aiMesh* mesh = scene->mMeshes[0];
    for (unsigned i = 0; i != mesh->mNumVertices; i++)
    {
        const aiVector3D v = mesh->mVertices[i];
        positions.PushBack(Rndr::Point3f(v.x, v.y, v.z));
    }
    for (uint32_t i = 0; i != mesh->mNumFaces; i++)
    {
        for (uint32_t j = 0; j != 3; j++)
        {
            indices.PushBack(mesh->mFaces[i].mIndices[j]);
        }
    }
    aiReleaseImport(scene);

    // Reindex the vertex buffer so that we remove redundant vertices.
    Opal::Array<uint32_t> remap(indices.GetSize());
    const size_t vertex_count =
        meshopt_generateVertexRemap(remap.GetData(), indices.GetData(), indices.GetSize(), positions.GetData(), indices.GetSize(), sizeof(Rndr::Point3f));
    Opal::Array<uint32_t> remapped_indices(indices.GetSize());
    Opal::Array<Rndr::Point3f> remapped_vertices(vertex_count);
    meshopt_remapIndexBuffer(remapped_indices.GetData(), indices.GetData(), indices.GetSize(), remap.GetData());
    meshopt_remapVertexBuffer(remapped_vertices.GetData(), positions.GetData(), positions.GetSize(), sizeof(Rndr::Point3f), remap.GetData());

    // Optimize the vertex cache by organizing the vertex data for same triangles to be close to
    // each other.
    meshopt_optimizeVertexCache(remapped_indices.GetData(), remapped_indices.GetData(), indices.GetSize(), vertex_count);

    // Reduce overdraw to reduce for how many fragments we need to call fragment shader.
    meshopt_optimizeOverdraw(remapped_indices.GetData(), remapped_indices.GetData(), indices.GetSize(),
                             reinterpret_cast<float*>(remapped_vertices.GetData()), vertex_count, sizeof(Rndr::Point3f), 1.05f);

    // Optimize vertex fetches by reordering the vertex buffer.
    meshopt_optimizeVertexFetch(remapped_vertices.GetData(), remapped_indices.GetData(), indices.GetSize(), remapped_vertices.GetData(), vertex_count,
                                sizeof(Rndr::Point3f));

    // Generate lower level LOD.
    constexpr float k_threshold = 0.2f;
    const size_t target_index_count = static_cast<size_t>(static_cast<float>(remapped_indices.GetSize()) * k_threshold);
    constexpr float k_target_error = 1e-2f;
    lod_indices.Resize(remapped_indices.GetSize());
    lod_indices.Resize(meshopt_simplify(lod_indices.GetData(), remapped_indices.GetData(), remapped_indices.GetSize(),
                                        reinterpret_cast<float*>(remapped_vertices.GetData()), vertex_count, sizeof(Rndr::Point3f),
                                        target_index_count, k_target_error, 0, nullptr));

    indices = remapped_indices;
    positions = remapped_vertices;
    return true;
}