#include "vulkan/vulkan-device.hpp"

#include "rndr/return-macros.hpp"

#include "types.h"

VulkanPhysicalDevice::VulkanPhysicalDevice(VkPhysicalDevice physical_device)
{
    Init(physical_device);
}

VulkanPhysicalDevice::~VulkanPhysicalDevice()
{
    Destroy();
}

VulkanPhysicalDevice::VulkanPhysicalDevice(VulkanPhysicalDevice&& other) noexcept
    : m_physical_device(other.m_physical_device),
      m_properties(other.m_properties),
      m_features(other.m_features),
      m_memory_properties(other.m_memory_properties),
      m_queue_family_properties(Opal::Move(other.m_queue_family_properties)),
      m_supported_extensions(Opal::Move(other.m_supported_extensions))
{
    other.m_physical_device = VK_NULL_HANDLE;
    other.m_properties = {};
    other.m_features = {};
    other.m_memory_properties = {};
    other.m_queue_family_properties.Clear();
    other.m_supported_extensions.Clear();
}

VulkanPhysicalDevice& VulkanPhysicalDevice::operator=(VulkanPhysicalDevice&& other) noexcept
{
    Destroy();

    m_physical_device = other.m_physical_device;
    m_properties = other.m_properties;
    m_features = other.m_features;
    m_memory_properties = other.m_memory_properties;
    m_queue_family_properties = Opal::Move(other.m_queue_family_properties);
    m_supported_extensions = Opal::Move(other.m_supported_extensions);

    other.m_physical_device = VK_NULL_HANDLE;
    other.m_properties = {};
    other.m_features = {};
    other.m_memory_properties = {};
    other.m_queue_family_properties.Clear();
    other.m_supported_extensions.Clear();

    return *this;
}

bool VulkanPhysicalDevice::Init(VkPhysicalDevice physical_device)
{
    RNDR_RETURN_ON_FAIL(physical_device != VK_NULL_HANDLE, false, "Physical device handle is invalid!", RNDR_NOOP);

    vkGetPhysicalDeviceProperties(physical_device, &m_properties);
    vkGetPhysicalDeviceFeatures(physical_device, &m_features);
    vkGetPhysicalDeviceMemoryProperties(physical_device, &m_memory_properties);

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, nullptr);
    RNDR_RETURN_ON_FAIL(queue_family_count > 0, false, "No queue families found!", RNDR_NOOP);

    m_queue_family_properties.Resize(queue_family_count);
    vkGetPhysicalDeviceQueueFamilyProperties(physical_device, &queue_family_count, m_queue_family_properties.GetData());

    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, nullptr);
    if (extension_count > 0)
    {
        Opal::DynamicArray<VkExtensionProperties> extensions(extension_count);
        vkEnumerateDeviceExtensionProperties(physical_device, nullptr, &extension_count, extensions.GetData());
        for (const VkExtensionProperties& extension : extensions)
        {
            m_supported_extensions.PushBack(extension.extensionName);
        }
    }

    m_physical_device = physical_device;
    return true;
}

Opal::Expected<u32, VkResult> VulkanPhysicalDevice::GetQueueFamilyIndex(VkQueueFlags queue_flags) const
{
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        const VkQueueFamilyProperties& props = m_queue_family_properties[i];
        if ((props.queueFlags & queue_flags) == queue_flags)
        {
            return Opal::Expected<u32, VkResult>(i);
        }
    }

    return Opal::Expected<u32, VkResult>(VK_ERROR_FEATURE_NOT_PRESENT);
}

bool VulkanPhysicalDevice::IsExtensionSupported(const char* extension_name) const
{
    bool is_found = false;
    for (const Opal::StringUtf8& supported_extension : m_supported_extensions)
    {
        if (supported_extension == extension_name)
        {
            is_found = true;
            break;
        }
    }
    RNDR_RETURN_ON_FAIL(is_found, false, "Device extension not supported!", RNDR_NOOP);
    return true;
}

bool VulkanPhysicalDevice::Destroy()
{
    m_physical_device = VK_NULL_HANDLE;
    m_queue_family_properties.Clear();
    m_supported_extensions.Clear();
    return true;
}

VulkanDevice::VulkanDevice(VulkanPhysicalDevice physical_device, const VulkanDeviceDesc& desc)
{
    Init(Opal::Move(physical_device), desc);
}

VulkanDevice::~VulkanDevice()
{
    Destroy();
}

VulkanDevice::VulkanDevice(VulkanDevice&& other) noexcept
    : m_device(other.m_device), m_physical_device(Opal::Move(other.m_physical_device)), m_desc(Opal::Move(other.m_desc))
{
    other.m_device = VK_NULL_HANDLE;
    other.m_physical_device = {};
    other.m_desc = {};
}

VulkanDevice& VulkanDevice::operator=(VulkanDevice&& other) noexcept
{
    Destroy();

    m_device = other.m_device;
    m_physical_device = Opal::Move(other.m_physical_device);
    m_desc = Opal::Move(other.m_desc);

    other.m_device = VK_NULL_HANDLE;
    other.m_physical_device = {};
    other.m_desc = {};

    return *this;
}

bool VulkanDevice::Init(VulkanPhysicalDevice physical_device, const VulkanDeviceDesc& desc)
{
    RNDR_RETURN_ON_FAIL(physical_device.IsValid(), false, "Physical device is invalid!", RNDR_NOOP);

    constexpr f32 k_queue_priority = 1.0f;
    Opal::DynamicArray<VkDeviceQueueCreateInfo> queue_create_infos;
    auto queue_family_index = physical_device.GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    if (((desc.queue_flags & VK_QUEUE_GRAPHICS_BIT) != 0u) && queue_family_index.HasValue())
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_index.GetValue();
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &k_queue_priority;
        queue_create_infos.PushBack(queue_create_info);
    }

    Opal::DynamicArray device_extensions(desc.extensions);
    if (desc.use_swap_chain)
    {
        device_extensions.PushBack(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
    }
    for (const char* extension_name : device_extensions)
    {
        RNDR_RETURN_ON_FAIL(physical_device.IsExtensionSupported(extension_name), false, "Device extension not supported!", RNDR_NOOP);
    }

    VkDeviceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    create_info.pQueueCreateInfos = queue_create_infos.GetData();
    create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.GetSize());
    create_info.pEnabledFeatures = &desc.features;
    create_info.ppEnabledExtensionNames = device_extensions.GetData();
    create_info.enabledExtensionCount = static_cast<u32>(device_extensions.GetSize());

    const VkResult result = vkCreateDevice(physical_device.GetNativePhysicalDevice(), &create_info, nullptr, &m_device);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to create device!", Destroy());

    m_physical_device = Opal::Move(physical_device);
    m_desc = desc;
    return true;
}

bool VulkanDevice::Destroy()
{
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_physical_device = {};
    m_desc = {};
    return true;
}
