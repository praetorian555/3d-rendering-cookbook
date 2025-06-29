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
#include "opal/math/transform.h"
#include "opal/paths.h"
#include "opal/time.h"

#include "rndr/application.hpp"
#include "rndr/file.h"
#include "rndr/generic-window.hpp"
#include "rndr/input-system.hpp"
#include "rndr/log.h"
#include "rndr/projections.h"

#include "types.h"
#include "vulkan/vulkan-graphics-context.hpp"
#include "vulkan/vulkan-swap-chain.hpp"

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

struct UniformBufferObject
{
    Rndr::Matrix4x4f model;
    Rndr::Matrix4x4f view;
    Rndr::Matrix4x4f projection;
};

const Opal::DynamicArray<Vertex> g_vertices = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
                                               {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
                                               {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
                                               {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};

const Opal::DynamicArray<u16> g_indices = {0, 1, 2, 2, 3, 0};

#define VK_CHECK(expr)                                                  \
    do                                                                  \
    {                                                                   \
        [[maybe_unused]] const VkResult result_ = expr;                 \
        RNDR_ASSERT(result_ == VK_SUCCESS, "Vulkan operation failed!"); \
    } while (0)

void Run(Rndr::Application* app);
int main()
{
    Rndr::ApplicationDesc desc{.enable_input_system = true};
    Rndr::Application* app = Rndr::Application::Create(desc);
    RNDR_ASSERT(app != nullptr, "Failed to create app!");
    Run(app);
    Rndr::Application::Destroy();
    return 0;
}

struct VulkanRendererDesc
{
    bool enable_validation_layers = false;
    Opal::ArrayView<Opal::StringUtf8> required_instance_extensions;
    Opal::Ref<Rndr::GenericWindow> window;
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

    explicit VulkanRenderer(VulkanRendererDesc desc = {});
    ~VulkanRenderer();

    void Draw();
    void RecreateSwapChain();

    void OnResize();

    static u32 FindMemoryType(VkPhysicalDevice physical_device, u32 type_filter, VkMemoryPropertyFlags properties);

private:
    void CreateQueues();
    void CreateRenderPass();
    void CreateDescriptorSetLayout();
    void CreateGraphicsPipeline();
    VkShaderModule CreateShaderModule(const Opal::DynamicArray<u8>& code);
    void CreateFrameBuffers();
    void CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer,
                      VkDeviceMemory& out_buffer_memory);
    void CreateVertexBuffer();
    void CreateIndexBuffer();
    void CreateUniformBuffers();
    void CreateDescriptorPool();
    void CreateDescriptorSets();
    void CreateCommandBuffers();
    void CreateSyncObjects();
    void CleanUpSwapChain();

    void RecordCommandBuffer(VkCommandBuffer command_buffer, u32 image_index);
    void CopyBuffer(VkBuffer source_buffer, VkBuffer dst_buffer, VkDeviceSize size);
    void UpdateUniformBuffer(u32 current_frame);

    VulkanRendererDesc m_desc;
    VulkanGraphicsContext m_graphics_context;
    VulkanSurface m_surface;
    VulkanDevice m_device;
    VulkanQueueFamilyIndices m_queue_family_indices;
    VkQueue m_graphics_queue = VK_NULL_HANDLE;
    VkQueue m_present_queue = VK_NULL_HANDLE;
    VulkanSwapChain m_swap_chain;
    VkViewport m_viewport;
    VkRect2D m_scissor;
    VkRenderPass m_render_pass = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_descriptor_set_layout = VK_NULL_HANDLE;
    VkPipelineLayout m_pipeline_layout = VK_NULL_HANDLE;
    VkPipeline m_graphics_pipeline = VK_NULL_HANDLE;
    Opal::DynamicArray<VkFramebuffer> m_swap_chain_frame_buffers;
    VkBuffer m_vertex_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_vertex_buffer_memory = VK_NULL_HANDLE;
    VkBuffer m_index_buffer = VK_NULL_HANDLE;
    VkDeviceMemory m_index_buffer_memory = VK_NULL_HANDLE;
    Opal::DynamicArray<VkCommandBuffer> m_command_buffers;
    Opal::DynamicArray<VkSemaphore> m_image_available_semaphores;
    Opal::DynamicArray<VkSemaphore> m_render_finished_semaphores;
    Opal::DynamicArray<VkFence> m_in_flight_fences;
    Opal::DynamicArray<VkBuffer> m_uniform_buffers;
    Opal::DynamicArray<VkDeviceMemory> m_uniform_buffers_memory;
    Opal::DynamicArray<void*> m_mapped_uniform_buffers;
    VkDescriptorPool m_descriptor_pool = VK_NULL_HANDLE;
    Opal::DynamicArray<VkDescriptorSet> m_descriptor_sets;

    Opal::DynamicArray<const char*> m_validation_layers = {"VK_LAYER_KHRONOS_validation"};
    Opal::DynamicArray<const char*> m_device_extensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    u32 m_current_frame_in_flight = 0;
    bool m_has_frame_buffer_resized = false;
};

void Run(Rndr::Application* app)
{
    Rndr::GenericWindow* window = app->CreateGenericWindow({.width = 800, .height = 600, .name = "Vulkan Triangle Example"});
    const VulkanRendererDesc renderer_desc{.enable_validation_layers = true, .window = Opal::Ref(window)};
    VulkanRenderer renderer(renderer_desc);

    app->on_window_resize.Bind([&renderer](const Rndr::GenericWindow&, i32, i32) { renderer.OnResize(); });

    f32 delta_seconds = 1 / 60.0f;
    while (!window->IsClosed())
    {
        const f64 start_time = Opal::GetSeconds();

        app->ProcessSystemEvents(delta_seconds);

        i32 x, y, width, height;
        window->GetPositionAndSize(x, y, width, height);

        // Detect if we are minimized
        const Rndr::Vector2f window_size = {static_cast<float>(width), static_cast<float>(height)};
        if (window_size.x != 0 && window_size.y != 0)
        {
            renderer.Draw();
        }

        const f64 end_time = Opal::GetSeconds();
        delta_seconds = static_cast<f32>(end_time - start_time);
    }

    app->DestroyGenericWindow(window);
}

VulkanRenderer::VulkanRenderer(VulkanRendererDesc desc) : m_desc(Opal::Move(desc))
{
    m_graphics_context.Init();
    m_surface.Init(m_graphics_context, m_desc.window->GetNativeHandle());
    auto physical_devices = m_graphics_context.EnumeratePhysicalDevices();
    RNDR_ASSERT(physical_devices.GetSize() > 0, "No physical devices found!");
    VulkanDeviceDesc device_desc;
    device_desc.surface = m_surface;
    m_device.Init(Opal::Move(physical_devices[0]), device_desc);
    m_queue_family_indices = m_device.GetQueueFamilyIndices();
    CreateQueues();
    auto window_size = m_desc.window->GetSize();
    RNDR_ASSERT(window_size.HasValue(), "Failed to get window size!");
    const u32 width = static_cast<u32>(window_size.GetValue().x);
    const u32 height = static_cast<u32>(window_size.GetValue().y);
    m_swap_chain.Init(m_device, m_surface, {.width = width, .height = height});
    CreateRenderPass();
    CreateDescriptorSetLayout();
    CreateGraphicsPipeline();
    CreateFrameBuffers();
    CreateVertexBuffer();
    CreateIndexBuffer();
    CreateUniformBuffers();
    CreateDescriptorPool();
    CreateDescriptorSets();
    CreateCommandBuffers();
    CreateSyncObjects();
}

VulkanRenderer::~VulkanRenderer()
{
    vkDeviceWaitIdle(m_device.GetNativeDevice());
    for (i32 i = 0; i < k_max_frames_in_flight; ++i)
    {
        vkDestroySemaphore(m_device.GetNativeDevice(), m_render_finished_semaphores[i], nullptr);
        vkDestroySemaphore(m_device.GetNativeDevice(), m_image_available_semaphores[i], nullptr);
        vkDestroyFence(m_device.GetNativeDevice(), m_in_flight_fences[i], nullptr);
    }
    vkDestroyBuffer(m_device.GetNativeDevice(), m_index_buffer, nullptr);
    vkFreeMemory(m_device.GetNativeDevice(), m_index_buffer_memory, nullptr);
    vkDestroyBuffer(m_device.GetNativeDevice(), m_vertex_buffer, nullptr);
    vkFreeMemory(m_device.GetNativeDevice(), m_vertex_buffer_memory, nullptr);
    for (const VkFramebuffer& frame_buffer : m_swap_chain_frame_buffers)
    {
        vkDestroyFramebuffer(m_device.GetNativeDevice(), frame_buffer, nullptr);
    }
    for (i32 i = 0; i < k_max_frames_in_flight; ++i)
    {
        vkDestroyBuffer(m_device.GetNativeDevice(), m_uniform_buffers[i], nullptr);
        vkFreeMemory(m_device.GetNativeDevice(), m_uniform_buffers_memory[i], nullptr);
    }
    vkDestroyDescriptorPool(m_device.GetNativeDevice(), m_descriptor_pool, nullptr);
    vkDestroyDescriptorSetLayout(m_device.GetNativeDevice(), m_descriptor_set_layout, nullptr);
    vkDestroyPipeline(m_device.GetNativeDevice(), m_graphics_pipeline, nullptr);
    vkDestroyPipelineLayout(m_device.GetNativeDevice(), m_pipeline_layout, nullptr);
    vkDestroyRenderPass(m_device.GetNativeDevice(), m_render_pass, nullptr);
    m_device.DestroyCommandBuffers(m_command_buffers, m_queue_family_indices.graphics_family);
    m_swap_chain.Destroy();
    m_surface.Destroy();
    m_device.Destroy();
    m_graphics_context.Destroy();
}

void VulkanRenderer::CreateRenderPass()
{
    VkAttachmentDescription color_attachment{};
    color_attachment.format = m_swap_chain.GetDesc().pixel_format;
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

    VK_CHECK(vkCreateRenderPass(m_device.GetNativeDevice(), &render_pass_info, nullptr, &m_render_pass));
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

    const VkVertexInputBindingDescription binding_description = Vertex::GetBindingDescription();
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
    m_viewport.width = static_cast<f32>(m_swap_chain.GetExtent().width);
    m_viewport.height = static_cast<f32>(m_swap_chain.GetExtent().height);
    m_viewport.minDepth = 0.0f;
    m_viewport.maxDepth = 1.0f;

    m_scissor.offset = {.x = 0, .y = 0};
    m_scissor.extent = m_swap_chain.GetExtent();

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
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
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
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &m_descriptor_set_layout;
    pipeline_layout_info.pushConstantRangeCount = 0;
    pipeline_layout_info.pPushConstantRanges = nullptr;

    VK_CHECK(vkCreatePipelineLayout(m_device.GetNativeDevice(), &pipeline_layout_info, nullptr, &m_pipeline_layout));

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

    VK_CHECK(vkCreateGraphicsPipelines(m_device.GetNativeDevice(), VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &m_graphics_pipeline));

    vkDestroyShaderModule(m_device.GetNativeDevice(), vertex_shader_module, nullptr);
    vkDestroyShaderModule(m_device.GetNativeDevice(), fragment_shader_module, nullptr);
}

VkShaderModule VulkanRenderer::CreateShaderModule(const Opal::DynamicArray<u8>& code)
{
    VkShaderModuleCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    create_info.codeSize = code.GetSize();
    create_info.pCode = reinterpret_cast<const u32*>(code.GetData());
    VkShaderModule shader_module = VK_NULL_HANDLE;
    VK_CHECK(vkCreateShaderModule(m_device.GetNativeDevice(), &create_info, nullptr, &shader_module));
    return shader_module;
}

void VulkanRenderer::CreateFrameBuffers()
{
    const auto& image_views = m_swap_chain.GetImageViews();
    m_swap_chain_frame_buffers.Resize(image_views.GetSize());
    for (u32 i = 0; i < image_views.GetSize(); ++i)
    {
        const VkImageView attachments[] = {image_views[i]};

        VkFramebufferCreateInfo frame_buffer_info{};
        frame_buffer_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        frame_buffer_info.renderPass = m_render_pass;
        frame_buffer_info.attachmentCount = 1;
        frame_buffer_info.pAttachments = attachments;
        frame_buffer_info.width = m_swap_chain.GetExtent().width;
        frame_buffer_info.height = m_swap_chain.GetExtent().height;
        frame_buffer_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(m_device.GetNativeDevice(), &frame_buffer_info, nullptr, &m_swap_chain_frame_buffers[i]));
    }
}

void VulkanRenderer::CreateCommandBuffers()
{
    const VulkanQueueFamilyIndices indices = m_device.GetQueueFamilyIndices();
    m_command_buffers = m_device.CreateCommandBuffers(indices.graphics_family, k_max_frames_in_flight);
    RNDR_ASSERT(m_command_buffers.GetSize() == k_max_frames_in_flight, "Failed to create command buffers!");
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
    render_pass_info.renderArea.extent = m_swap_chain.GetExtent();

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

    vkCmdBindIndexBuffer(command_buffer, m_index_buffer, 0, VK_INDEX_TYPE_UINT16);

    vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_pipeline_layout, 0, 1,
                            &m_descriptor_sets[m_current_frame_in_flight], 0, nullptr);

    vkCmdDrawIndexed(command_buffer, static_cast<u32>(g_indices.GetSize()), 1, 0, 0, 0);
    vkCmdEndRenderPass(command_buffer);

    VK_CHECK(vkEndCommandBuffer(command_buffer));
}

void VulkanRenderer::Draw()
{
    // Wait for the previous frame to finish
    vkWaitForFences(m_device.GetNativeDevice(), 1, &m_in_flight_fences[m_current_frame_in_flight], VK_TRUE, UINT64_MAX);

    // Acquire an image from the swap chain
    u32 image_index = 0;
    VkResult result = vkAcquireNextImageKHR(m_device.GetNativeDevice(), m_swap_chain.GetNativeSwapChain(), UINT64_MAX,
                                            m_image_available_semaphores[m_current_frame_in_flight], VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR)
    {
        RecreateSwapChain();
        return;
    }
    RNDR_ASSERT(result == VK_SUCCESS || result == VK_SUBOPTIMAL_KHR, "Failed to acquire next image from the swap chain!");

    vkResetFences(m_device.GetNativeDevice(), 1, &m_in_flight_fences[m_current_frame_in_flight]);

    // Record the command buffer
    vkResetCommandBuffer(m_command_buffers[m_current_frame_in_flight], 0);
    RecordCommandBuffer(m_command_buffers[m_current_frame_in_flight], image_index);

    UpdateUniformBuffer(m_current_frame_in_flight);

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

    const VkSwapchainKHR swap_chains[] = {m_swap_chain.GetNativeSwapChain()};
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
        VK_CHECK(vkCreateSemaphore(m_device.GetNativeDevice(), &semaphore_info, nullptr, &m_image_available_semaphores[i]));
        VK_CHECK(vkCreateSemaphore(m_device.GetNativeDevice(), &semaphore_info, nullptr, &m_render_finished_semaphores[i]));
        VK_CHECK(vkCreateFence(m_device.GetNativeDevice(), &fence_info, nullptr, &m_in_flight_fences[i]));
    }
}
void VulkanRenderer::RecreateSwapChain()
{
    vkDeviceWaitIdle(m_device.GetNativeDevice());

    CleanUpSwapChain();

    auto size = m_desc.window->GetSize();
    RNDR_ASSERT(size.HasValue(), "Failed to get window size!");

    // TODO: Potentially here we would also like to recreate render pass if the image format changes.
    const u32 width = static_cast<u32>(size.GetValue().x);
    const u32 height = static_cast<u32>(size.GetValue().y);
    m_swap_chain.Init(m_device, m_surface, {.width = width, .height = height});

    m_viewport.width = static_cast<f32>(m_swap_chain.GetExtent().width);
    m_viewport.height = static_cast<f32>(m_swap_chain.GetExtent().height);
    m_scissor.extent = m_swap_chain.GetExtent();

    CreateFrameBuffers();
}

void VulkanRenderer::CleanUpSwapChain()
{
    for (const VkFramebuffer& frame_buffer : m_swap_chain_frame_buffers)
    {
        vkDestroyFramebuffer(m_device.GetNativeDevice(), frame_buffer, nullptr);
    }
    m_swap_chain.Destroy();
}

void VulkanRenderer::OnResize()
{
    m_has_frame_buffer_resized = true;
}

void VulkanRenderer::CreateVertexBuffer()
{
    const VkDeviceSize buffer_size = sizeof(g_vertices[0]) * g_vertices.GetSize();

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging_buffer, staging_buffer_memory);

    void* data = nullptr;
    VK_CHECK(vkMapMemory(m_device.GetNativeDevice(), staging_buffer_memory, 0, buffer_size, 0, &data));
    memcpy(data, g_vertices.GetData(), buffer_size);
    vkUnmapMemory(m_device.GetNativeDevice(), staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_vertex_buffer, m_vertex_buffer_memory);

    CopyBuffer(staging_buffer, m_vertex_buffer, buffer_size);

    vkDestroyBuffer(m_device.GetNativeDevice(), staging_buffer, nullptr);
    vkFreeMemory(m_device.GetNativeDevice(), staging_buffer_memory, nullptr);
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

void VulkanRenderer::CreateQueues()
{
    auto index = m_device.GetPhysicalDevice().GetQueueFamilyIndex(VK_QUEUE_GRAPHICS_BIT);
    RNDR_ASSERT(index.HasValue(), "No graphics queue!");
    vkGetDeviceQueue(m_device.GetNativeDevice(), index.GetValue(), 0, &m_graphics_queue);
    vkGetDeviceQueue(m_device.GetNativeDevice(), index.GetValue(), 0, &m_present_queue);
}

void VulkanRenderer::CreateBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& out_buffer,
                                  VkDeviceMemory& out_buffer_memory)
{
    VkBufferCreateInfo buffer_info{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer_info.size = size;
    buffer_info.usage = usage;
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    VK_CHECK(vkCreateBuffer(m_device.GetNativeDevice(), &buffer_info, nullptr, &out_buffer));

    VkMemoryRequirements memory_requirements;
    vkGetBufferMemoryRequirements(m_device.GetNativeDevice(), out_buffer, &memory_requirements);

    VkMemoryAllocateInfo alloc_info{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    alloc_info.allocationSize = memory_requirements.size;
    alloc_info.memoryTypeIndex = FindMemoryType(m_device.GetNativePhysicalDevice(), memory_requirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(m_device.GetNativeDevice(), &alloc_info, nullptr, &out_buffer_memory));
    VK_CHECK(vkBindBufferMemory(m_device.GetNativeDevice(), out_buffer, out_buffer_memory, 0));
}

void VulkanRenderer::CopyBuffer(VkBuffer source_buffer, VkBuffer dst_buffer, VkDeviceSize size)
{
    const VulkanQueueFamilyIndices indices = m_device.GetQueueFamilyIndices();
    VkCommandBuffer command_buffer = m_device.CreateCommandBuffer(indices.graphics_family);
    RNDR_ASSERT(command_buffer != VK_NULL_HANDLE, "Failed to create command buffer!");

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

    m_device.DestroyCommandBuffer(command_buffer, indices.graphics_family);
}

void VulkanRenderer::CreateIndexBuffer()
{
    const VkDeviceSize buffer_size = sizeof(g_indices[0]) * g_indices.GetSize();

    VkBuffer staging_buffer = VK_NULL_HANDLE;
    VkDeviceMemory staging_buffer_memory = VK_NULL_HANDLE;
    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 staging_buffer, staging_buffer_memory);

    void* data = nullptr;
    VK_CHECK(vkMapMemory(m_device.GetNativeDevice(), staging_buffer_memory, 0, buffer_size, 0, &data));
    memcpy(data, g_indices.GetData(), buffer_size);
    vkUnmapMemory(m_device.GetNativeDevice(), staging_buffer_memory);

    CreateBuffer(buffer_size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                 m_index_buffer, m_index_buffer_memory);

    CopyBuffer(staging_buffer, m_index_buffer, buffer_size);

    vkDestroyBuffer(m_device.GetNativeDevice(), staging_buffer, nullptr);
    vkFreeMemory(m_device.GetNativeDevice(), staging_buffer_memory, nullptr);
}

void VulkanRenderer::CreateDescriptorSetLayout()
{
    VkDescriptorSetLayoutBinding ubo_layout_binding{};
    ubo_layout_binding.binding = 0;
    ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    ubo_layout_binding.descriptorCount = 1;
    ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

    VkDescriptorSetLayoutCreateInfo layout_info{};
    layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layout_info.bindingCount = 1;
    layout_info.pBindings = &ubo_layout_binding;

    VK_CHECK(vkCreateDescriptorSetLayout(m_device.GetNativeDevice(), &layout_info, nullptr, &m_descriptor_set_layout));
}

void VulkanRenderer::CreateUniformBuffers()
{
    const VkDeviceSize buffer_size = sizeof(UniformBufferObject);

    m_uniform_buffers.Resize(k_max_frames_in_flight);
    m_uniform_buffers_memory.Resize(k_max_frames_in_flight);
    m_mapped_uniform_buffers.Resize(k_max_frames_in_flight);

    for (u32 i = 0; i < k_max_frames_in_flight; ++i)
    {
        CreateBuffer(buffer_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_uniform_buffers[i],
                     m_uniform_buffers_memory[i]);
        VK_CHECK(vkMapMemory(m_device.GetNativeDevice(), m_uniform_buffers_memory[i], 0, buffer_size, 0, &m_mapped_uniform_buffers[i]));
    }
}

void VulkanRenderer::UpdateUniformBuffer(u32 current_frame)
{
    static f64 s_start_time = Opal::GetSeconds();

    const f64 current_time = Opal::GetSeconds();
    const f32 delta_time_since_program_start = static_cast<f32>(current_time - s_start_time);

    UniformBufferObject ubo{};
    ubo.model = Opal::RotateZ(90.0f * delta_time_since_program_start);
    ubo.model = Opal::Transpose(ubo.model);
    ubo.view = Opal::LookAt_RH(Rndr::Point3f{2.0f, 2.0f, 2.0f}, Rndr::Point3f{0.0f, 0.0f, 0.0f}, Rndr::Vector3f{0.0f, 0.0f, 1.0f});
    ubo.view = Opal::Transpose(ubo.view);
    const f32 aspect_ratio = static_cast<f32>(m_swap_chain.GetExtent().width) / static_cast<f32>(m_swap_chain.GetExtent().height);
    ubo.projection = Rndr::PerspectiveVulkan(45.0f, aspect_ratio, 0.1f, 10.0f);
    ubo.projection = Opal::Transpose(ubo.projection);

    memcpy(m_mapped_uniform_buffers[current_frame], &ubo, sizeof(ubo));
}

void VulkanRenderer::CreateDescriptorPool()
{
    VkDescriptorPoolSize pool_size{};
    pool_size.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    pool_size.descriptorCount = k_max_frames_in_flight;

    VkDescriptorPoolCreateInfo pool_info{};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.poolSizeCount = 1;
    pool_info.pPoolSizes = &pool_size;
    pool_info.maxSets = k_max_frames_in_flight;

    VK_CHECK(vkCreateDescriptorPool(m_device.GetNativeDevice(), &pool_info, nullptr, &m_descriptor_pool));
}

void VulkanRenderer::CreateDescriptorSets()
{
    Opal::DynamicArray<VkDescriptorSetLayout> layouts(k_max_frames_in_flight, m_descriptor_set_layout);
    VkDescriptorSetAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = m_descriptor_pool;
    alloc_info.descriptorSetCount = k_max_frames_in_flight;
    alloc_info.pSetLayouts = layouts.GetData();

    m_descriptor_sets.Resize(k_max_frames_in_flight);
    VK_CHECK(vkAllocateDescriptorSets(m_device.GetNativeDevice(), &alloc_info, m_descriptor_sets.GetData()));

    for (i32 i = 0; i < k_max_frames_in_flight; i++)
    {
        VkDescriptorBufferInfo buffer_info{};
        buffer_info.buffer = m_uniform_buffers[i];
        buffer_info.offset = 0;
        buffer_info.range = sizeof(UniformBufferObject);

        VkWriteDescriptorSet descriptor_write{};
        descriptor_write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptor_write.dstSet = m_descriptor_sets[i];
        descriptor_write.dstBinding = 0;
        descriptor_write.dstArrayElement = 0;
        descriptor_write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        descriptor_write.descriptorCount = 1;
        descriptor_write.pBufferInfo = &buffer_info;

        vkUpdateDescriptorSets(m_device.GetNativeDevice(), 1, &descriptor_write, 0, nullptr);
    }
}
