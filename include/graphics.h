/**
 * @file    graphics.h
 * @brief   Graphics type/routines
 * 
 * @see     https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
 * @see     https://gpuopen.com/learn/understanding-vulkan-objects/
 */
#pragma once
#include <gsl/gsl>
#include <service.hpp>
#include <vulkan/vulkan.h>

using fn_get_extensions_t = const char** (*)(uint32_t*);

struct vulkan_exception_t final {
    const VkResult code;
    gsl::czstring<> message;

  public:
    constexpr vulkan_exception_t(VkResult _code,
                                 gsl::czstring<> _message) noexcept
        : code{_code}, message{_message} {
    }
};

class vulkan_instance_t final {
  public:
    VkInstance handle{};
    VkApplicationInfo info{};
    std::unique_ptr<VkLayerProperties[]> layers{};

  public:
    explicit vulkan_instance_t(gsl::czstring<> id,
                               fn_get_extensions_t fextensions) noexcept(false);
    ~vulkan_instance_t() noexcept;
};

VkResult get_physical_deivce(VkInstance instance,
                             VkPhysicalDevice& physical_device) noexcept;

uint32_t get_graphics_queue_available(VkQueueFamilyProperties* properties,
                                      uint32_t count) noexcept;

VkResult make_device(VkPhysicalDevice physical_device, VkDevice& handle,
                     uint32_t& queue_index, float priority = 0.012f) noexcept;

uint32_t get_surface_support(VkPhysicalDevice device, VkSurfaceKHR surface,
                             uint32_t count, uint32_t exclude_index) noexcept;

VkResult make_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                     VkDevice& handle, //
                     uint32_t& graphics_index, uint32_t& present_index,
                     float priority = 0.012f) noexcept;

struct vulkan_renderpass_t final {
    const VkDevice device{};
    VkRenderPass handle{};
    VkAttachmentDescription colors{};
    VkAttachmentReference color_ref{};
    VkSubpassDescription subpass{};

  public:
    vulkan_renderpass_t(VkDevice _device,
                        VkFormat surface_format) noexcept(false);
    ~vulkan_renderpass_t() noexcept;

  public:
    static void setup_color_attachment(VkAttachmentDescription& colors,
                                       VkAttachmentReference& color_ref,
                                       VkFormat surface_format) noexcept;
};

// shader stage creation
struct vulkan_pipeline_t final {
    const VkDevice device{};
    VkPipeline handle{};
    VkPipelineLayout layout{};
    VkViewport viewport{};
    VkRect2D scissor{};
    VkPipelineShaderStageCreateInfo shader_stages[2]{}; // vert, frag
    VkPipelineVertexInputStateCreateInfo vertex_input_state{};
    VkPipelineInputAssemblyStateCreateInfo input_assembly{};
    VkPipelineViewportStateCreateInfo viewport_state{};
    VkPipelineRasterizationStateCreateInfo rasterization{};
    VkPipelineMultisampleStateCreateInfo multisample{};
    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    VkPipelineColorBlendStateCreateInfo color_blend_state{};
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state{};

  public:
    vulkan_pipeline_t(const vulkan_renderpass_t& renderpass,
                      VkSurfaceCapabilitiesKHR& capabilities, //
                      VkShaderModule vert, VkShaderModule frag) noexcept(false);
    ~vulkan_pipeline_t() noexcept;

  public:
    static void setup_shader_stage(VkPipelineShaderStageCreateInfo (&stage)[2],
                                   VkShaderModule vert,
                                   VkShaderModule frag) noexcept {
        // for vertex shader
        stage[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[0].pSpecializationInfo = nullptr;
        stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage[0].module = vert;
        // for fragment shader
        stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[1].pSpecializationInfo = nullptr;
        stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage[1].module = frag;
        // default entry point: "main"
        stage[0].pName = stage[1].pName = "main";
    }
    static void setup_vertex_input_state(
        VkPipelineVertexInputStateCreateInfo& info) noexcept;
    static void
    setup_input_assembly(VkPipelineInputAssemblyStateCreateInfo& info) noexcept;
    static void setup_viewport_scissor(const VkSurfaceCapabilitiesKHR& cap,
                                       VkPipelineViewportStateCreateInfo& info,
                                       VkViewport& viewport,
                                       VkRect2D& scissor) noexcept;
    static void setup_rasterization_state(
        VkPipelineRasterizationStateCreateInfo& info) noexcept;
    static void setup_multi_sample_state(
        VkPipelineMultisampleStateCreateInfo& info) noexcept;
    static void
    setup_color_blend_state(VkPipelineColorBlendAttachmentState& attachment,
                            VkPipelineColorBlendStateCreateInfo& info) noexcept;
    static VkResult make_pipeline_layout(VkDevice device,
                                         VkPipelineLayout& layout) noexcept;
};

VkResult check_surface_format(VkPhysicalDevice device, VkSurfaceKHR surface,
                              VkFormat surface_format,
                              VkColorSpaceKHR surface_color_space,
                              bool& suitable) noexcept;
VkResult check_present_mode(VkPhysicalDevice device, VkSurfaceKHR surface,
                            const VkPresentModeKHR present_mode,
                            bool& suitable) noexcept;

struct vulkan_shader_module_t final {
    const VkDevice device{};
    VkShaderModule handle{};

  public:
    vulkan_shader_module_t(VkDevice _device,
                           const fs::path fpath) noexcept(false);
    ~vulkan_shader_module_t() noexcept;
};

// must update swapchain if resized
struct vulkan_swapchain_t final {
    const VkDevice device{};
    VkSwapchainKHR handle{};
    VkSwapchainCreateInfoKHR info{};

  public:
    /// @see https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPresentModeKHR.html
    vulkan_swapchain_t(VkDevice _device, VkSurfaceKHR surface,
                       const VkSurfaceCapabilitiesKHR& capabilities,
                       VkFormat surface_format,
                       VkColorSpaceKHR surface_color_space,
                       VkPresentModeKHR present_mode) noexcept(false);
    ~vulkan_swapchain_t() noexcept;
};

// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Framebuffers
struct vulkan_presentation_t final {
    VkDevice device{};
    uint32_t num_images = 0;
    std::unique_ptr<VkImage[]> images{};
    std::unique_ptr<VkImageView[]> image_views{};
    std::unique_ptr<VkFramebuffer[]> framebuffers{};

  public:
    vulkan_presentation_t(VkDevice _device, VkRenderPass renderpass,
                          VkSwapchainKHR swapchain,
                          const VkSurfaceCapabilitiesKHR& capabilities,
                          VkFormat surface_format) noexcept(false);
    ~vulkan_presentation_t() noexcept;
};

struct vulkan_command_pool_t final {
    const VkDevice device{};
    VkCommandPool handle{};
    const uint32_t count{};
    std::unique_ptr<VkCommandBuffer[]> buffers{};

  public:
    vulkan_command_pool_t(VkDevice _device, uint32_t queue_index,
                          uint32_t _count) noexcept(false);
    ~vulkan_command_pool_t() noexcept;
};

struct vulkan_semaphore_t final {
    const VkDevice device{};
    VkSemaphore handle{};

  public:
    vulkan_semaphore_t(VkDevice _device) noexcept(false);
    ~vulkan_semaphore_t() noexcept;
};

struct vulkan_fence_t final {
    const VkDevice device{};
    VkFence handle{};

  public:
    vulkan_fence_t(VkDevice _device) noexcept(false);
    ~vulkan_fence_t() noexcept;
};
