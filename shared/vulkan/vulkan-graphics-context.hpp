#pragma once

#include "vulkan/vulkan.hpp"

#include "opal/container/dynamic-array.h"
#include "opal/container/string.h"

#include "types.h"
#include "vulkan/vulkan-device.hpp"

struct VulkanGraphicsContextDesc
{
    bool enable_validation_layers = true;
    Opal::DynamicArray<Opal::StringUtf8> required_instance_extensions;
};

class VulkanGraphicsContext
{
public:
    VulkanGraphicsContext() = default;
    explicit VulkanGraphicsContext(const VulkanGraphicsContextDesc& desc);
    ~VulkanGraphicsContext();

    bool Init(const VulkanGraphicsContextDesc& desc = {});
    bool Destroy();

    [[nodiscard]] bool IsValid() const { return m_instance != VK_NULL_HANDLE; }
    [[nodiscard]] const VulkanGraphicsContextDesc& GetDesc() const { return m_desc; }
    [[nodiscard]] VkInstance GetInstance() const { return m_instance; }

    Opal::DynamicArray<VulkanPhysicalDevice> EnumeratePhysicalDevices() const;

private:
    static Opal::DynamicArray<const char*> GetRequiredInstanceExtensions(const VulkanGraphicsContextDesc& desc);
    static Opal::DynamicArray<VkExtensionProperties> GetSupportedInstanceExtensions();

    VulkanGraphicsContextDesc m_desc;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    Opal::DynamicArray<VulkanPhysicalDevice> m_physical_devices;
};
