#pragma once

#include "vulkan/vulkan.hpp"

#include "opal/container/dynamic-array.h"

#include "rndr/generic-window.hpp"

#include "types.h"

struct VulkanSwapChainSupportDetails
{
    VkSurfaceCapabilitiesKHR capabilities;
    Opal::DynamicArray<VkSurfaceFormatKHR> formats;
    Opal::DynamicArray<VkPresentModeKHR> present_modes;
};

struct VulkanSwapChainDesc
{
    VkFormat pixel_format = VK_FORMAT_B8G8R8A8_SRGB;
    VkColorSpaceKHR color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    u32 width = 0;
    u32 height = 0;
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

class VulkanSwapChain
{
public:
    VulkanSwapChain() = default;
    VulkanSwapChain(const class VulkanDevice& device, const VulkanSurface& surface, const VulkanSwapChainDesc& desc = {});
    ~VulkanSwapChain();
    VulkanSwapChain(const VulkanSwapChain&) = delete;
    VulkanSwapChain& operator=(const VulkanSwapChain&) = delete;
    VulkanSwapChain(VulkanSwapChain&& other) noexcept;
    VulkanSwapChain& operator=(VulkanSwapChain&& other) noexcept;

    bool Init(const VulkanDevice& device, const VulkanSurface& surface, const VulkanSwapChainDesc& desc = {});
    bool Destroy();

    [[nodiscard]] bool IsValid() const { return m_swap_chain != VK_NULL_HANDLE; }
    [[nodiscard]] VkSwapchainKHR GetNativeSwapChain() const { return m_swap_chain; }
    [[nodiscard]] const VulkanSwapChainDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] const VkExtent2D& GetExtent() const { return m_extent; }
    [[nodiscard]] const Opal::DynamicArray<VkImageView>& GetImageViews() const { return m_image_views; }

private:
    VulkanSwapChainDesc m_desc;
    VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
    VkExtent2D m_extent = {};
    Opal::DynamicArray<VkImage> m_images;
    Opal::DynamicArray<VkImageView> m_image_views;
    Opal::Ref<const VulkanDevice> m_device;
};
