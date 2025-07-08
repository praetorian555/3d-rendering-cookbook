#pragma once

#include <vulkan/vulkan.hpp>

#include "opal/container/array-view.h"
#include "opal/container/ref.h"

#include "types.h"

struct VulkanBufferDesc
{
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkMemoryPropertyFlags memory_properties;
    Opal::ArrayView<u8> initial_data;
};

class VulkanBuffer
{
public:
    VulkanBuffer() = default;
    explicit VulkanBuffer(const class VulkanDevice& device, const VulkanBufferDesc& desc = {});
    ~VulkanBuffer();

    VulkanBuffer(const VulkanBuffer&) = delete;
    VulkanBuffer& operator=(const VulkanBuffer&) = delete;
    VulkanBuffer(VulkanBuffer&& other) noexcept;
    VulkanBuffer& operator=(VulkanBuffer&& other) noexcept;

    bool Init(const class VulkanDevice& device, const VulkanBufferDesc& desc = {});
    bool Destroy();

    VkBuffer GetNativeBuffer() const { return m_buffer; }
    VkDeviceMemory GetNativeMemory() const { return m_memory; }

private:
    VulkanBufferDesc m_desc;
    VkBuffer m_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_memory = VK_NULL_HANDLE;
    Opal::Ref<const class VulkanDevice> m_device;
};