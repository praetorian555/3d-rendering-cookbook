#include "rndr/rndr.h"

#include "opal/time.h"

#include "types.h"

void Run();

/**
 * In this example you will learn how to:
 *      1. Update a data buffer per frame.
 *      2. Render a mesh using just vertices, with no index buffers.
 *      3. Render wireframes.
 *      4. Use math transformations.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

const char* const g_shader_code_vertex = R"(
        #version 460 core
        layout(std140, binding = 0) uniform PerFrameData {
          uniform mat4 MVP;
          uniform int isWireframe;
        };
        layout (location=0) out vec3 color;
        const vec3 pos[8] = vec3[8](
          vec3(-1.0,-1.0, 1.0), vec3( 1.0,-1.0, 1.0),
          vec3(1.0, 1.0, 1.0),  vec3(-1.0, 1.0, 1.0),
          vec3(-1.0,-1.0,-1.0), vec3(1.0,-1.0,-1.0),
          vec3( 1.0, 1.0,-1.0), vec3(-1.0, 1.0,-1.0)
        );
        const vec3 col[8] = vec3[8](
          vec3(1.0, 0.0, 0.0), vec3(0.0, 1.0, 0.0),
          vec3(0.0, 0.0, 1.0), vec3(1.0, 1.0, 0.0),
          vec3(1.0, 1.0, 0.0), vec3(0.0, 0.0, 1.0),
          vec3(0.0, 1.0, 0.0), vec3(1.0, 0.0, 0.0)
        );
        const int indices[36] = int[36](
          0, 1, 2, 2, 3, 0, // front
          1, 5, 6, 6, 2, 1, // right
          7, 6, 5, 5, 4, 7, // back
          4, 0, 3, 3, 7, 4, // left
          4, 5, 1, 1, 0, 4, // bottom
          3, 2, 6, 6, 7, 3  // top
        );
        void main() {
          int idx = indices[gl_VertexID];
          gl_Position = MVP * vec4(pos[idx], 1.0);
          color = isWireframe > 0 ? vec3(0.0) : col[idx];
        }
    )";

const char* const g_shader_code_fragment = R"(
        #version 460 core
        layout (location=0) in vec3 color;
        layout (location=0) out vec4 out_FragColor;
        void main() {
          out_FragColor = vec4(color, 1.0);
        };
    )";

struct PerFrameData
{
    Matrix4x4f mvp;
    int is_wire_frame;
};
constexpr size_t k_per_frame_size = sizeof(PerFrameData);

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Cube Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    RNDR_ASSERT(swap_chain.IsValid());
    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = g_shader_code_vertex});
    RNDR_ASSERT(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = g_shader_code_fragment});
    RNDR_ASSERT(pixel_shader.IsValid());
    const Rndr::Pipeline solid_pipeline(graphics_context, {.vertex_shader = &vertex_shader,
                                                           .pixel_shader = &pixel_shader,
                                                           .rasterizer = {.fill_mode = Rndr::FillMode::Solid},
                                                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(solid_pipeline.IsValid());
    const Rndr::Pipeline wireframe_pipeline(
        graphics_context, {.vertex_shader = &vertex_shader,
                           .pixel_shader = &pixel_shader,
                           .rasterizer = {.fill_mode = Rndr::FillMode::Wireframe, .depth_bias = -1.0, .slope_scaled_depth_bias = -1.0},
                           .depth_stencil = {.is_depth_enabled = true}});
    RNDR_ASSERT(wireframe_pipeline.IsValid());
    Rndr::Buffer per_frame_buffer(
        graphics_context,
        {.type = Rndr::BufferType::Constant, .usage = Rndr::Usage::Dynamic, .size = k_per_frame_size, .stride = k_per_frame_size});
    constexpr Vector4f k_clear_color = Rndr::Colors::k_black;

    window.on_resize.Bind([&swap_chain](i32 width, i32 height) { swap_chain.SetSize(width, height); });

    PerFrameData per_frame_data;

    constexpr int32_t k_index_count = 36;
    Rndr::CommandList render_solid_command_list(graphics_context);
    render_solid_command_list.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));
    render_solid_command_list.ClearColor(k_clear_color);
    render_solid_command_list.ClearDepth(1.0f);
    render_solid_command_list.Bind(swap_chain);
    render_solid_command_list.Bind(solid_pipeline);
    render_solid_command_list.BindConstantBuffer(per_frame_buffer, 0);
    render_solid_command_list.DrawVertices(Rndr::PrimitiveTopology::Triangle, k_index_count);

    Rndr::CommandList render_wireframe_command_list(graphics_context);
    render_wireframe_command_list.UpdateBuffer(per_frame_buffer, Opal::AsBytes(per_frame_data));
    render_wireframe_command_list.Bind(wireframe_pipeline);
    render_wireframe_command_list.DrawVertices(Rndr::PrimitiveTopology::Triangle, k_index_count);
    render_wireframe_command_list.Present(swap_chain);

    while (!window.IsClosed())
    {
        window.ProcessEvents();

        const f32 ratio = static_cast<f32>(window.GetWidth()) / static_cast<float>(window.GetHeight());
        const f32 angle = static_cast<f32>(Math::Mod(10 * Opal::GetSeconds(), 360.0));
        const Matrix4x4f t = Math::Translate(Vector3f(0.0f, 0.0f, -3.5f)) * Math::Rotate(angle, Vector3f(1.0f, 1.0f, 1.0f));
        const Matrix4x4f p = Math::Perspective_RH_N1(45.0f, ratio, 0.1f, 1000.0f);
        Matrix4x4f mvp = p * t;
        mvp = Math::Transpose(mvp);

        per_frame_data.mvp = mvp;
        per_frame_data.is_wire_frame = 0;
        render_solid_command_list.Submit();

        per_frame_data.is_wire_frame = 1;
        render_wireframe_command_list.Submit();
    }
}