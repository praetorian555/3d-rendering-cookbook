#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <imgui.h>

#include "opal/container/string.h"
#include "opal/paths.h"
#include "opal/math/transform.h"

#include "rndr/file.h"
#include "rndr/input-layout-builder.h"
#include "rndr/render-api.h"
#include "rndr/rndr.h"
#include "rndr/time.h"
#include "rndr/window.h"
#include "rndr/projections.h"

#include "imgui-wrapper.h"
#include "types.h"

void Run();

/**
 * In this example you will learn how to:
 *      1. Load a mesh from a file using Assimp.
 *      2. Create a shader from a file.
 *      3. Render a mesh using vertex pulling technique.
 *      4. Control vertical sync using ImGui.
 */
int main() {
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

struct PerFrameData {
    Rndr::Matrix4x4f mvp;
};

struct VertexData {
    Rndr::Point3f pos;
    Rndr::Point2f tc;
};

constexpr size_t k_per_frame_size = sizeof(PerFrameData);

bool LoadMesh(const Opal::StringUtf8 &file_path, Opal::DynamicArray<VertexData> &vertices,
              Opal::DynamicArray<uint32_t> &indices);

void Run() {
    Opal::DynamicArray<VertexData> vertices;
    Opal::DynamicArray<uint32_t> indices;
    const Opal::StringUtf8 file_path = Opal::Paths::Combine(nullptr, ASSETS_ROOT, "duck.gltf").GetValue();
    if (!LoadMesh(file_path, vertices, indices)) {
        return;
    }

    bool vertical_sync = false;

    Rndr::Window window({.width = 800, .height = 600, .name = "Vertex Pulling Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {
                                   .width = window.GetWidth(), .height = window.GetHeight(),
                                   .enable_vsync = vertical_sync
                               });
    RNDR_ASSERT(swap_chain.IsValid());

    // Read shaders from files.
    const Opal::StringUtf8 vertex_shader_path =
            Opal::Paths::Combine(nullptr, ASSETS_ROOT, "shaders", "vertex-pulling-vert.glsl").GetValue();
    const Opal::StringUtf8 pixel_shader_path = Opal::Paths::Combine(nullptr, ASSETS_ROOT, "shaders",
                                                                    "vertex-pulling-frag.glsl").GetValue();
    const Opal::StringUtf8 geometry_shader_path =
            Opal::Paths::Combine(nullptr, ASSETS_ROOT, "shaders", "vertex-pulling-geom.glsl").GetValue();
    const Opal::StringUtf8 vertex_shader_code = Rndr::File::ReadEntireTextFile(vertex_shader_path);
    const Opal::StringUtf8 pixel_shader_code = Rndr::File::ReadEntireTextFile(pixel_shader_path);
    const Opal::StringUtf8 geometry_shader_code = Rndr::File::ReadEntireTextFile(geometry_shader_path);

    // Create shaders.
    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = vertex_shader_code});
    RNDR_ASSERT(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = pixel_shader_code});
    RNDR_ASSERT(pixel_shader.IsValid());
    Rndr::Shader geometry_shader(graphics_context, {
                                     .type = Rndr::ShaderType::Geometry, .source = geometry_shader_code
                                 });
    RNDR_ASSERT(geometry_shader.IsValid());

    // Setup vertex buffer.
    constexpr size_t k_stride = sizeof(VertexData);
    const Rndr::Buffer vertex_buffer(
        graphics_context,
        {
            .type = Rndr::BufferType::ShaderStorage, .size = static_cast<uint32_t>(k_stride * vertices.GetSize()),
            .stride = k_stride
        },
        Opal::AsBytes(vertices));
    RNDR_ASSERT(vertex_buffer.IsValid());

    // Setup index buffer.
    const Rndr::Buffer index_buffer(
        graphics_context,
        {
            .type = Rndr::BufferType::Index, .size = static_cast<uint32_t>(sizeof(uint32_t) * indices.GetSize()),
            .stride = sizeof(uint32_t)
        },
        Opal::AsBytes(indices));
    RNDR_ASSERT(index_buffer.IsValid());

    // Configure input layout.
    Rndr::InputLayoutBuilder builder;
    const Rndr::InputLayoutDesc input_layout_desc =
            builder.AddVertexBuffer(vertex_buffer, 1, Rndr::DataRepetition::PerVertex).AddIndexBuffer(index_buffer).
            Build();

    // Configure the pipeline.
    const Rndr::Pipeline solid_pipeline(graphics_context, {
                                            .vertex_shader = &vertex_shader,
                                            .pixel_shader = &pixel_shader,
                                            .geometry_shader = &geometry_shader,
                                            .input_layout = input_layout_desc,
                                            .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                            .depth_stencil = {.is_depth_enabled = true}
                                        });
    RNDR_ASSERT(solid_pipeline.IsValid());

    // Load mesh albedo texture.
    const Opal::StringUtf8 mesh_image_path = Opal::Paths::Combine(nullptr, ASSETS_ROOT, "duck-base-color.png").
            GetValue();
    Rndr::Bitmap mesh_image = Rndr::File::ReadEntireImage(mesh_image_path, Rndr::PixelFormat::R8G8B8_UNORM_SRGB);
    RNDR_ASSERT(mesh_image.IsValid());
    const Rndr::Texture mesh_albedo(
        graphics_context, {
            .width = mesh_image.GetWidth(), .height = mesh_image.GetHeight(),
            .pixel_format = mesh_image.GetPixelFormat()
        },
        {}, Opal::ArrayView<const u8>(mesh_image.GetData(), mesh_image.GetSize3D()));
    RNDR_ASSERT(mesh_albedo.IsValid());

    // Create a buffer to store per-frame data.
    Rndr::Buffer per_frame_buffer(
        graphics_context,
        {
            .type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size,
            .stride = k_per_frame_size
        });
    constexpr Rndr::Vector4f k_clear_color = Rndr::Colors::k_white;

    // Handle window resizing.
    window.on_resize.Bind([&swap_chain](int32_t width, int32_t height) { swap_chain.SetSize(width, height); });

    // Bind stuff that stay the same across the frames.
    graphics_context.BindSwapChainFrameBuffer(swap_chain);
    graphics_context.BindPipeline(solid_pipeline);
    graphics_context.BindBuffer(per_frame_buffer, 0);
    graphics_context.BindTexture(mesh_albedo, 0);

    ImGuiWrapper::Init(window, graphics_context);

    const int32_t index_count = static_cast<int32_t>(indices.GetSize());
    while (!window.IsClosed()) {
        window.ProcessEvents();

        // Setup transform that rotates the mesh around the Y axis.
        const float ratio = static_cast<float>(window.GetWidth()) / static_cast<float>(window.GetHeight());
        const float angle = static_cast<float>(std::fmod(10 * Rndr::GetSystemTime(), 360.0));
        const Rndr::Matrix4x4f t = Opal::Translate(Rndr::Vector3f(0.0f, -0.5f, -1.5f)) *
                                   Opal::Rotate(angle, Rndr::Vector3f(0.0f, 1.0f, 0.0f)) * Opal::RotateX(-90.0f);
        const Rndr::Matrix4x4f p = Rndr::PerspectiveOpenGL(45.0f, ratio, 0.1f, 1000.0f);
        Rndr::Matrix4x4f mvp = p * t;
        mvp = Opal::Transpose(mvp);
        PerFrameData per_frame_data = {.mvp = mvp};

        // Send transform to the GPU each frame.
        graphics_context.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));

        // Clear the screen and draw the mesh.
        graphics_context.ClearColor(k_clear_color);
        graphics_context.ClearDepth(1.0f);
        graphics_context.DrawIndices(Rndr::PrimitiveTopology::Triangle, index_count);

        ImGuiWrapper::StartFrame();
        const ImVec2 window_size(150, 75);
        ImGui::SetNextWindowSize(window_size, ImGuiCond_Always);
        ImGui::Begin("Info");
        ImGui::Text("FPS: %f", ImGui::GetIO().Framerate);
        if (ImGui::Checkbox("Vertical Sync", &vertical_sync)) {
            swap_chain.SetVerticalSync(vertical_sync);
        }
        ImGui::End();
        ImGuiWrapper::EndFrame();

        graphics_context.Present(swap_chain);
    }
    ImGuiWrapper::Destroy();
}

bool LoadMesh(const Opal::StringUtf8 &file_path, Opal::DynamicArray<VertexData> &vertices,
              Opal::DynamicArray<uint32_t> &indices) {
    const aiScene *scene = aiImportFile(file_path.GetData(), aiProcess_Triangulate);
    if ((scene == nullptr) || !scene->HasMeshes()) {
        RNDR_LOG_ERROR("Unable to load %s", file_path.GetData());
        return false;
    }

    const aiMesh *mesh = scene->mMeshes[0];
    for (unsigned i = 0; i != mesh->mNumVertices; i++) {
        const aiVector3D v = mesh->mVertices[i];
        const aiVector3D t = mesh->mTextureCoords[0][i];
        vertices.PushBack({.pos = Rndr::Point3f(v.x, v.y, v.z), .tc = Rndr::Point2f(t.x, t.y)});
    }
    for (unsigned i = 0; i != mesh->mNumFaces; i++) {
        for (unsigned j = 0; j != 3; j++) {
            indices.PushBack(mesh->mFaces[i].mIndices[j]);
        }
    }
    aiReleaseImport(scene);
    return true;
}
