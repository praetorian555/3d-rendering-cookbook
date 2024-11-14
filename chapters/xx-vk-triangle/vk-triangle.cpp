#include "vulkan/vulkan.hpp"

#include "rndr/input.h"
#include "rndr/rndr.h"
#include "rndr/window.h"

#include "opal/container/array.h"
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
};

struct VulkanRenderer
{
    VulkanRenderer(const VulkanRendererDesc& desc = {});
    ~VulkanRenderer();

    Opal::Array<VkExtensionProperties> GetSupportedInstanceExtensions();

private:
    void CreateInstance();

private:
    VulkanRendererDesc m_desc;
    VkInstance m_instance;
};

void Run()
{
    Rndr::Window window(Rndr::WindowDesc{.width = 800, .height = 600, .name = "Vulkan Triangle Example"});
    VulkanRendererDesc renderer_desc{.enable_validation_layers = true};
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

VulkanRenderer::VulkanRenderer(const VulkanRendererDesc& desc) : m_desc(desc)
{
    CreateInstance();
}

void VulkanRenderer::CreateInstance()
{
    // Check if all the requested instance extensions are supported
    Opal::Array<VkExtensionProperties> supported_extensions = GetSupportedInstanceExtensions();
    for (const Opal::StringUtf8& required_extension_name : m_desc.required_instance_extensions)
    {
        bool is_found = false;
        for (const VkExtensionProperties& supported_extension : supported_extensions)
        {
            auto compare_result =
                Opal::Compare(required_extension_name, 0, required_extension_name.GetSize(), (const c8*)supported_extension.extensionName);
            if (compare_result.HasValue() && compare_result.GetValue() == 0)
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

    Opal::Array<const char*> validation_layers = {"VK_LAYER_KHRONOS_validation"};
    if (m_desc.enable_validation_layers)
    {
        u32 available_layer_count = 0;
        vkEnumerateInstanceLayerProperties(&available_layer_count, nullptr);

        Opal::Array<VkLayerProperties> available_layers;
        available_layers.Resize(available_layer_count);
        vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers.GetData());

        for (const char* validation_layer_name : validation_layers)
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

    Opal::Array<const char*> enabled_extension_names;
    if (m_desc.required_instance_extensions.GetSize() > 0)
    {
        enabled_extension_names.Resize(m_desc.required_instance_extensions.GetSize());
        for (int i = 0; i < m_desc.required_instance_extensions.GetSize(); ++i)
        {
            enabled_extension_names[i] = (const char*)m_desc.required_instance_extensions[i].GetData();
        }
    }

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<u32>(enabled_extension_names.GetSize());
    create_info.ppEnabledExtensionNames = enabled_extension_names.GetData();
    if (m_desc.enable_validation_layers)
    {
        create_info.enabledLayerCount = static_cast<u32>(validation_layers.GetSize());
        create_info.ppEnabledLayerNames = validation_layers.GetData();
    }
    else
    {
        create_info.enabledLayerCount = 0;
    }

    VkResult vk_result = vkCreateInstance(&create_info, nullptr, &m_instance);
    RNDR_ASSERT(vk_result == VK_SUCCESS);
}

VulkanRenderer::~VulkanRenderer()
{
    vkDestroyInstance(m_instance, nullptr);
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
