/**
 * @see     https://www.khronos.org/registry/vulkan/specs/1.2-extensions/html/vkspec.html
 * @see     https://gpuopen.com/learn/understanding-vulkan-objects/
 */
#pragma once
#include <filesystem>
#include <gsl/gsl>
#include <memory>
#include <thread>
#include <vulkan/vulkan.h>

namespace fs = std::filesystem;

auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;
auto create(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;
auto read(FILE* stream, size_t& rsz) -> std::unique_ptr<std::byte[]>;
auto read_all(const fs::path& p, size_t& fsize) -> std::unique_ptr<std::byte[]>;

class stop_watch_t final {
    clock_t begin = clock(); // <time.h>

  public:
    uint64_t elapsed() const noexcept {
        return clock() - begin;
    }
    float pick() const noexcept {
        return static_cast<float>(elapsed()) / CLOCKS_PER_SEC;
    }
    float reset() noexcept {
        const auto d = pick();
        begin = clock();
        return d;
    }
};

static void sleep_for_fps(stop_watch_t& timer, uint32_t hz) noexcept {
    const std::chrono::milliseconds time_per_frame{1000 / hz};
    const std::chrono::milliseconds elapsed{static_cast<uint32_t>(timer.reset() * 1000)};
    if (elapsed < time_per_frame) {
        const auto sleep_time = time_per_frame - elapsed;
        std::this_thread::sleep_for(sleep_time);
    }
}

struct vulkan_exception_t final {
    const VkResult code;
    gsl::czstring<> message;
};

/**
 * @brief   VkInstance + RAII
 */
class vulkan_instance_t final {
  public:
    VkInstance handle{};
    VkApplicationInfo info{};
    std::string name;

  public:
    explicit vulkan_instance_t(gsl::czstring<> name,                //
                               gsl::span<const char* const> layers, //
                               gsl::span<const char* const> extensions) noexcept(false);
    ~vulkan_instance_t() noexcept;
    vulkan_instance_t(const vulkan_instance_t&) = delete;
    vulkan_instance_t(vulkan_instance_t&&) = delete;
    vulkan_instance_t& operator=(const vulkan_instance_t&) = delete;
    vulkan_instance_t& operator=(vulkan_instance_t&&) = delete;
};

VkResult get_physical_device(VkInstance instance, VkPhysicalDevice& physical_device) noexcept;

uint32_t get_graphics_queue_available(VkQueueFamilyProperties* properties, uint32_t count) noexcept;

/**
 * @brief Create a new `VkDevice` with GFX queue
 */
VkResult make_device(VkPhysicalDevice physical_device, //
                     VkDevice& handle,                 //
                     uint32_t& queue_index, float priority = 0.012f) noexcept;

uint32_t get_surface_support(VkPhysicalDevice device, VkSurfaceKHR surface, uint32_t count,
                             uint32_t exclude_index) noexcept;

/**
 * @brief Create a new `VkDevice` with GFX, Present queue
 */
VkResult make_device(VkPhysicalDevice physical_device, VkSurfaceKHR surface, //
                     VkDevice& handle,                                       //
                     uint32_t& graphics_index, uint32_t& present_index, float priority = 0.012f) noexcept;

VkResult check_surface_format(VkPhysicalDevice device, VkSurfaceKHR surface, VkFormat surface_format,
                              VkColorSpaceKHR surface_color_space, bool& suitable) noexcept;
VkResult check_present_mode(VkPhysicalDevice device, VkSurfaceKHR surface, const VkPresentModeKHR present_mode,
                            bool& suitable) noexcept;

/**
 * @brief create 1 device with 2 queue information
 * 
 * @param queues queue information. 0 is for graphics, 1 is for presentation
 * @return VkResult `VK_SUCCESS` if everything was successful
 */
VkResult create_device(VkPhysicalDevice physical_device,                     //
                       const VkSurfaceKHR* surfaces, uint32_t surface_count, //
                       VkDevice& device, VkDeviceQueueCreateInfo (&queues)[2]) noexcept;

/**
 * @param surface_format  `VK_FORMAT_B8G8R8A8_UNORM` 44, 
 *                        `VK_FORMAT_B8G8R8A8_SRGB` 50, 
 *                        `VK_FORMAT_A2R10G10B10_UNORM_PACK32` 58, 
 *                        `VK_FORMAT_R16G16B16A16_SFLOAT` 97
 */
VkResult check_support(VkPhysicalDevice device, VkSurfaceKHR surface, //
                       VkFormat surface_format, VkColorSpaceKHR surface_color_space,
                       VkPresentModeKHR present_mode) noexcept;

/**
 * @todo Create a new `VkDevice` with GFX, Present, Transfer queue
 */

VkResult create_uniform_buffer(VkDevice device, VkBuffer& buffer, VkBufferCreateInfo& info,
                               VkDeviceSize buflen) noexcept;
VkResult create_vertex_buffer(VkDevice device, VkBuffer& buffer, VkBufferCreateInfo& info,
                              VkDeviceSize buflen) noexcept;
VkResult create_index_buffer(VkDevice device, VkBuffer& buffer, VkBufferCreateInfo& info, VkDeviceSize buflen) noexcept;

VkResult allocate_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory& memory,
                         const VkBufferCreateInfo& buffer_info, VkFlags desired,
                         const VkPhysicalDeviceMemoryProperties& props) noexcept;

/// @todo https://vulkan-tutorial.com/en/Vertex_buffers/Staging_buffer
/// @see vkBindBufferMemory
/// @see vkMapMemory
VkResult update_memory(VkDevice device, VkDeviceMemory memory,
                       const VkMemoryRequirements& requirements, //
                       const void* data, uint32_t offset = 0) noexcept;

/// @see vkMapMemory
[[deprecated]] VkResult write_memory(VkDevice device, VkBuffer buffer, VkDeviceMemory memory,
                                     const void* data) noexcept;

/**
 * @brief   VkRenderPass + RAII
 * @note    currently only 1 subpass
 */
class vulkan_renderpass_t final {
  public:
    const VkDevice device{};
    VkRenderPass handle{};
    VkAttachmentDescription colors{};
    VkAttachmentReference color_ref{};
    VkSubpassDescription subpasses[1]{};

  public:
    vulkan_renderpass_t(VkDevice _device, VkFormat surface_format) noexcept(false);
    ~vulkan_renderpass_t() noexcept;

  public:
    static void setup_color_attachment(VkAttachmentDescription& colors, VkAttachmentReference& color_ref,
                                       VkFormat surface_format) noexcept;
};

class vulkan_pipeline_input_t {
  public:
    virtual ~vulkan_pipeline_input_t() noexcept = default;

    virtual void setup_shader_stage(VkPipelineShaderStageCreateInfo (&stage)[2]) noexcept(false) = 0;
    virtual void setup_vertex_input_state(VkPipelineVertexInputStateCreateInfo& info) noexcept(false) = 0;
    virtual VkResult make_pipeline_layout(VkDevice device, VkPipelineLayout& layout) noexcept = 0;

    virtual void record(VkCommandBuffer command_buffer, //
                        VkPipeline pipeline, VkPipelineLayout pipeline_layout) noexcept(false) = 0;
    virtual VkResult update() noexcept(false) {
        return VK_SUCCESS;
    };
};

auto make_pipeline_input_1(VkDevice device,
                           const VkPhysicalDeviceMemoryProperties& props, //
                           const fs::path& folder) -> std::unique_ptr<vulkan_pipeline_input_t>;
auto make_pipeline_input_2(VkDevice device,
                           const VkPhysicalDeviceMemoryProperties& props, //
                           const fs::path& folder) -> std::unique_ptr<vulkan_pipeline_input_t>;
auto make_pipeline_input_3(VkDevice device,
                           const VkPhysicalDeviceMemoryProperties& props, //
                           const fs::path& shader_dir) -> std::unique_ptr<vulkan_pipeline_input_t>;

/**
 * @brief VkPipeline + VkPipelineLayout + RAII
 * @todo  setup_color_blend_state
 * @todo  setup_depth_stencil_state
 */
class vulkan_pipeline_t final {
  public:
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
    vulkan_pipeline_t(VkDevice device, VkRenderPass renderpass,
                      VkExtent2D& extent, //
                      vulkan_pipeline_input_t& input) noexcept(false);

    ~vulkan_pipeline_t() noexcept;

  public:
    [[deprecated]] static void setup_shader_stage(VkPipelineShaderStageCreateInfo (&stage)[2], //
                                                  VkShaderModule vert, VkShaderModule frag) noexcept;
    [[deprecated]] static void setup_vertex_input_state(VkPipelineVertexInputStateCreateInfo& info) noexcept;
    [[deprecated]] static VkResult make_pipeline_layout(VkDevice device, VkPipelineLayout& layout) noexcept;

    static void setup_input_assembly(VkPipelineInputAssemblyStateCreateInfo& info) noexcept;
    /// @note currently using viewport & scissor have equal size
    static void setup_viewport_scissor(const VkExtent2D& extent, VkPipelineViewportStateCreateInfo& info,
                                       VkViewport& viewport, VkRect2D& scissor) noexcept;
    static void setup_rasterization_state(VkPipelineRasterizationStateCreateInfo& info) noexcept;
    static void setup_multi_sample_state(VkPipelineMultisampleStateCreateInfo& info) noexcept;
    static void setup_color_blend_state(VkPipelineColorBlendAttachmentState& attachment,
                                        VkPipelineColorBlendStateCreateInfo& info) noexcept;
};

class vulkan_shader_module_t final {
  public:
    const VkDevice device{};
    VkShaderModule handle{};

  public:
    vulkan_shader_module_t(VkDevice _device, const fs::path fpath) noexcept(false);
    ~vulkan_shader_module_t() noexcept;
};

/**
 * @brief VkSwapchainKHR + RAII
 * @note  must update swapchain if resized
 * @see https://www.khronos.org/registry/vulkan/specs/1.2-extensions/man/html/VkPresentModeKHR.html
 */
class vulkan_swapchain_t final {
  public:
    VkDevice device{};
    VkSwapchainKHR handle{};
    VkSwapchainCreateInfoKHR info{};

  public:
    vulkan_swapchain_t(VkDevice _device, VkSurfaceKHR surface, const VkSurfaceCapabilitiesKHR& capabilities,
                       VkFormat surface_format, VkColorSpaceKHR surface_color_space,
                       VkPresentModeKHR present_mode) noexcept(false);
    ~vulkan_swapchain_t() noexcept;
};

// https://vulkan-tutorial.com/en/Drawing_a_triangle/Drawing/Framebuffers
class vulkan_presentation_t final {
  public:
    VkDevice device{};
    uint32_t num_images = 0;
    std::unique_ptr<VkImage[]> images{};
    std::unique_ptr<VkImageView[]> image_views{};
    std::unique_ptr<VkFramebuffer[]> framebuffers{};

  public:
    vulkan_presentation_t(VkDevice _device, VkRenderPass renderpass, VkSwapchainKHR swapchain,
                          const VkSurfaceCapabilitiesKHR& capabilities, VkFormat surface_format) noexcept(false);
    ~vulkan_presentation_t() noexcept;
};

class vulkan_command_pool_t final {
  public:
    const VkDevice device{};
    VkCommandPool handle{};
    const uint32_t count{};
    std::unique_ptr<VkCommandBuffer[]> buffers{};

  public:
    vulkan_command_pool_t(VkDevice _device, uint32_t queue_index, uint32_t _count) noexcept(false);
    ~vulkan_command_pool_t() noexcept;
};

class vulkan_semaphore_t final {
  public:
    const VkDevice device{};
    VkSemaphore handle{};

  public:
    vulkan_semaphore_t(VkDevice _device) noexcept(false);
    ~vulkan_semaphore_t() noexcept;
};

class vulkan_fence_t final {
  public:
    const VkDevice device{};
    VkFence handle{};

  public:
    vulkan_fence_t(VkDevice _device) noexcept(false);
    ~vulkan_fence_t() noexcept;
};

VkResult render_submit(VkQueue queue,                       //
                       gsl::span<VkCommandBuffer> commands, //
                       VkFence fence, VkSemaphore wait, VkSemaphore signal) noexcept;

VkResult present_submit(VkQueue queue,                                  //
                        uint32_t image_index, VkSwapchainKHR swapchain, //
                        VkSemaphore wait) noexcept;

class vulkan_command_recorder_t final {
  public:
    VkCommandBuffer commands;
    VkClearValue clear;

  public:
    vulkan_command_recorder_t(VkCommandBuffer command_buffer, //
                              VkRenderPass renderpass, VkFramebuffer framebuffer, VkExtent2D extent) noexcept(false);
    ~vulkan_command_recorder_t() noexcept(false);
};
