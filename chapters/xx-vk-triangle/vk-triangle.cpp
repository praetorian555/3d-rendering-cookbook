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

struct VulkanRenderer
{
    VulkanRenderer(Opal::Span<Opal::StringUtf8> required_instance_extensions = {});
    ~VulkanRenderer();

    Opal::Array<VkExtensionProperties> GetSupportedInstanceExtensions();

private:
    void CreateInstance(Opal::Span<Opal::StringUtf8> required_instance_extensions);

private:
    VkInstance m_instance;
};

void Run()
{
    Rndr::Window window(Rndr::WindowDesc{.width = 800, .height = 600, .name = "Vulkan Triangle Example"});
    VulkanRenderer renderer;

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

VulkanRenderer::VulkanRenderer(Opal::Span<Opal::StringUtf8> required_instance_extensions)
{
    Opal::Array<VkExtensionProperties> supported_extensions = GetSupportedInstanceExtensions();
    for (const Opal::StringUtf8& required_extension_name : required_instance_extensions)
    {
        bool is_found = false;
        for (const VkExtensionProperties& supported_extension : supported_extensions)
        {
            auto compare_result =
                Opal::Compare(required_extension_name, 0, required_extension_name.GetSize(), (const c8*)supported_extension.extensionName);
            if (compare_result.GetValue() == 0)
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

    CreateInstance(required_instance_extensions);
}

void VulkanRenderer::CreateInstance(Opal::Span<Opal::StringUtf8> required_instance_extensions)
{
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = "Vulkan Triangle Example";
    app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.pEngineName = "RNDR";
    app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    app_info.apiVersion = VK_API_VERSION_1_0;

    Opal::Array<const char*> enabled_extension_names;
    if (required_instance_extensions.GetSize() > 0)
    {
        enabled_extension_names.Resize(required_instance_extensions.GetSize());
        for (int i = 0; i < required_instance_extensions.GetSize(); ++i)
        {
            enabled_extension_names[i] = (const char*)required_instance_extensions[i].GetData();
        }
    }

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = static_cast<u32>(enabled_extension_names.GetSize());
    create_info.ppEnabledExtensionNames = enabled_extension_names.GetData();
    create_info.enabledLayerCount = 0;

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
