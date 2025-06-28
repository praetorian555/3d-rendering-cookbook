#pragma once

#include "vulkan/vulkan.hpp"

#include "opal/container/dynamic-array.h"
#include "opal/container/hash-map.h"
#include "opal/container/string.h"

#include "types.h"
#include "vulkan/vulkan-swap-chain.hpp"

struct VulkanDeviceDesc
{
    VkPhysicalDeviceFeatures features = {};
    Opal::DynamicArray<const char*> extensions;
    VkQueueFlags queue_flags = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;
    Opal::Ref<class VulkanSurface> surface;
};

struct VulkanQueueFamilyIndices
{
    static constexpr u32 k_invalid_index = 0xFFFFFFFF;

    u32 graphics_family = k_invalid_index;
    u32 present_family = k_invalid_index;
    u32 compute_family = k_invalid_index;
    u32 transfer_family = k_invalid_index;

    [[nodiscard]] Opal::DynamicArray<u32> GetValidQueueFamilies() const;
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
    [[nodiscard]] const VulkanQueueFamilyIndices& GetQueueFamilyIndices() const { return m_queue_family_indices; }

    VkCommandBuffer CreateCommandBuffer(u32 queue_family_index) const;
    Opal::DynamicArray<VkCommandBuffer> CreateCommandBuffers(u32 queue_family_index, u32 count) const;

    bool DestroyCommandBuffer(VkCommandBuffer command_buffer, u32 queue_family_index) const;
    bool DestroyCommandBuffers(const Opal::DynamicArray<VkCommandBuffer>& command_buffers, u32 queue_family_index) const;

private:
    VkDevice m_device = VK_NULL_HANDLE;
    Opal::HashMap<u32, VkCommandPool> m_queue_family_index_to_command_pool;
    VulkanPhysicalDevice m_physical_device;
    VulkanDeviceDesc m_desc;
    VulkanQueueFamilyIndices m_queue_family_indices;
};