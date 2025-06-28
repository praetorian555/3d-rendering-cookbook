#include "vulkan/vulkan-device.hpp"

#include "rndr/return-macros.hpp"

#include "types.h"
#include "vulkan-swap-chain.hpp"

Opal::DynamicArray<u32> VulkanQueueFamilyIndices::GetValidQueueFamilies() const
{
    Opal::DynamicArray<u32> valid_queue_families;
    if (graphics_family != k_invalid_index)
    {
        valid_queue_families.PushBack(graphics_family);
    }
    if (present_family != k_invalid_index && present_family != graphics_family)
    {
        valid_queue_families.PushBack(present_family);
    }
    if (transfer_family != k_invalid_index)
    {
        valid_queue_families.PushBack(transfer_family);
    }
    if (compute_family != k_invalid_index)
    {
        valid_queue_families.PushBack(compute_family);
    }
    return valid_queue_families;
}

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

Opal::Expected<u32, VkResult> VulkanPhysicalDevice::GetPresentQueueFamilyIndex(const class VulkanSurface& surface) const
{
    for (u32 i = 0; i < m_queue_family_properties.GetSize(); i++)
    {
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(m_physical_device, i, surface.GetNativeSurface(), &present_support);
        if (present_support == VK_TRUE)
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
    m_queue_family_properties = {};
    return true;
}

// VulkanDevice Implementation ////////////////////////////////////////////////////////////////////////////////////////

VulkanDevice::VulkanDevice(VulkanPhysicalDevice physical_device, const VulkanDeviceDesc& desc)
{
    Init(Opal::Move(physical_device), desc);
}

VulkanDevice::~VulkanDevice()
{
    Destroy();
}

VulkanDevice::VulkanDevice(VulkanDevice&& other) noexcept
    : m_device(other.m_device),
      m_queue_family_index_to_command_pool(Opal::Move(other.m_queue_family_index_to_command_pool)),
      m_physical_device(Opal::Move(other.m_physical_device)),
      m_desc(Opal::Move(other.m_desc)),
      m_queue_family_indices(other.m_queue_family_indices)
{
    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_index_to_command_pool.clear();
    other.m_physical_device = {};
    other.m_desc = {};
    other.m_queue_family_indices = {};
}

VulkanDevice& VulkanDevice::operator=(VulkanDevice&& other) noexcept
{
    Destroy();

    m_device = other.m_device;
    m_queue_family_index_to_command_pool = Opal::Move(other.m_queue_family_index_to_command_pool);
    m_physical_device = Opal::Move(other.m_physical_device);
    m_desc = Opal::Move(other.m_desc);
    m_queue_family_indices = other.m_queue_family_indices;

    other.m_device = VK_NULL_HANDLE;
    other.m_queue_family_index_to_command_pool.clear();
    other.m_physical_device = {};
    other.m_desc = {};
    other.m_queue_family_indices = {};

    return *this;
}

bool VulkanDevice::Init(VulkanPhysicalDevice physical_device, const VulkanDeviceDesc& desc)
{
    RNDR_RETURN_ON_FAIL(physical_device.IsValid(), false, "Physical device is invalid!", RNDR_NOOP);

    constexpr f32 k_queue_priority = 1.0f;
    VulkanQueueFamilyIndices queue_family_indices;
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
        queue_family_indices.graphics_family = queue_family_index.GetValue();
    }

    if (desc.surface.IsValid())
    {
        auto details = desc.surface->GetSwapChainSupportDetails(physical_device);
        auto present_queue_family_index = physical_device.GetPresentQueueFamilyIndex(desc.surface);
        RNDR_RETURN_ON_FAIL(present_queue_family_index.HasValue(), false, "Present queue family index is invalid!", RNDR_NOOP);
        queue_family_indices.present_family = present_queue_family_index.GetValue();
        if (present_queue_family_index.GetValue() != queue_family_index.GetValue())
        {
            VkDeviceQueueCreateInfo queue_create_info{};
            queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_create_info.queueFamilyIndex = present_queue_family_index.GetValue();
            queue_create_info.queueCount = 1;
            queue_create_info.pQueuePriorities = &k_queue_priority;
            queue_create_infos.PushBack(queue_create_info);
        }
    }

    Opal::DynamicArray device_extensions(desc.extensions);
    if (desc.surface.IsValid())
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

    VkResult result = vkCreateDevice(physical_device.GetNativePhysicalDevice(), &create_info, nullptr, &m_device);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to create device!", Destroy());

    auto queue_family_indices_array = queue_family_indices.GetValidQueueFamilies();
    for (u32 index : queue_family_indices_array)
    {
        VkCommandPoolCreateInfo pool_info{};
        pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        pool_info.queueFamilyIndex = queue_family_indices.graphics_family;
        pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        VkCommandPool command_pool;
        result = vkCreateCommandPool(m_device, &pool_info, nullptr, &command_pool);
        RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to create command pool!", Destroy());
        m_queue_family_index_to_command_pool.insert({index, command_pool});
    }

    m_physical_device = Opal::Move(physical_device);
    m_desc = desc;
    m_queue_family_indices = queue_family_indices;
    return true;
}

bool VulkanDevice::Destroy()
{
    for (auto p : m_queue_family_index_to_command_pool)
    {
        vkDestroyCommandPool(m_device, p.second, nullptr);
    }
    m_queue_family_index_to_command_pool.clear();
    if (m_device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(m_device, nullptr);
        m_device = VK_NULL_HANDLE;
    }
    m_physical_device = {};
    m_desc = {};
    return true;
}

VkCommandBuffer VulkanDevice::CreateCommandBuffer(u32 queue_family_index) const
{
    const auto it = m_queue_family_index_to_command_pool.find(queue_family_index);
    RNDR_RETURN_ON_FAIL(it != m_queue_family_index_to_command_pool.end(), VK_NULL_HANDLE, "Queue family index not supported!", RNDR_NOOP);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = it->second;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    const VkResult result = vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, VK_NULL_HANDLE, "Failed to allocate command buffer!", RNDR_NOOP);
    return command_buffer;
}

Opal::DynamicArray<VkCommandBuffer> VulkanDevice::CreateCommandBuffers(u32 queue_family_index, u32 count) const
{
    RNDR_RETURN_ON_FAIL(count > 0, Opal::DynamicArray<VkCommandBuffer>(), "Count must be greater than zero!", RNDR_NOOP);

    Opal::DynamicArray<VkCommandBuffer> command_buffers;
    const auto it = m_queue_family_index_to_command_pool.find(queue_family_index);
    RNDR_RETURN_ON_FAIL(it != m_queue_family_index_to_command_pool.end(), command_buffers, "Queue family index not supported!", RNDR_NOOP);

    command_buffers.Resize(count);
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = it->second;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = count;
    const VkResult result = vkAllocateCommandBuffers(m_device, &alloc_info, command_buffers.GetData());
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, command_buffers, "Failed to allocate command buffers!", command_buffers.Clear());
    return command_buffers;
}

bool VulkanDevice::DestroyCommandBuffer(VkCommandBuffer command_buffer, u32 queue_family_index) const
{
    const auto it = m_queue_family_index_to_command_pool.find(queue_family_index);
    RNDR_RETURN_ON_FAIL(it != m_queue_family_index_to_command_pool.end(), false, "Queue family index not supported!", RNDR_NOOP);
    vkFreeCommandBuffers(m_device, it->second, 1, &command_buffer);
    return true;
}

bool VulkanDevice::DestroyCommandBuffers(const Opal::DynamicArray<VkCommandBuffer>& command_buffers, u32 queue_family_index) const
{
    const auto it = m_queue_family_index_to_command_pool.find(queue_family_index);
    RNDR_RETURN_ON_FAIL(it != m_queue_family_index_to_command_pool.end(), false, "Queue family index not supported!", RNDR_NOOP);
    vkFreeCommandBuffers(m_device, it->second, static_cast<u32>(command_buffers.GetSize()), command_buffers.GetData());
    return true;
}
