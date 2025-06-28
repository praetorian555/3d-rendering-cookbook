#include "vulkan/vulkan-swap-chain.hpp"

#include "rndr/return-macros.hpp"
#include "vulkan-graphics-context.hpp"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.h"

#include <vulkan/vulkan_win32.h>
#endif

#include "vulkan-graphics-context.hpp"

VulkanSurface::VulkanSurface(const class VulkanGraphicsContext& context, Rndr::NativeWindowHandle window_handle)
{
    Init(context, window_handle);
}

VulkanSurface::~VulkanSurface()
{
    Destroy();
}

VulkanSurface::VulkanSurface(VulkanSurface&& other) noexcept : m_surface(other.m_surface), m_context(other.m_context)
{
    other.m_surface = VK_NULL_HANDLE;
    other.m_context = nullptr;
}

VulkanSurface& VulkanSurface::operator=(VulkanSurface&& other) noexcept
{
    Destroy();
    m_surface = other.m_surface;
    m_context = other.m_context;
    other.m_surface = VK_NULL_HANDLE;
    other.m_context = nullptr;
    return *this;
}

bool VulkanSurface::Init(const class VulkanGraphicsContext& context, Rndr::NativeWindowHandle window_handle)
{
#if defined(OPAL_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hwnd = reinterpret_cast<HWND>(window_handle);
    surface_create_info.hinstance = GetModuleHandle(nullptr);
    VkResult result = vkCreateWin32SurfaceKHR(context.GetInstance(), &surface_create_info, nullptr, &m_surface);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to create Vulkan surface!", Destroy());
    m_context = &context;
    return true;
#else
#error "Platform not supported!"
    return false;
#endif
}

bool VulkanSurface::Destroy()
{
    if (m_surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(m_context->GetInstance(), m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
    m_context = nullptr;
    return true;
}

VulkanSwapChainSupportDetails VulkanSurface::GetSwapChainSupportDetails(const class VulkanPhysicalDevice& device) const
{
    VulkanSwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device.GetNativePhysicalDevice(), m_surface, &details.capabilities);

    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device.GetNativePhysicalDevice(), m_surface, &format_count, nullptr);
    if (format_count > 0)
    {
        details.formats.Resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device.GetNativePhysicalDevice(), m_surface, &format_count, details.formats.GetData());
    }

    u32 present_mode_count = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device.GetNativePhysicalDevice(), m_surface, &present_mode_count, nullptr);
    if (present_mode_count != 0)
    {
        details.present_modes.Resize(present_mode_count);
        const VkResult result = vkGetPhysicalDeviceSurfacePresentModesKHR(device.GetNativePhysicalDevice(), m_surface, &present_mode_count,
                                                                    details.present_modes.GetData());
        RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, {}, "Failed to get present modes!", RNDR_NOOP);
    }
    return details;
}