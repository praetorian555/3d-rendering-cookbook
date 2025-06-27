#pragma once

#include "vulkan/vulkan.hpp"

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "types.h"

struct VulkanDeviceDesc
{
    VkPhysicalDeviceFeatures features;
    Opal::DynamicArray<const char*> extensions;
    VkQueueFlags queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    bool use_swap_chain = true;
    Opal::Ref<class VulkanSurface> surface;
};



class VulkanPhysicalDevice
{
public:
    VulkanPhysicalDevice() = default;
    explicit VulkanPhysicalDevice(VkPhysicalDevice physical_device);
    ~VulkanPhysicalDevice();

    VulkanPhysicalDevice(const VulkanPhysicalDevice&) = delete;
    VulkanPhysicalDevice& operator=(const VulkanPhysicalDevice&) = delete;
    VulkanPhysicalDevice(VulkanPhysicalDevice&&) noexcept;
    VulkanPhysicalDevice& operator=(VulkanPhysicalDevice&&) noexcept;

    bool Init(VkPhysicalDevice physical_device);
    bool Destroy();

    [[nodiscard]] bool IsValid() const { return m_physical_device != VK_NULL_HANDLE; }

    [[nodiscard]] VkPhysicalDevice GetNativePhysicalDevice() const { return m_physical_device; }
    [[nodiscard]] const VkPhysicalDeviceProperties& GetProperties() const { return m_properties; }
    [[nodiscard]] const VkPhysicalDeviceFeatures& GetFeatures() const { return m_features; }
    [[nodiscard]] const VkPhysicalDeviceMemoryProperties& GetMemoryProperties() const { return m_memory_properties; }
    [[nodiscard]] const Opal::DynamicArray<VkQueueFamilyProperties>& GetQueueFamilyProperties() const { return m_queue_family_properties; }
    [[nodiscard]] const Opal::DynamicArray<Opal::StringUtf8>& GetSupportedExtensions() const { return m_supported_extensions; }
    [[nodiscard]] Opal::Expected<u32, VkResult> GetQueueFamilyIndex(VkQueueFlags queue_flags) const;
    [[nodiscard]] Opal::Expected<u32, VkResult> GetPresentQueueFamilyIndex(const class VulkanSurface& surface) const;

    [[nodiscard]] bool IsExtensionSupported(const char* extension_name) const;

private:
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties m_properties = {};
    VkPhysicalDeviceFeatures m_features = {};
    VkPhysicalDeviceMemoryProperties m_memory_properties = {};
    Opal::DynamicArray<VkQueueFamilyProperties> m_queue_family_properties;
    Opal::DynamicArray<Opal::StringUtf8> m_supported_extensions;
};

class VulkanDevice
{
public:
    VulkanDevice() = default;
    explicit VulkanDevice(VulkanPhysicalDevice physical_device, const VulkanDeviceDesc& desc = {});
    ~VulkanDevice();

    VulkanDevice(const VulkanDevice&) = delete;
    const VulkanDevice& operator=(const VulkanDevice&) = delete;
    VulkanDevice(VulkanDevice&& other) noexcept;
    VulkanDevice& operator=(VulkanDevice&& other) noexcept;

    bool Init(VulkanPhysicalDevice physical_device, const VulkanDeviceDesc& desc = {});
    bool Destroy();

    [[nodiscard]] VkDevice GetNativeDevice() const { return m_device; }
    [[nodiscard]] const VulkanPhysicalDevice& GetPhysicalDevice() const { return m_physical_device; }
    [[nodiscard]] VkPhysicalDevice GetNativePhysicalDevice() const { return m_physical_device.GetNativePhysicalDevice(); }
    [[nodiscard]] const VulkanDeviceDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] VulkanSwapChainSupportDetails GetSwapChainSupportDetails(const VulkanSurface& surface) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    VulkanPhysicalDevice m_physical_device;
    VulkanDeviceDesc m_desc;
};