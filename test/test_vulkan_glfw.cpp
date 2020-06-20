
#include <catch2/catch.hpp>

#include <chrono>
#include <iostream>
#include <string_view>
#include <thread>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "graphics.h"

fs::path get_asset_dir() noexcept;

using namespace std;

auto create_vulkan_window(gsl::czstring<> window_name) noexcept
    -> std::shared_ptr<GLFWwindow> {
    REQUIRE(glfwInit());
    REQUIRE(glfwVulkanSupported());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    auto window =
        glfwCreateWindow(90 * 5, 90 * 5, window_name, nullptr, nullptr);
    REQUIRE(window);
    // ...
    return {window, [](GLFWwindow* window) {
                glfwDestroyWindow(window);
                glfwTerminate();
            }};
}

auto make_vulkan_surface(GLFWwindow* window, //
                         VkInstance instance, VkPhysicalDevice device,
                         VkSurfaceKHR& surface,
                         VkSurfaceCapabilitiesKHR& capabilities)
    -> std::shared_ptr<void> {
    REQUIRE(glfwCreateWindowSurface(instance, window, nullptr, &surface) //
            == VK_SUCCESS);
    REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                                      &capabilities) //
            == VK_SUCCESS);
    return {surface,
            [=](void*) { vkDestroySurfaceKHR(instance, surface, nullptr); }};
}

void render_loop(GLFWwindow* window, //
                 VkDevice device, VkQueue queues[2],
                 uint32_t graphics_index, //
                 const VkSurfaceCapabilitiesKHR& capabilities,
                 vulkan_pipeline_t& pipeline, vulkan_renderpass_t& renderpass,
                 vulkan_swapchain_t& swapchain,
                 vulkan_presentation_t& presentation,
                 vulkan_pipeline_input_t& input);

auto make_pipeline_input(VkDevice device,
                         const VkPhysicalDeviceMemoryProperties& props)
    -> std::unique_ptr<vulkan_pipeline_input_t>;

// https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
// https://www.glfw.org/docs/3.3/vulkan_guide.html
// https://vulkan-tutorial.com/Introduction
TEST_CASE("GLFW + Vulkan", "[vulkan][glfw]") {
    // glfw init + window
    auto window = create_vulkan_window("Test Window: Vulkan");
    // instance / physical device
    vulkan_instance_t instance{"glfw3", glfwGetRequiredInstanceExtensions};
    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance.handle, physical_device) ==
            VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties physical_properties{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &physical_properties);

    // surface
    VkSurfaceKHR surface{};
    VkSurfaceCapabilitiesKHR capabilities{};
    auto holder = make_vulkan_surface(window.get(), //
                                      instance.handle, physical_device, surface,
                                      capabilities);
    constexpr auto surface_format = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr auto surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    {
        bool suitable = false;
        REQUIRE(check_surface_format(physical_device, surface, surface_format,
                                     surface_color_space,
                                     suitable) == VK_SUCCESS);
        REQUIRE(suitable);
    }
    constexpr auto present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        bool suitable = false;
        REQUIRE(check_present_mode(physical_device, surface, present_mode,
                                   suitable) == VK_SUCCESS);
        REQUIRE(suitable);
    }

    // virtual device & queue
    VkDevice device{};
    uint32_t graphics_index = 0, presentation_index = 0;
    REQUIRE(make_device(physical_device, surface, device, //
                        graphics_index, presentation_index) == VK_SUCCESS);
    auto on_return_3 = gsl::finally([&device]() { //
        vkDestroyDevice(device, nullptr);
    });
    VkQueue queues[2]{};
    vkGetDeviceQueue(device, graphics_index, 0, queues + 0);
    vkGetDeviceQueue(device, presentation_index, 0, queues + 1);
    REQUIRE(queues[0] != VK_NULL_HANDLE);
    REQUIRE(queues[1] != VK_NULL_HANDLE);

    auto input = make_pipeline_input(device, physical_properties);

    // graphics pipeline + presentation
    vulkan_renderpass_t renderpass{device, surface_format};
    const auto asset_dir = get_asset_dir();
    vulkan_shader_module_t vert{device, asset_dir / "sample_vert.spv"};
    vulkan_shader_module_t frag{device, asset_dir / "sample_frag.spv"};
    vulkan_pipeline_t pipeline{renderpass, capabilities, //
                               *input, vert.handle, frag.handle};
    {
        auto swapchain = std::make_unique<vulkan_swapchain_t>(
            device, surface, capabilities, surface_format, surface_color_space,
            present_mode);
        auto presentation = std::make_unique<vulkan_presentation_t>(
            device, renderpass.handle, swapchain->handle, capabilities,
            surface_format);
        render_loop(window.get(), device, queues, graphics_index, capabilities,
                    pipeline, renderpass, //
                    *swapchain, *presentation, *input);
        REQUIRE(vkDeviceWaitIdle(device) == VK_SUCCESS);
    }
}

class process_timer_t final {
    clock_t begin = clock(); // <time.h>

  public:
    float pick() const noexcept {
        auto current = clock();
        return static_cast<float>(current - begin) / CLOCKS_PER_SEC;
    }
    float reset() noexcept {
        const auto d = pick();
        begin = clock();
        return d;
    }
};

void sleep_for_fps(process_timer_t& timer, uint32_t hz) {
    const std::chrono::milliseconds time_per_frame{1000 / hz};
    const std::chrono::milliseconds elapsed{
        static_cast<uint32_t>(timer.reset() * 1000)};
    if (elapsed < time_per_frame) {
        const auto sleep_time = time_per_frame - elapsed;
        this_thread::sleep_for(sleep_time);
    }
}

class recorder_t final {
    VkCommandBuffer command_buffer;

  public:
    recorder_t(VkCommandBuffer _command_buffer, //
               VkRenderPass renderpass, VkFramebuffer framebuffer,
               VkExtent2D extent)
        : command_buffer{_command_buffer} {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        REQUIRE(vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS);
        VkRenderPassBeginInfo render{};
        render.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render.renderPass = renderpass;
        render.framebuffer = framebuffer;
        render.renderArea.offset = {0, 0}; // consider shader (loads/stores)
        render.renderArea.extent = extent;
        VkClearValue color{0.0f, 0.0f, 0.0f, 1.0f};
        render.clearValueCount = 1;
        render.pClearValues = &color;
        vkCmdBeginRenderPass(command_buffer, &render,
                             VK_SUBPASS_CONTENTS_INLINE);
    };
    ~recorder_t() {
        vkCmdEndRenderPass(command_buffer);
        REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
    };
    VkResult record(VkPipeline pipeline, vulkan_pipeline_input_t& input) {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline);
        input.record(pipeline, command_buffer);
        return VK_SUCCESS;
    }
};

VkResult render_submit(VkQueue gfx_queue, uint32_t image_index,
                       VkCommandBuffer& command_buffer, //
                       VkFence fence,                   //
                       VkSemaphore wait, VkSemaphore signal) {
    VkSubmitInfo submit{};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore wait_group[] = {wait};
    submit.pWaitSemaphores = wait_group;
    submit.waitSemaphoreCount = 1;
    const VkPipelineStageFlags stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submit.pWaitDstStageMask = stages;
    submit.pCommandBuffers = &command_buffer;
    submit.commandBufferCount = 1;
    VkSemaphore signal_group[] = {signal};
    submit.pSignalSemaphores = signal_group;
    submit.signalSemaphoreCount = 1;
    return vkQueueSubmit(gfx_queue, 1, &submit, fence);
}

VkResult present_submit(VkQueue present_queue, uint32_t image_index,
                        VkSwapchainKHR swapchain, VkSemaphore wait) {
    VkPresentInfoKHR info{};
    info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    VkSemaphore wait_group[] = {wait};
    info.pWaitSemaphores = wait_group;
    info.waitSemaphoreCount = 1;
    VkSwapchainKHR swapchains[] = {swapchain};
    info.pSwapchains = swapchains;
    info.swapchainCount = 1;
    info.pImageIndices = &image_index;
    return vkQueuePresentKHR(present_queue, &info);
}

void render_loop(GLFWwindow* window, //
                 VkDevice device, VkQueue queues[2],
                 uint32_t graphics_index, //
                 const VkSurfaceCapabilitiesKHR& capabilities,
                 vulkan_pipeline_t& pipeline, vulkan_renderpass_t& renderpass,
                 vulkan_swapchain_t& swapchain,
                 vulkan_presentation_t& presentation,
                 vulkan_pipeline_input_t& input) {
    // command buffer
    vulkan_command_pool_t command_pool{device, graphics_index,
                                       presentation.num_images};
    for (auto i = 0u; i < presentation.num_images; ++i) {
        // record: command buffer + renderpass + pipeline
        recorder_t r{command_pool.buffers[i], renderpass.handle,
                     presentation.framebuffers[i], capabilities.maxImageExtent};
        if (auto ec = r.record(pipeline.handle, input))
            FAIL(ec);
    }
    // synchronization + timer
    vulkan_semaphore_t semaphore_1{device}, semaphore_2{device};
    vulkan_fence_t fence{device};
    process_timer_t timer{};
    // game loop
    auto repeat = 120;
    while (!glfwWindowShouldClose(window) && repeat--) {
        /// update
        glfwPollEvents();
        /// render: select command buffer for current image
        ///         and submit to GFX queue
        uint32_t image_index{};
        {
            if (auto ec = vkAcquireNextImageKHR(device, swapchain.handle,
                                                UINT64_MAX, semaphore_1.handle,
                                                VK_NULL_HANDLE, &image_index))
                FAIL(ec);
            if (auto ec = render_submit(queues[0], image_index, //
                                        command_pool.buffers[image_index],
                                        fence.handle, //
                                        semaphore_1.handle, semaphore_2.handle))
                FAIL(ec);
        }
        /// present: semaphore for synchronization and request presentation
        ///          @todo glfwSwapBuffers(window.get());
        {
            if (auto ec = present_submit(queues[1], image_index,
                                         swapchain.handle, semaphore_2.handle))
                FAIL(ec);
            if (auto ec = vkQueueWaitIdle(queues[1]))
                FAIL(ec);
        }
        /// wait for each loop
        VkFence fences[1]{fence.handle};
        uint64_t nano = UINT64_MAX;
        if (auto ec = vkWaitForFences(device, 1, fences, VK_TRUE, nano))
            FAIL(ec);
        if (auto ec = vkResetFences(device, 1, fences))
            FAIL(ec);
        sleep_for_fps(timer, 120);
    }
}

struct input1_t : vulkan_pipeline_input_t {
    struct input_unit_t final {
        glm::vec2 position{};
        glm::vec3 color{};
    };

    static constexpr auto binding_count = 1;

  public:
    const VkDevice device{};
    std::vector<input_unit_t> vertices{};
    VkPipelineVertexInputStateCreateInfo info{};
    VkVertexInputBindingDescription desc{};
    VkVertexInputAttributeDescription attrs[2]{};
    VkBuffer buffers[binding_count]{};
    VkDeviceSize offsets[binding_count]{};
    VkBufferCreateInfo buffer_info{};
    VkDeviceMemory memory{};

  public:
    input1_t(VkDevice _device, const VkPhysicalDeviceMemoryProperties& props)
        : device{_device}, vertices{{{0.0f, -0.5f}, {1, 0, 0}},
                                    {{0.5f, 0.5f}, {0, 1, 0}},
                                    {{-0.5f, 0.5f}, {0, 0, 1}}} {
        make_input_description(info, desc, attrs);
        if (auto ec = create_buffer(buffers[0], buffer_info))
            throw vulkan_exception_t{ec, "vkCreateBuffer"};
        if (auto ec = create_memory(buffers[0], memory, buffer_info, props)) {
            vkDestroyBuffer(device, buffers[0], nullptr);
            throw vulkan_exception_t{ec, "vkAllocateMemory"};
        }
        if (auto ec = update(buffers[0], memory)) {
            vkFreeMemory(device, memory, nullptr);
            vkDestroyBuffer(device, buffers[0], nullptr);
            throw vulkan_exception_t{ec, "vkBindBufferMemory || vkMapMemory"};
        }
    }
    ~input1_t() noexcept {
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffers[0], nullptr);
    }

    static void make_input_description(
        VkPipelineVertexInputStateCreateInfo& info,
        VkVertexInputBindingDescription& desc,
        VkVertexInputAttributeDescription (&attrs)[2]) noexcept {
        desc.binding = 0;
        desc.stride = sizeof(input_unit_t);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // per vertex input
        // layout(location = 0) in vec2 i_position;
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32_SFLOAT; // vec2
        attrs[0].offset = 0;
        // layout(location = 1) in vec3 i_color;
        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attrs[1].offset = sizeof(input_unit_t::position);
        // ...
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info.vertexBindingDescriptionCount = 1;
        info.pVertexBindingDescriptions = &desc;
        info.vertexAttributeDescriptionCount = 2;
        info.pVertexAttributeDescriptions = attrs;
    }

    void setup_vertex_input_state(
        VkPipelineVertexInputStateCreateInfo& _info) noexcept override {
        _info = this->info;
    }

    VkResult create_buffer(VkBuffer& buffer, VkBufferCreateInfo& info) const
        noexcept {
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = sizeof(input_unit_t) * vertices.size();
        info.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        return vkCreateBuffer(device, &info, nullptr, &buffer);
    }
    VkResult create_memory(VkBuffer buffer, VkDeviceMemory& memory,
                           const VkBufferCreateInfo& buffer_info,
                           const VkPhysicalDeviceMemoryProperties& props) const
        noexcept {
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = requirements.size;
        for (auto i = 0u; i < props.memoryTypeCount; ++i) {
            // match type?
            const auto typebit = (1 << i);
            if (requirements.memoryTypeBits & typebit) {
                const auto desired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
                // match prop?
                const auto mtype = props.memoryTypes[i];
                if ((mtype.propertyFlags & desired) == desired)
                    info.memoryTypeIndex = i;
            }
        }
        return vkAllocateMemory(device, &info, nullptr, &memory);
    }

    VkResult update(VkBuffer buffer, VkDeviceMemory memory) const noexcept {
        constexpr auto offset = 0;
        if (auto ec = vkBindBufferMemory(device, buffer, memory, offset))
            return ec;
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        const auto flags = VkMemoryMapFlags{};
        void* ptr = nullptr;
        if (auto ec = vkMapMemory(device, memory, offset, requirements.size, //
                                  flags, &ptr))
            return ec;
        memcpy(ptr, vertices.data(), requirements.size);
        vkUnmapMemory(device, memory);
        return VK_SUCCESS;
    }

    void record(VkPipeline pipeline,
                VkCommandBuffer command_buffer) noexcept override {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline);
        constexpr auto first_binding = 0;
        vkCmdBindVertexBuffers(command_buffer, first_binding, binding_count,
                               buffers, offsets);
        constexpr auto num_instance = 1;
        constexpr auto first_vertex = 0;
        constexpr auto first_instance = 0;
        vkCmdDraw(command_buffer, static_cast<uint32_t>(vertices.size()),
                  num_instance, first_vertex, first_instance);
    }
};

auto make_pipeline_input(VkDevice device,
                         const VkPhysicalDeviceMemoryProperties& props)
    -> std::unique_ptr<vulkan_pipeline_input_t> {

    return std::make_unique<input1_t>(device, props);
}
