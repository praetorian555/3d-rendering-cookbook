#pragma once

#include "vulkan/vulkan.hpp"

#include "opal/container/dynamic-array.h"

#include "rndr/generic-window.hpp"

struct VulkanSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    Opal::DynamicArray<VkSurfaceFormatKHR> formats;
    Opal::DynamicArray<VkPresentModeKHR> present_modes;
};

class VulkanSurface
{
public:
    VulkanSurface() = default;
    explicit VulkanSurface(const class VulkanGraphicsContext& context, Rndr::NativeWindowHandle window_handle);
    ~VulkanSurface();
    VulkanSurface(const VulkanSurface&) = delete;
    VulkanSurface& operator=(const VulkanSurface&) = delete;
    VulkanSurface(VulkanSurface&& other) noexcept;
    VulkanSurface& operator=(VulkanSurface&& other) noexcept;

    bool Init(const class VulkanGraphicsContext& context, Rndr::NativeWindowHandle window_handle);
    bool Destroy();

    [[nodiscard]] bool IsValid() const { return m_surface != VK_NULL_HANDLE; }
    [[nodiscard]] VkSurfaceKHR GetNativeSurface() const { return m_surface; }
    [[nodiscard]] VulkanSwapChainSupportDetails GetSwapChainSupportDetails(const class VulkanPhysicalDevice& device) const;

private:
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    Opal::Ref<const class VulkanGraphicsContext> m_context;
};