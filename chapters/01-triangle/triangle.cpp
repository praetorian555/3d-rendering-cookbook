#include "rndr/render-api.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "types.h"

void Run();

/**
 * In this example you will learn how to:
 *      1. How to setup a Rndr library.
 *      2. How to create a window.
 *      3. How to create a graphics context.
 *      4. How to create a swap chain.
 *      5. How to create a shader.
 *      6. How to create a pipeline.
 *      7. How to clear the screen.
 *      8. How to draw a triangle.
 */
int main()
{
    Rndr::Init();
    Run();
    Rndr::Destroy();
}

void Run()
{
    Rndr::Window window({.width = 800, .height = 600, .name = "Triangle Example"});
    Rndr::GraphicsContext graphics_context({.window_handle = window.GetNativeWindowHandle()});
    RNDR_ASSERT(graphics_context.IsValid());
    Rndr::SwapChain swap_chain(graphics_context, {.width = window.GetWidth(), .height = window.GetHeight()});
    RNDR_ASSERT(swap_chain.IsValid());
    Rndr::Shader vertex_shader(graphics_context, {.type = Rndr::ShaderType::Vertex, .source = u8R"(
        #version 460 core
        layout (location=0) out vec3 color;
        const vec2 pos[3] = vec2[3](
          vec2(-0.6, -0.4),
          vec2(0.6, -0.4),
          vec2(0.0, 0.6)
        );
        const vec3 col[3] = vec3[3](
          vec3(1.0, 0.0, 0.0),
          vec3(0.0, 1.0, 0.0),
          vec3(0.0, 0.0, 1.0)
        );
        void main() {
          gl_Position = vec4(pos[gl_VertexID], 0.0, 1.0);
          color = col[gl_VertexID];
        }
        )"});
    RNDR_ASSERT(vertex_shader.IsValid());
    Rndr::Shader pixel_shader(graphics_context, {.type = Rndr::ShaderType::Fragment, .source = u8R"(
        #version 460 core
        layout (location=0) in vec3 color;
        layout (location=0) out vec4 out_FragColor;
        void main() {
          out_FragColor = vec4(color, 1.0);
        };
        )"});
    RNDR_ASSERT(pixel_shader.IsValid());
    const Rndr::Pipeline pipeline(graphics_context, {.vertex_shader = &vertex_shader, .pixel_shader = &pixel_shader});
    RNDR_ASSERT(pipeline.IsValid());
    constexpr Vector4f k_clear_color = Rndr::Colors::k_black;

    window.on_resize.Bind([&swap_chain](i32 width, i32 height) { swap_chain.SetSize(width, height); });

    Rndr::CommandList command_list(graphics_context);
    command_list.BindSwapChainFrameBuffer(swap_chain);
    command_list.BindPipeline(pipeline);
    command_list.ClearColor(k_clear_color);
    command_list.DrawVertices(Rndr::PrimitiveTopology::Triangle, 3);
    command_list.Present(swap_chain);

    while (!window.IsClosed())
    {
        window.ProcessEvents();
        command_list.Submit();
    }
}