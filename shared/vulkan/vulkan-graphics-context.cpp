#include "vulkan/vulkan-graphics-context.hpp"

#include "opal/defines.h"

#if defined(OPAL_PLATFORM_WINDOWS)
#include "rndr/platform/windows-header.h"

#include <vulkan/vulkan_win32.h>
#endif

#include "opal/container/dynamic-array.h"

#include "rndr/definitions.h"
#include "rndr/return-macros.hpp"

VulkanGraphicsContext::VulkanGraphicsContext(const VulkanGraphicsContextDesc& desc)
{
    Init(desc);
}

VulkanGraphicsContext::~VulkanGraphicsContext()
{
    Destroy();
}

namespace
{
VkResult CreateDebugUtilsMessengerEXT(VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT* create_info,
                                      const VkAllocationCallbacks* allocator, VkDebugUtilsMessengerEXT* debug_messenger)
{
    auto func = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
    if (func != nullptr)
    {
        return func(instance, create_info, allocator, debug_messenger);
    }
    return VK_ERROR_EXTENSION_NOT_PRESENT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator)
{
    auto func = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT"));
    if (func != nullptr)
    {
        func(instance, debug_messenger, allocator);
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT, VkDebugUtilsMessageTypeFlagsEXT,
                                             const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void*)
{
    RNDR_LOG_INFO("[Vulkan Validation] %s", callback_data->pMessage);
    return VK_FALSE;
}
}  // namespace

bool VulkanGraphicsContext::Init(const VulkanGraphicsContextDesc& desc)
{
    // Check if all the requested instance extensions are supported
    Opal::DynamicArray<const char*> required_extensions = GetRequiredInstanceExtensions(desc);
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
        RNDR_RETURN_ON_FAIL(is_found, false, "Extension not supported!", RNDR_NOOP);
    }

    Opal::DynamicArray validation_layer_names = {"VK_LAYER_KHRONOS_validation"};

    // Check if validation layers are available
    if (desc.enable_validation_layers)
    {
        u32 available_layer_count = 0;
        vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);

        Opal::DynamicArray<VkLayerProperties> available_layers;
        available_layers.Resize(available_layer_count);
        vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.GetData());

        for (const char* validation_layer_name : validation_layer_names)
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
            RNDR_RETURN_ON_FAIL(is_found, false, "Validation layer not available!", RNDR_NOOP);
        }
    }

    // Creation of instance
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
    if (desc.enable_validation_layers)
    {
        create_info.enabledLayerCount = static_cast<u32>(validation_layer_names.GetSize());
        create_info.ppEnabledLayerNames = validation_layer_names.GetData();
    }
    else
    {
        create_info.enabledLayerCount = 0;
    }
    VkResult result = vkCreateInstance(&create_info, nullptr, &m_instance);
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to create Vulkan instance!", RNDR_NOOP);

    // Creation of debug messanger
    if (m_desc.enable_validation_layers)
    {
        VkDebugUtilsMessengerCreateInfoEXT debug_create_info{};
        debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debug_create_info.pfnUserCallback = DebugCallback;
        debug_create_info.pUserData = nullptr;
        result = CreateDebugUtilsMessengerEXT(m_instance, &debug_create_info, nullptr, &m_debug_messenger);
        RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, false, "Failed to create debug messenger!", Destroy());
    }

    m_desc = desc;
    return true;
}

bool VulkanGraphicsContext::Destroy()
{
    if (m_debug_messenger != VK_NULL_HANDLE)
    {
        DestroyDebugUtilsMessengerEXT(m_instance, m_debug_messenger, nullptr);
        m_debug_messenger = VK_NULL_HANDLE;
    }
    if (m_instance != VK_NULL_HANDLE)
    {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
    }
    return true;
}

Opal::DynamicArray<const char*> VulkanGraphicsContext::GetRequiredInstanceExtensions(const VulkanGraphicsContextDesc& desc)
{
    Opal::DynamicArray<const char*> required_extension_names;
    if (desc.required_instance_extensions.GetSize() > 0)
    {
        required_extension_names.Resize(desc.required_instance_extensions.GetSize());
        for (int i = 0; i < desc.required_instance_extensions.GetSize(); ++i)
        {
            required_extension_names[i] = desc.required_instance_extensions[i].GetData();
        }
    }
    // We need this extension if we want to display the image to the display
    required_extension_names.PushBack(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(OPAL_PLATFORM_WINDOWS)
    // We need it if we want to display the image to the display on Windows
    required_extension_names.PushBack(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#endif
    if (desc.enable_validation_layers)
    {
        required_extension_names.PushBack(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    return required_extension_names;
}

Opal::DynamicArray<VkExtensionProperties> VulkanGraphicsContext::GetSupportedInstanceExtensions()
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

Opal::DynamicArray<VulkanPhysicalDevice> VulkanGraphicsContext::EnumeratePhysicalDevices() const
{
    u32 gpu_count = 0;
    vkEnumeratePhysicalDevices(m_instance, &gpu_count, nullptr);

    Opal::DynamicArray<VkPhysicalDevice> physical_devices(gpu_count);
    const VkResult result = vkEnumeratePhysicalDevices(m_instance, &gpu_count, physical_devices.GetData());
    RNDR_RETURN_ON_FAIL(result == VK_SUCCESS, Opal::DynamicArray<VulkanPhysicalDevice>(), "Failed to enumerate physical devices!",
                        RNDR_NOOP);

    Opal::DynamicArray<VulkanPhysicalDevice> gpu_list;
    for (const VkPhysicalDevice& device : physical_devices)
    {
        gpu_list.PushBack(VulkanPhysicalDevice(device));
    }
    return gpu_list;
}
