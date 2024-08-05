#include "imgui-wrapper.h"

#include "rndr/platform/windows-header.h"

#if RNDR_WINDOWS
#include "backends/imgui_impl_win32.h"
#endif

#include "imgui.h"

#if RNDR_OPENGL
#include "backends/imgui_impl_opengl3.h"
#endif

#include "rndr/render-api.h"
#include "rndr/window.h"
#include "rndr/log.h"

// Forward declare message handler from imgui_impl_win32.cpp
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND handle, UINT msg_code, WPARAM param_w, LPARAM param_l);

ImGuiWrapper& ImGuiWrapper::Get()
{
    static ImGuiWrapper s_wrapper;
    return s_wrapper;
}

bool ImGuiWrapper::Init(Rndr::Window& window, Rndr::GraphicsContext& context, const ImGuiDesc& desc)
{
    ImGuiWrapper& wrapper = Get();

    if (wrapper.m_window != nullptr || wrapper.m_context != nullptr)
    {
        RNDR_LOG_WARNING("ImGuiWrapper::Init: ImGuiWrapper is already initialized.");
        return false;
    }

    wrapper.m_window = &window;
    wrapper.m_context = &context;
    wrapper.m_desc = desc;

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= wrapper.m_desc.enable_keyboard_navigation ? ImGuiConfigFlags_NavEnableKeyboard : ImGuiConfigFlags_None;
    io.ConfigFlags |= wrapper.m_desc.enable_gamepad_navigation ? ImGuiConfigFlags_NavEnableGamepad : ImGuiConfigFlags_None;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    auto lambda_callback = [](Rndr::NativeWindowHandle handle, uint32_t msg_code, uint64_t param_w, int64_t param_l)
    { return ImGui_ImplWin32_WndProcHandler(handle, msg_code, param_w, param_l); };
    window.on_native_event.Bind(lambda_callback);

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_Init(window.GetNativeWindowHandle());
#if RNDR_OPENGL
    ImGui_ImplOpenGL3_Init("#version 330");
#else
    RNDR_ASSERT(false && "Unsupported graphics context!");
#endif
    return true;
}

bool ImGuiWrapper::Destroy()  // NOLINT
{
    ImGuiWrapper& wrapper = Get();

#if RNDR_OPENGL
    ImGui_ImplOpenGL3_Shutdown();
#endif

    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    wrapper.m_window = nullptr;
    wrapper.m_context = nullptr;

    return true;
}

void ImGuiWrapper::StartFrame()
{
    ImGuiWrapper& wrapper = Get();

    if (wrapper.m_frame_started)
    {
        RNDR_LOG_ERROR("ImGuiWrapper::StartFrame: StartFrame already called!");
        return;
    }
    wrapper.m_frame_started = true;

#if RNDR_OPENGL
    ImGui_ImplOpenGL3_NewFrame();
#endif

    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    if (wrapper.m_desc.display_demo_window && wrapper.m_demo_window_opened)
    {
        ImGui::ShowDemoWindow(&wrapper.m_demo_window_opened);
    }
}

void ImGuiWrapper::EndFrame()
{
    ImGuiWrapper& wrapper = Get();

    if (!wrapper.m_frame_started)
    {
        RNDR_LOG_ERROR("ImGuiWrapper::EndFrame: StartFrame was never called!");
        return;
    }
    wrapper.m_frame_started = false;

    ImGui::Render();

#if RNDR_OPENGL
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
#endif
}

const ImGuiDesc& ImGuiWrapper::GetProps()
{
    return Get().m_desc;
}
