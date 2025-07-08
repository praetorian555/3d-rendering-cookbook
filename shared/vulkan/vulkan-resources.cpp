#include "vulkan/vulkan-resources.hpp"

#include "rndr/return-macros.hpp"
#include "vulkan/vulkan-device.hpp"

VulkanBuffer::VulkanBuffer(const VulkanDevice& device, const VulkanBufferDesc& desc)
{
    Init(device, desc);
}

VulkanBuffer::~VulkanBuffer()
{
    Destroy();
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : m_desc(other.m_desc), m_buffer(other.m_buffer), m_memory(other.m_memory), m_device(other.m_device)
{
    m_desc = {};
    other.m_device = nullptr;
    other.m_buffer = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept
{
    Destroy();
    m_desc = other.m_desc;
    m_buffer = other.m_buffer;
    m_memory = other.m_memory;
    m_device = other.m_device;
    other.m_desc = {};
    other.m_buffer = VK_NULL_HANDLE;
    other.m_memory = VK_NULL_HANDLE;
    other.m_device = nullptr;
    return *this;
}

bool VulkanBuffer::Init(const VulkanDevice& device, const VulkanBufferDesc& desc)
{
    m_device = &device;
    m_desc = desc;

    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = desc.size;
    buffer_info.usage = desc.usage;
    // Means that at any given time only one queue family can access this buffer.
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VkResult result = vkCreateBuffer(m_device->GetNativeDevice(), &buffer_info, nullptr, &m_buffer);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to create buffer!", Destroy());

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(m_device->GetNativeDevice(), m_buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex =
        m_device->GetPhysicalDevice().FindMemoryTypeIndex(memory_requirements.memoryTypeBits, desc.memory_properties);
    result = vkAllocateMemory(m_device->GetNativeDevice(), &alloc_info, nullptr, &m_memory);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to allocate buffer memory!", Destroy());

    result = vkBindBufferMemory(m_device->GetNativeDevice(), m_buffer, m_memory, 0);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to bind buffer memory!", Destroy());

    if (!desc.initial_data.IsEmpty())
    {
        void* data = nullptr;
        result = vkMapMemory(m_device->GetNativeDevice(), m_memory, 0, desc.size, 0, &data);
        RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to map buffer memory!", Destroy());
        memcpy(data, desc.initial_data.GetData(), desc.initial_data.GetSize());
        vkUnmapMemory(m_device->GetNativeDevice(), m_memory);
    }

    return true;
}

bool VulkanBuffer::Destroy()
{
    if (m_memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(m_device->GetNativeDevice(), m_memory, nullptr);
        m_memory = VK_NULL_HANDLE;
    }
    if (m_buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(m_device->GetNativeDevice(), m_buffer, nullptr);
        m_buffer = VK_NULL_HANDLE;
    }
    m_device = nullptr;
    return true;
}