#include <optional>

#include "opal/defines.h"

#include "vulkan/vulkan.hpp"
#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.h"
#include "vulkan/vulkan_win32.h"
#endif

#include "opal/container/dynamic-array.h"
#include "opal/container/in-place-array.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/file.h"
#include "rndr/input.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "types.h"

static constexpr i32 k_max_frames_in_flight = 2;

struct Vertex
{
    Rndr::Vector2f pos;
    Rndr::Vector3f color;

    static VkVertexInputBindingDescription GetBindingDescription()
    {
        VkVertexInputBindingDescription binding_description{};
        binding_description.binding = 0;
        binding_description.stride = sizeof(Vertex);
        binding_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return binding_description;
    }

    static Opal::InPlaceArray<VkVertexInputAttributeDescription, 2> GetAttributeDescriptions()
    {
        Opal::InPlaceArray<VkVertexInputAttributeDescription, 2> attribute_descriptions{};

        attribute_descriptions[0].binding = 0;
        attribute_descriptions[0].location = 0;
        attribute_descriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attribute_descriptions[0].offset = offsetof(Vertex, pos);

        attribute_descriptions[1].binding = 0;
        attribute_descriptions[1].location = 1;
        attribute_descriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attribute_descriptions[1].offset = offsetof(Vertex, color);

        return attribute_descriptions;
    }
};

const Opal::DynamicArray<Vertex> vertices = {{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                             {{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
                                             {{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}}};

#define VK_CHECK(expr)                                  \
    do                                                  \
    {                                                   \
        [[maybe_unused]] const VkResult result_ = expr; \
        RNDR_ASSERT(result_ == VK_SUCCESS);             \
    } while (0)

void Run();
int main()
{
    Rndr::Init(Rndr::RndrDesc{.enable_input_system = true});
    Run();
    Rndr::Destroy();
    return 0;
}

struct VulkanRendererDesc
{
    bool enable_validation_layers = false;
    Opal::ArrayView<Opal::StringUtf8> required_instance_extensions;
    Opal::Ref<Rndr::Window> window;
};

struct VulkanRenderer
{
    struct QueueFamilyIndices
    {
        std::optional<u32> graphics_family;
        std::optional<u32> present_family;
    };

    struct SwapChainSupportDetails
    {
        VkSurfaceCapabilitiesKHR capabilities;
        Opal::DynamicArray<VkSurfaceFormatKHR> formats;
        Opal::DynamicArray<VkPresentModeKHR> present_modes;
    };

    VulkanRenderer(VulkanRendererDesc desc = {});
    ~VulkanRenderer();

    void Draw();
    void RecreateSwapChain();

    void OnResize();

    Opal::DynamicArray<const char*> GetRequiredInstanceExtensions();

    static Opal::DynamicArray<VkExtensionProperties> GetSupportedInstanceExtensions();
    static u32 FindMemoryType(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties);

private:
    void CreateInstance();
    void SetupDebugMessenger();
    void CreateSurface();
    void PickPhysicalDevice();
    bool IsDeviceSuitable(const VkPhysicalDevice& device);
    QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device);
    void CreateLogicalDevice();
    bool CheckDeviceExtensionSupport(const VkPhysicalDevice& device);
    SwapChainSupportDetails QuerySwapChainSupport(const VkPhysicalDevice& device);
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const Opal::DynamicArray<VkSurfaceFormatKHR>& available_formats);
    VkPresentModeKHR ChooseSwapPresentMode(const Opal::DynamicArray<VkPresentModeKHR>& available_present_modes);
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
    void CreateSwapChain();
    void CreateImageViews();
    void CreateRenderPass();
    void CreateGraphicsPipeline();
    VkShaderModule CreateShaderModule(const Opal::DynamicArray<u8>& code);
    void CreateFrameBuffers();
    void CreateCommandPool();
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer,
                      VkDeviceMemory& out_buffer_memory);
    void CreateVertexBuffer();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanUpSwapChain();

    void RecordCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);

    void CopyBuffer(VkBuffer source_buffer, VkBuffer dst_buffer, VkDeviceSize size);

private:
    VulkanRendererDesc m_desc;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;
    VkSwapchainKHR m_swap_chain = VK_NULL_HANDLE;
    Opal::DynamicArray<VkImage> m_swap_chain_images;
    VkFormat m_swap_chain_image_format;
    VkExtent2D m_swap_chain_extent;
    Opal::DynamicArray<VkImageView> m_swap_chain_image_views;
    VkViewport m_viewport;
    VkRect2D m_scissor;
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;
    Opal::DynamicArray<VkFramebuffer> m_swap_chain_frame_buffers;
    VkCommandPool m_command_pool = VK_NULL_HANDLE;
    VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertex_buffer_memory = VK_NULL_HANDLE;
    Opal::DynamicArray<VkCommandBuffer> m_command_buffers;
    Opal::DynamicArray<VkSemaphore> m_image_available_semaphores;
    Opal::DynamicArray<VkSemaphore> m_render_finished_semaphores;
    Opal::DynamicArray<VkFence> m_in_flight_fences;

    Opal::DynamicArray<const char*> m_validation_layers = {"VK_LAYER_KHRONOS_validation"};
    Opal::DynamicArray<const char*> m_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    u32 m_current_frame_in_flight = 0;
    bool m_has_frame_buffer_resized = false;
};

void Run()
{
    Rndr::Window window(Rndr::WindowDesc{.width = 800, .height = 600, .name = "Vulkan Triangle Example"});
    const VulkanRendererDesc renderer_desc{.enable_validation_layers = true, .window = Opal::Ref(window)};
    VulkanRenderer renderer(renderer_desc);

    window.on_resize.Bind(
        [&renderer](i32 width, i32 height)
        {
            RNDR_UNUSED(width);
            RNDR_UNUSED(height);
            renderer.OnResize();
        });

    f32 delta_seconds = 1 / 60.0f;
    while (!window.IsClosed())
    {
        const f64 start_time = Opal::GetSeconds();

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

        // Detect if we are minimized
        Rndr::Vector2f window_size = window.GetSize();
        if (window_size.x != 0 && window_size.y != 0)
        {
            renderer.Draw();
        }

        const f64 end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_time - start_time);
    }
}

static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                                    VkDebugUtilsMessageTypeFlagsEXT messageType,
                                                    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData, void* pUserData)
{
    RNDR_UNUSED(messageSeverity);
    RNDR_UNUSED(messageType);
    RNDR_UNUSED(pUserData);
    RNDR_LOG_INFO("Validation Layer: %s", pCallbackData->pMessage);
    return VK_FALSE;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator)
{
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        func(instance, debugMessenger, pAllocator);
    }
}

VulkanRenderer::VulkanRenderer(VulkanRendererDesc desc) : m_desc(Opal::Move(desc))
{
    CreateInstance();
    SetupDebugMessenger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
    CreateSwapChain();
    CreateImageViews();
    CreateRenderPass();
    CreateGraphicsPipeline();
    CreateFrameBuffers();
    CreateCommandPool();
    CreateVertexBuffer();
    CreateCommandBuffers();
    CreateSyncObjects();
}

VulkanRenderer::~VulkanRenderer()
{
    vkDeviceWaitIdle(m_device);
    for (i32 i = 0; i < k_max_frames_in_flight; ++i)
    {
        vkDestroySemaphore(m_device, m_render_finished_semaphores[i], nullptr);
        vkDestroySemaphore(m_device, m_image_available_semaphores[i], nullptr);
        vkDestroyFence(m_device, m_in_flight_fences[i], nullptr);
    }
    vkDestroyBuffer(m_device, m_vertex_buffer, nullptr);
    vkFreeMemory(m_device, m_vertex_buffer_memory, nullptr);
    vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    for (const VkFramebuffer& frame_buffer : m_swap_chain_frame_buffers)
    {
        vkDestroyFramebuffer(m_device, frame_buffer, nullptr);
    }
    vkDestroyPipeline(m_device, m_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device, m_pipeline_layout, nullptr);
    vkDestroyRenderPass(m_device, m_render_pass, nullptr);
    for (const VkImageView& image_view : m_swap_chain_image_views)
    {
        vkDestroyImageView(m_device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
    vkDestroyDevice(m_device, nullptr);
    vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_desc.enable_validation_layers)
    {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
    }
    vkDestroyInstance(m_instance, nullptr);
}

void VulkanRenderer::CreateInstance()
{
    // Check if all the requested instance extensions are supported
    Opal::DynamicArray<const char*> required_extensions = GetRequiredInstanceExtensions();
    const Opal::DynamicArray<VkExtensionProperties> supported_extensions = GetSupportedInstanceExtensions();
    for (const char* required_extension_name : required_extensions)
    {
        bool is_found = false;
        for (const VkExtensionProperties& supported_extension : supported_extensions)
        {
            if (strcmp(required_extension_name, supported_extension.extensionName) == 0)
            {
                is_found = true;
                break;
            }
        }
        if (!is_found)
        {
            RNDR_ASSERT(false);
            return;
        }
    }

    // Check if validation layers are available
    if (m_desc.enable_validation_layers)
    {
        u32 available_layer_count = 0;
        vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);

        Opal::DynamicArray<VkLayerProperties> available_layers;
        available_layers.Resize(available_layer_count);
        vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.GetData());

        for (const char* validation_layer_name : m_validation_layers)
        {
            bool is_found = false;
            for (const VkLayerProperties& available_layer : available_layers)
            {
                if (strcmp(validation_layer_name, available_layer.layerName) == 0)
                {
                    is_found = true;
                    break;
                }
            }
            if (!is_found)
            {
                RNDR_ASSERT(false);
                return;
            }
        }
    }

    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkan Triangle Example";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "RNDR";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<u32>(required_extensions.GetSize());
    create_info.ppEnabledExtensionNames = required_extensions.GetData();
    if (m_desc.enable_validation_layers)
    {
        create_info.enabledLayerCount = static_cast<u32>(m_validation_layers.GetSize());
        create_info.ppEnabledLayerNames = m_validation_layers.GetData();
    }
    else
    {
        create_info.enabledLayerCount = 0;
    }

    VK_CHECK(vkCreateInstance(&create_info, nullptr, &m_instance));
}

Opal::DynamicArray<const char*> VulkanRenderer::GetRequiredInstanceExtensions()
{
    Opal::DynamicArray<const char*> required_extension_names;
    if (m_desc.required_instance_extensions.GetSize() > 0)
    {
        required_extension_names.Resize(m_desc.required_instance_extensions.GetSize());
        for (int i = 0; i < m_desc.required_instance_extensions.GetSize(); ++i)
        {
            required_extension_names[i] = (const char*)m_desc.required_instance_extensions[i].GetData();
        }
    }
    required_extension_names.PushBack("VK_KHR_surface");
#if defined(OPAL_PLATFORM_WINDOWS)
    required_extension_names.PushBack("VK_KHR_win32_surface");
#endif
    if (m_desc.enable_validation_layers)
    {
        required_extension_names.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return required_extension_names;
}

Opal::DynamicArray<VkExtensionProperties> VulkanRenderer::GetSupportedInstanceExtensions()
{
    u32 count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    Opal::DynamicArray<VkExtensionProperties> extensions;
    if (count == 0)
    {
        return extensions;
    }
    extensions.Resize(count);
    vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.GetData());
    return extensions;
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
                                      const VkAllocationCallbacks* pAllocator, VkDebugUtilsMessengerEXT* pDebugMessenger)
{
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr)
    {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    }
    else
    {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanRenderer::SetupDebugMessenger()
{
    if (!m_desc.enable_validation_layers)
    {
        return;
    }

    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                  VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                              VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    create_info.pfnUserCallback = DebugCallback;
    create_info.pUserData = nullptr;

    VK_CHECK(CreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger));
}

void VulkanRenderer::CreateSurface()
{

#if defined(OPAL_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hwnd = m_desc.window->GetNativeWindowHandle();
    surface_create_info.hinstance = GetModuleHandle(nullptr);

    VK_CHECK(vkCreateWin32SurfaceKHR(m_instance, &surface_create_info, nullptr, &m_surface));
#else
#error Surface creation is not supported on this platform!
#endif
}

void VulkanRenderer::PickPhysicalDevice()
{
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    RNDR_ASSERT(device_count > 0);

    Opal::DynamicArray<VkPhysicalDevice> devices(device_count);
    vkEnumeratePhysicalDevices(m_instance, &device_count, devices.GetData());
    for (const VkPhysicalDevice& device : devices)
    {
        if (IsDeviceSuitable(device))
        {
            m_physical_device = device;
            break;
        }
    }
    RNDR_ASSERT(m_physical_device != VK_NULL_HANDLE);
}

bool VulkanRenderer::IsDeviceSuitable(const VkPhysicalDevice& device)
{
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);

    if (properties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU || !features.geometryShader)
    {
        return false;
    }

    const QueueFamilyIndices queue_family_indices = FindQueueFamilies(device);
    const bool are_queue_families_present =
        queue_family_indices.graphics_family.has_value() && queue_family_indices.present_family.has_value();
    if (!are_queue_families_present)
    {
        return false;
    }

    const bool extensions_supported = CheckDeviceExtensionSupport(device);
    if (!extensions_supported)
    {
        return false;
    }

    const SwapChainSupportDetails swap_chain_details = QuerySwapChainSupport(device);
    const bool is_swap_chain_adequate = !swap_chain_details.formats.IsEmpty() && !swap_chain_details.present_modes.IsEmpty();
    return is_swap_chain_adequate;
}

VulkanRenderer::QueueFamilyIndices VulkanRenderer::FindQueueFamilies(const VkPhysicalDevice& device)
{
    QueueFamilyIndices indices;

    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    Opal::DynamicArray<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queue_families.GetData());
    i32 i = 0;
    for (const VkQueueFamilyProperties& queue_family : queue_families)
    {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphics_family = i;
        }
        VkBool32 present_support = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &present_support);
        if (present_support)
        {
            indices.present_family = i;
        }
        i++;
    }

    return indices;
}

void VulkanRenderer::CreateLogicalDevice()
{
    QueueFamilyIndices queue_family_indices = FindQueueFamilies(m_physical_device);

    const f32 queue_priority = 1.0f;
    Opal::DynamicArray<VkDeviceQueueCreateInfo> queue_create_infos;
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.PushBack(queue_create_info);
    }
    {
        VkDeviceQueueCreateInfo queue_create_info{};
        queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_info.queueFamilyIndex = queue_family_indices.present_family.value();
        queue_create_info.queueCount = 1;
        queue_create_info.pQueuePriorities = &queue_priority;
        queue_create_infos.PushBack(queue_create_info);
    }

    const VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.pQueueCreateInfos = queue_create_infos.GetData();
    device_create_info.queueCreateInfoCount = static_cast<u32>(queue_create_infos.GetSize());
    device_create_info.pEnabledFeatures = &device_features;

    device_create_info.enabledExtensionCount = static_cast<u32>(m_device_extensions.GetSize());
    device_create_info.ppEnabledExtensionNames = m_device_extensions.GetData();

    if (m_desc.enable_validation_layers)
    {
        device_create_info.enabledLayerCount = static_cast<u32>(m_validation_layers.GetSize());
        device_create_info.ppEnabledLayerNames = m_validation_layers.GetData();
    }
    else
    {
        device_create_info.enabledLayerCount = 0;
    }

    VK_CHECK(vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device));

    vkGetDeviceQueue(m_device, queue_family_indices.graphics_family.value(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, queue_family_indices.present_family.value(), 0, &m_present_queue);
}

bool VulkanRenderer::CheckDeviceExtensionSupport(const VkPhysicalDevice& device)
{
    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    Opal::DynamicArray<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.GetData());

    const Opal::DynamicArray<const char*> required_extensions = m_device_extensions;
    for (const char* required_extension_name : m_device_extensions)
    {
        bool is_found = false;
        for (const VkExtensionProperties& available_extension : available_extensions)
        {
            if (strcmp(required_extension_name, available_extension.extensionName) == 0)
            {
                is_found = true;
                break;
            }
        }
        if (!is_found)
        {
            return false;
        }
    }
    return true;
}

VulkanRenderer::SwapChainSupportDetails VulkanRenderer::QuerySwapChainSupport(const VkPhysicalDevice& device)
{
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities);

    u32 format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, nullptr);
    if (format_count > 0)
    {
        details.formats.Resize(format_count);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &format_count, details.formats.GetData());
    }

    u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount != 0)
    {
        details.present_modes.Resize(presentModeCount);
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.present_modes.GetData()));
    }

    return details;
}

VkSurfaceFormatKHR VulkanRenderer::ChooseSwapSurfaceFormat(const Opal::DynamicArray<VkSurfaceFormatKHR>& available_formats)
{
    RNDR_ASSERT(available_formats.GetSize() > 0);
    for (const VkSurfaceFormatKHR& format : available_formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }
    return available_formats[0];
}

VkPresentModeKHR VulkanRenderer::ChooseSwapPresentMode(const Opal::DynamicArray<VkPresentModeKHR>& available_present_modes)
{
    for (const VkPresentModeKHR& present_mode : available_present_modes)
    {
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
        {
            return present_mode;
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanRenderer::ChooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities)
{
    if (capabilities.currentExtent.width != UINT32_MAX)
    {
        return capabilities.currentExtent;
    }

    const Rndr::Vector2f size = m_desc.window->GetSize();
    VkExtent2D actual_extent = {static_cast<u32>(size.x), static_cast<u32>(size.y)};
    actual_extent.width = Math::Clamp(actual_extent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actual_extent.height = Math::Clamp(actual_extent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actual_extent;
}

void VulkanRenderer::CreateSwapChain()
{
    const SwapChainSupportDetails swap_chain_support = QuerySwapChainSupport(m_physical_device);
    const VkSurfaceFormatKHR surface_format = ChooseSwapSurfaceFormat(swap_chain_support.formats);
    const VkPresentModeKHR present_mode = ChooseSwapPresentMode(swap_chain_support.present_modes);
    const VkExtent2D extent = ChooseSwapExtent(swap_chain_support.capabilities);

    u32 image_count = swap_chain_support.capabilities.minImageCount + 1;
    if (swap_chain_support.capabilities.maxImageCount > 0 && image_count > swap_chain_support.capabilities.maxImageCount)
    {
        image_count = swap_chain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = m_surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = FindQueueFamilies(m_physical_device);
    const u32 queue_family_indices[] = {indices.graphics_family.value(), indices.present_family.value()};
    if (indices.graphics_family != indices.present_family)
    {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    }
    else
    {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = nullptr;
    }

    create_info.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;  // swap_chain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    // If set to VK_TRUE it means that we don't care about the color of the pixels if they are occluded by other window.
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(m_device, &create_info, nullptr, &m_swap_chain));

    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, nullptr));
    m_swap_chain_images.Resize(image_count);
    VK_CHECK(vkGetSwapchainImagesKHR(m_device, m_swap_chain, &image_count, m_swap_chain_images.GetData()));

    m_swap_chain_image_format = surface_format.format;
    m_swap_chain_extent = extent;
}

void VulkanRenderer::CreateImageViews()
{
    m_swap_chain_image_views.Resize(m_swap_chain_images.GetSize());
    for (u32 i = 0; i < m_swap_chain_image_views.GetSize(); ++i)
    {
        VkImageViewCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        create_info.image = m_swap_chain_images[i];
        create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        create_info.format = m_swap_chain_image_format;
        create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        create_info.subresourceRange.baseMipLevel = 0;
        create_info.subresourceRange.levelCount = 1;
        create_info.subresourceRange.baseArrayLayer = 0;
        create_info.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(m_device, &create_info, nullptr, &m_swap_chain_image_views[i]));
    }
}

void VulkanRenderer::CreateRenderPass()
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = m_swap_chain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_info.attachmentCount = 1;
    render_pass_info.pAttachments = &color_attachment;
    render_pass_info.subpassCount = 1;
    render_pass_info.pSubpasses = &subpass;
    render_pass_info.dependencyCount = 1;
    render_pass_info.pDependencies = &dependency;

    VK_CHECK(vkCreateRenderPass(m_device, &render_pass_info, nullptr, &m_render_pass));
}

void VulkanRenderer::CreateGraphicsPipeline()
{
    const Opal::StringUtf8 vertex_shader_path = Opal::Paths::Combine(nullptr, ASSETS_ROOT, "vk-triangle", "triangle-vert.spv").GetValue();
    const Opal::StringUtf8 fragment_shader_path = Opal::Paths::Combine(nullptr, ASSETS_ROOT, "vk-triangle", "triangle-frag.spv").GetValue();
    const Opal::DynamicArray<u8> vertex_shader_contents = Rndr::File::ReadEntireFile(vertex_shader_path);
    const Opal::DynamicArray<u8> fragment_shader_contents = Rndr::File::ReadEntireFile(fragment_shader_path);

    VkShaderModule vertex_shader_module = CreateShaderModule(vertex_shader_contents);
    VkShaderModule fragment_shader_module = CreateShaderModule(fragment_shader_contents);

    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vertex_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = fragment_shader_module;
    frag_shader_stage_info.pName = "main";

    const VkPipelineShaderStageCreateInfo shader_stages[] = {vert_shader_stage_info, frag_shader_stage_info};

    Opal::DynamicArray<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = static_cast<u32>(dynamic_states.GetSize());
    dynamic_state_info.pDynamicStates = dynamic_states.GetData();

    VkVertexInputBindingDescription binding_description = Vertex::GetBindingDescription();
    Opal::InPlaceArray<VkVertexInputAttributeDescription, 2> attribute_descriptions = Vertex::GetAttributeDescriptions();

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_description;
    vertex_input_info.vertexAttributeDescriptionCount = static_cast<u32>(attribute_descriptions.GetSize());
    vertex_input_info.pVertexAttributeDescriptions = attribute_descriptions.GetData();

    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly.primitiveRestartEnable = VK_FALSE;

    m_viewport.x = 0.0f;
    m_viewport.y = 0.0f;
    m_viewport.width = static_cast<f32>(m_swap_chain_extent.width);
    m_viewport.height = static_cast<f32>(m_swap_chain_extent.height);
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;

    m_scissor.offset = {0, 0};
    m_scissor.extent = m_swap_chain_extent;

    VkPipelineViewportStateCreateInfo viewport_state{};
    viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state.viewportCount = 1;
    viewport_state.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable = VK_FALSE;
    rasterizer.rasterizerDiscardEnable = VK_FALSE;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer.depthBiasEnable = VK_FALSE;
    rasterizer.depthBiasConstantFactor = 0.0f;
    rasterizer.depthBiasClamp = 0.0f;
    rasterizer.depthBiasSlopeFactor = 0.0f;

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f;
    multisampling.pSampleMask = nullptr;
    multisampling.alphaToCoverageEnable = VK_FALSE;
    multisampling.alphaToOneEnable = VK_FALSE;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;
    color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
    color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
    color_blend_attachment.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
    color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
    color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
    color_blend_attachment.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

    VkPipelineColorBlendStateCreateInfo color_blending{};
    color_blending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blending.logicOpEnable = VK_FALSE;
    color_blending.logicOp = VK_LOGIC_OP_COPY;  // Optional
    color_blending.attachmentCount = 1;
    color_blending.pAttachments = &color_blend_attachment;
    color_blending.blendConstants[0] = 0.0f;  // Optional
    color_blending.blendConstants[1] = 0.0f;  // Optional
    color_blending.blendConstants[2] = 0.0f;  // Optional
    color_blending.blendConstants[3] = 0.0f;  // Optional

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pSetLayouts = nullptr;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(m_device, &pipeline_layout_info, nullptr, &m_pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = 2;
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly;
    pipeline_info.pViewportState = &viewport_state;
    pipeline_info.pRasterizationState = &rasterizer;
    pipeline_info.pMultisampleState = &multisampling;
    pipeline_info.pDepthStencilState = nullptr;
    pipeline_info.pColorBlendState = &color_blending;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = m_pipeline_layout;
    pipeline_info.renderPass = m_render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE;
    pipeline_info.basePipelineIndex = -1;

    VK_CHECK(vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline));

    vkDestroyShaderModule(m_device, vertex_shader_module, nullptr);
    vkDestroyShaderModule(m_device, fragment_shader_module, nullptr);
}

VkShaderModule VulkanRenderer::CreateShaderModule(const Opal::DynamicArray<u8>& code)
{
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.GetSize();
    create_info.pCode = reinterpret_cast<const u32*>(code.GetData());
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(m_device, &create_info, nullptr, &shader_module));
    return shader_module;
}

void VulkanRenderer::CreateFrameBuffers()
{
    m_swap_chain_frame_buffers.Resize(m_swap_chain_image_views.GetSize());
    for (u32 i = 0; i < m_swap_chain_image_views.GetSize(); ++i)
    {
        const VkImageView attachments[] = {m_swap_chain_image_views[i]};

        VkFramebufferCreateInfo frame_buffer_info{};
        frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buffer_info.renderPass = m_render_pass;
        frame_buffer_info.attachmentCount = 1;
        frame_buffer_info.pAttachments = attachments;
        frame_buffer_info.width = m_swap_chain_extent.width;
        frame_buffer_info.height = m_swap_chain_extent.height;
        frame_buffer_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(m_device, &frame_buffer_info, nullptr, &m_swap_chain_frame_buffers[i]));
    }
}

void VulkanRenderer::CreateCommandPool()
{
    QueueFamilyIndices queue_family_indices = FindQueueFamilies(m_physical_device);

    VkCommandPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    pool_info.queueFamilyIndex = queue_family_indices.graphics_family.value();
    pool_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    VK_CHECK(vkCreateCommandPool(m_device, &pool_info, nullptr, &m_command_pool));
}

void VulkanRenderer::CreateCommandBuffers()
{
    m_command_buffers.Resize(k_max_frames_in_flight);

    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.commandPool = m_command_pool;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandBufferCount = static_cast<u32>(m_command_buffers.GetSize());

    VK_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, m_command_buffers.GetData()));
}

void VulkanRenderer::RecordCommandBuffer(VkCommandBuffer command_buffer, u32 image_index)
{
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = nullptr;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    VkRenderPassBeginInfo render_pass_info{};
    render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    render_pass_info.renderPass = m_render_pass;
    render_pass_info.framebuffer = m_swap_chain_frame_buffers[image_index];
    render_pass_info.renderArea.offset = {0, 0};
    render_pass_info.renderArea.extent = m_swap_chain_extent;

    const VkClearValue clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    render_pass_info.clearValueCount = 1;
    render_pass_info.pClearValues = &clear_color;

    vkCmdBeginRenderPass(command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphics_pipeline);
    vkCmdSetViewport(command_buffer, 0, 1, &m_viewport);
    vkCmdSetScissor(command_buffer, 0, 1, &m_scissor);

    VkBuffer vertex_buffers[] = {m_vertex_buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffer, 0, 1, vertex_buffers, offsets);

    vkCmdDraw(command_buffer, static_cast<u32>(vertices.GetSize()), 1, 0, 0);
    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer));
}

void VulkanRenderer::Draw()
{
    // Wait for the previous frame to finish
    vkWaitForFences(m_device, 1, &m_in_flight_fences[m_current_frame_in_flight], VK_TRUE, UINT64_MAX);

    // Acquire an image from the swap chain
    u32 image_index = 0;
    VkResult result = vkAcquireNextImageKHR(m_device, m_swap_chain, UINT64_MAX, m_image_available_semaphores[m_current_frame_in_flight],
                                            VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    RNDR_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR);

    vkResetFences(m_device, 1, &m_in_flight_fences[m_current_frame_in_flight]);

    // Record the command buffer
    vkResetCommandBuffer(m_command_buffers[m_current_frame_in_flight], 0);
    RecordCommandBuffer(m_command_buffers[m_current_frame_in_flight], image_index);

    // Submit the command buffer
    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    const VkSemaphore wait_semaphores[] = {m_image_available_semaphores[m_current_frame_in_flight]};
    const VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &m_command_buffers[m_current_frame_in_flight];

    VkSemaphore signal_semaphores[] = {m_render_finished_semaphores[m_current_frame_in_flight]};
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;

    VK_CHECK(vkQueueSubmit(m_graphics_queue, 1, &submit_info, m_in_flight_fences[m_current_frame_in_flight]));

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;

    const VkSwapchainKHR swap_chains[] = {m_swap_chain};
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = nullptr;

    result = vkQueuePresentKHR(m_present_queue, &present_info);
    if (m_has_frame_buffer_resized)
    {
        m_has_frame_buffer_resized = false;
        RecreateSwapChain();
    }

    m_current_frame_in_flight = (m_current_frame_in_flight + 1) % k_max_frames_in_flight;
}

void VulkanRenderer::CreateSyncObjects()
{
    m_image_available_semaphores.Resize(k_max_frames_in_flight);
    m_render_finished_semaphores.Resize(k_max_frames_in_flight);
    m_in_flight_fences.Resize(k_max_frames_in_flight);

    VkSemaphoreCreateInfo semaphore_info{};
    semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (i32 i = 0; i < k_max_frames_in_flight; ++i)
    {
        VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_image_available_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(m_device, &semaphore_info, nullptr, &m_render_finished_semaphores[i]));
        VK_CHECK(vkCreateFence(m_device, &fence_info, nullptr, &m_in_flight_fences[i]));
    }
}
void VulkanRenderer::RecreateSwapChain()
{
    vkDeviceWaitIdle(m_device);

    CleanUpSwapChain();

    // TODO: Potentially here we would also like to recreate render pass if the image format changes.
    CreateSwapChain();

    m_viewport.width = static_cast<f32>(m_swap_chain_extent.width);
    m_viewport.height = static_cast<f32>(m_swap_chain_extent.height);
    m_scissor.extent = m_swap_chain_extent;

    CreateImageViews();
    CreateFrameBuffers();
}

void VulkanRenderer::CleanUpSwapChain()
{
    for (const VkFramebuffer& frame_buffer : m_swap_chain_frame_buffers)
    {
        vkDestroyFramebuffer(m_device, frame_buffer, nullptr);
    }
    for (const VkImageView& image_view : m_swap_chain_image_views)
    {
        vkDestroyImageView(m_device, image_view, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swap_chain, nullptr);
    m_swap_chain = VK_NULL_HANDLE;
}

void VulkanRenderer::OnResize()
{
    m_has_frame_buffer_resized = true;
}

void VulkanRenderer::CreateVertexBuffer()
{
    const VkDeviceSize buffer_size = sizeof(vertices[0]) * vertices.GetSize();

    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging_buffer, staging_buffer_memory);

    void* data = nullptr;
    VK_CHECK(vkMapMemory(m_device, staging_buffer_memory, 0, buffer_size, 0, &data));
    memcpy(data, vertices.GetData(), buffer_size);
    vkUnmapMemory(m_device, staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_vertex_buffer, m_vertex_buffer_memory);

    CopyBuffer(staging_buffer, m_vertex_buffer, buffer_size);

    vkDestroyBuffer(m_device, staging_buffer, nullptr);
    vkFreeMemory(m_device, staging_buffer_memory, nullptr);
}

u32 VulkanRenderer::FindMemoryType(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties)
{
    VkPhysicalDeviceMemoryProperties memory_properties;
    vkGetPhysicalDeviceMemoryProperties(physical_device, &memory_properties);

    for (u32 i = 0; i < memory_properties.memoryTypeCount; ++i)
    {
        if ((type_filter & (1 << i)) != 0 && (memory_properties.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }

    return 0;
}

void VulkanRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer,
                                  VkDeviceMemory& out_buffer_memory)
{
    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(m_device, &buffer_info, nullptr, &out_buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(m_device, out_buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(m_physical_device, memory_requirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(m_device, &alloc_info, nullptr, &out_buffer_memory));
    VK_CHECK(vkBindBufferMemory(m_device, out_buffer, out_buffer_memory, 0));
}

void VulkanRenderer::CopyBuffer(VkBuffer source_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = m_command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(m_device, &alloc_info, &command_buffer));

    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy_region{};
    copy_region.size = size;
    vkCmdCopyBuffer(command_buffer, source_buffer, dst_buffer, 1, &copy_region);

    vkEndCommandBuffer(command_buffer);

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    vkQueueSubmit(m_graphics_queue, 1, &submit_info, VK_NULL_HANDLE);
    vkQueueWaitIdle(m_graphics_queue);

    vkFreeCommandBuffers(m_device, m_command_pool, 1, &command_buffer);
}
