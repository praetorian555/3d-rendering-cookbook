#pragma once

#include "rndr/core/base.h"

struct ImGuiDesc
{
    bool enable_keyboard_navigation = false;
    bool enable_gamepad_navigation = false;
    bool display_demo_window = false;
};

namespace Rndr
{
class Window;
class GraphicsContext;
}  // namespace Rndr

/**
 * Provides an easy way to configure imgui. This class is a singleton. Use StartFrame() and
 * EndFrame() to start and end a new ImGui frame. Between these two calls you can use ImGui
 * functions as usual.
 */
class ImGuiWrapper
{
public:
    /**
     * Initializes ImGui backend.
     * @param window Window to attach to.
     * @param context Graphics context to attach to.
     * @param desc ImGui properties.
     * @return True if initialization was successful.
     */
    static bool Init(Rndr::Window& window, Rndr::GraphicsContext& context, const ImGuiDesc& desc = ImGuiDesc{});

    /**
     * Shuts down ImGui backend.
     * @return True if shutdown was successful.
     */
    static bool Destroy();

    /**
     * Starts a new ImGui frame.
     */
    static void StartFrame();

    /**
     * Ends the current ImGui frame.
     */
    static void EndFrame();

    /**
     * Returns the ImGui properties.
     * @return ImGui properties.
     */
    [[nodiscard]] static const ImGuiDesc& GetProps();

private:
    static ImGuiWrapper& Get();

    Rndr::Window* m_window = nullptr;
    Rndr::GraphicsContext* m_context = nullptr;
    ImGuiDesc m_desc;

    bool m_frame_started = false;
    bool m_demo_window_opened = true;
};
