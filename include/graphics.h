/**
 * @file    graphics.h
 * @brief   Graphics type/routines
 * 
 * @see     https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
 * @see     https://gpuopen.com/learn/understanding-vulkan-objects/
 */
#pragma once
#include <filesystem>
#include <gsl/gsl>
#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

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

VkResult get_physical_device(VkInstance instance,
                             VkPhysicalDevice& physical_device) noexcept;

uint32_t get_graphics_queue_available(VkQueueFamilyProperties* properties,
                                      uint32_t count) noexcept;

/**
 * @brief Create a new `VkDevice` with GFX queue
 */
VkResult make_device(VkPhysicalDevice physical_device, VkDevice& handle,
                     uint32_t& queue_index, float priority = 0.012f) noexcept;

uint32_t get_surface_support(VkPhysicalDevice device, VkSurfaceKHR surface,
                             uint32_t count, uint32_t exclude_index) noexcept;

/**
 * @brief Create a new `VkDevice` with GFX, Present queue
 */
VkResult make_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface,
                     VkDevice& handle, //
                     uint32_t& graphics_index, uint32_t& present_index,
                     float priority = 0.012f) noexcept;

VkResult check_surface_format(VkPhysicalDevice device, VkSurfaceKHR surface,
                              VkFormat surface_format,
                              VkColorSpaceKHR surface_color_space,
                              bool& suitable) noexcept;
VkResult check_present_mode(VkPhysicalDevice device, VkSurfaceKHR surface,
                            const VkPresentModeKHR present_mode,
                            bool& suitable) noexcept;

/**
 * @todo Create a new `VkDevice` with GFX, Present, Transfer queue
 */

VkResult create_vertex_buffer(VkDevice device, VkBuffer& buffer,
                              VkBufferCreateInfo& info,
                              uint32_t length) noexcept;
VkResult create_index_buffer(VkDevice device, VkBuffer& buffer,
                             VkBufferCreateInfo& info,
                             uint32_t length) noexcept;

VkResult
allocate_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory& memory,
                const VkBufferCreateInfo& buffer_info, VkFlags desired,
                const VkPhysicalDeviceMemoryProperties& props) noexcept;

/// @todo https://vulkan-tutorial.com/en/Vertex_buffers/Staging_buffer
VkResult write_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                      const void* data) noexcept;

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

class vulkan_pipeline_input_t {
  public:
    virtual ~vulkan_pipeline_input_t() noexcept = default;

    virtual void setup_shader_stage(
        VkPipelineShaderStageCreateInfo (&stage)[2]) noexcept(false) = 0;
    virtual void setup_vertex_input_state(
        VkPipelineVertexInputStateCreateInfo& info) noexcept(false) = 0;
    virtual void record(VkPipeline pipeline,
                        VkCommandBuffer commands) noexcept(false) = 0;
};

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
    [[deprecated]] vulkan_pipeline_t(const vulkan_renderpass_t& renderpass,
                                     VkSurfaceCapabilitiesKHR& capabilities, //
                                     VkShaderModule vert,
                                     VkShaderModule frag) noexcept(false);
    vulkan_pipeline_t(const vulkan_renderpass_t& renderpass,
                      VkSurfaceCapabilitiesKHR& capabilities, //
                      vulkan_pipeline_input_t& input) noexcept(false);

    ~vulkan_pipeline_t() noexcept;

  public:
    static void setup_shader_stage(VkPipelineShaderStageCreateInfo (&stage)[2],
                                   VkShaderModule vert,
                                   VkShaderModule frag) noexcept;
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

auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;
auto read(FILE* stream, size_t& rsz) -> std::unique_ptr<std::byte[]>;
auto read_all(const fs::path& p, size_t& fsize) -> std::unique_ptr<std::byte[]>;
