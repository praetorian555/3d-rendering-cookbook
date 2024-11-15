#include <optional>

#include "opal/defines.h"

#include "vulkan/vulkan.hpp"
#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.h"
#include "vulkan/vulkan_win32.h"
#endif

#include "rndr/input.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "opal/container/array.h"
#include "opal/container/ref.h"
#include "opal/container/string.h"
#include "opal/time.h"

#include "types.h"

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
    Opal::Span<Opal::StringUtf8> required_instance_extensions;
    Opal::Ref<Rndr::Window> window;
};

struct VulkanRenderer
{
    struct QueueFamilyIndices
    {
        std::optional<u32> graphics_family;
        std::optional<u32> present_family;
    };

    VulkanRenderer(const VulkanRendererDesc& desc = {});
    ~VulkanRenderer();

    Opal::Array<const char*> GetRequiredInstanceExtensions();

    static Opal::Array<VkExtensionProperties> GetSupportedInstanceExtensions();

private:
    void CreateInstance();
    void SetupDebugMessanger();
    void CreateSurface();
    void PickPhysicalDevice();
    bool IsDeviceSuitable(const VkPhysicalDevice& device);
    QueueFamilyIndices FindQueueFamilies(const VkPhysicalDevice& device);
    void CreateLogicalDevice();
    bool CheckDeviceExtensionSupport(const VkPhysicalDevice& device);

private:
    VulkanRendererDesc m_desc;
    VkInstance m_instance = VK_NULL_HANDLE;
    VkSurfaceKHR m_surface = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT m_debug_messenger = VK_NULL_HANDLE;
    VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
    VkDevice m_device = VK_NULL_HANDLE;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;

    Opal::Array<const char*> m_validation_layers = {"VK_LAYER_KHRONOS_validation"};
    Opal::Array<const char*> m_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
};

void Run()
{
    Rndr::Window window(Rndr::WindowDesc{.width = 800, .height = 600, .name = "Vulkan Triangle Example"});
    VulkanRendererDesc renderer_desc{.enable_validation_layers = true, .window = Opal::Ref(window)};
    VulkanRenderer renderer(renderer_desc);

    f32 delta_seconds = 1 / 60.0f;
    while (!window.IsClosed())
    {
        const f64 start_time = Opal::GetSeconds();

        window.ProcessEvents();
        Rndr::InputSystem::ProcessEvents(delta_seconds);

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

VulkanRenderer::VulkanRenderer(const VulkanRendererDesc& desc) : m_desc(desc)
{
    CreateInstance();
    SetupDebugMessanger();
    CreateSurface();
    PickPhysicalDevice();
    CreateLogicalDevice();
}

VulkanRenderer::~VulkanRenderer()
{
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
    Opal::Array<const char*> required_extensions = GetRequiredInstanceExtensions();
    Opal::Array<VkExtensionProperties> supported_extensions = GetSupportedInstanceExtensions();
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

        Opal::Array<VkLayerProperties> available_layers;
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

    [[maybe_unused]] VkResult vk_result = vkCreateInstance(&create_info, nullptr, &m_instance);
    RNDR_ASSERT(vk_result == VK_SUCCESS);
}

Opal::Array<const char*> VulkanRenderer::GetRequiredInstanceExtensions()
{
    Opal::Array<const char*> required_extension_names;
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

Opal::Array<VkExtensionProperties> VulkanRenderer::GetSupportedInstanceExtensions()
{
    u32 count = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr);

    Opal::Array<VkExtensionProperties> extensions;
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

void VulkanRenderer::SetupDebugMessanger()
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

    [[maybe_unused]] VkResult result = CreateDebugUtilsMessengerEXT(m_instance, &create_info, nullptr, &m_debug_messenger);
    RNDR_ASSERT(result == VK_SUCCESS);
}

void VulkanRenderer::CreateSurface()
{

#if defined(OPAL_PLATFORM_WINDOWS)
    VkWin32SurfaceCreateInfoKHR surface_create_info{};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hwnd = m_desc.window->GetNativeWindowHandle();
    surface_create_info.hinstance = GetModuleHandle(nullptr);

    [[maybe_unused]] VkResult vk_result = vkCreateWin32SurfaceKHR(m_instance, &surface_create_info, nullptr, &m_surface);
    RNDR_ASSERT(vk_result == VK_SUCCESS);
#else
#error Surface creation is not supported on this platform!
#endif
}

void VulkanRenderer::PickPhysicalDevice()
{
    u32 device_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &device_count, nullptr);
    RNDR_ASSERT(device_count > 0);

    Opal::Array<VkPhysicalDevice> devices(device_count);
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

    QueueFamilyIndices queue_family_indices = FindQueueFamilies(device);
    return queue_family_indices.graphics_family.has_value() && queue_family_indices.present_family.has_value() &&
           CheckDeviceExtensionSupport(device);
}

VulkanRenderer::QueueFamilyIndices VulkanRenderer::FindQueueFamilies(const VkPhysicalDevice& device)
{
    QueueFamilyIndices indices;

    u32 count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);

    Opal::Array<VkQueueFamilyProperties> queue_families(count);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &count, queue_families.GetData());
    i32 i = 0;
    for (const VkQueueFamilyProperties& queue_family : queue_families)
    {
        if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT)
        {
            indices.graphics_family = i;
        }
        VkBool32 present_support = false;
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
    Opal::Array<VkDeviceQueueCreateInfo> queue_create_infos;
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

    VkPhysicalDeviceFeatures device_features{};

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

    [[maybe_unused]] VkResult vk_result = vkCreateDevice(m_physical_device, &device_create_info, nullptr, &m_device);
    RNDR_ASSERT(vk_result == VK_SUCCESS);

    vkGetDeviceQueue(m_device, queue_family_indices.graphics_family.value(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device, queue_family_indices.present_family.value(), 0, &m_present_queue);
}

bool VulkanRenderer::CheckDeviceExtensionSupport(const VkPhysicalDevice& device)
{
    u32 extension_count = 0;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);

    Opal::Array<VkExtensionProperties> available_extensions(extension_count);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, available_extensions.GetData());

    Opal::Array<const char*> required_extensions = m_device_extensions;
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
