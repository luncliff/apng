#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <thread>

#include "vulkan_1.h"

using namespace std;

fs::path get_asset_dir() noexcept;
auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;
auto create_window_glfw(gsl::czstring<> name) noexcept -> std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>;
auto make_vulkan_instance(GLFWwindow*, gsl::czstring<> name) -> std::unique_ptr<vulkan_instance_t>;

class stop_watch_t final {
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

void sleep_for_fps(stop_watch_t& timer, uint32_t hz) noexcept {
    const chrono::milliseconds time_per_frame{1000 / hz};
    const chrono::milliseconds elapsed{static_cast<uint32_t>(timer.reset() * 1000)};
    if (elapsed < time_per_frame) {
        const auto sleep_time = time_per_frame - elapsed;
        this_thread::sleep_for(sleep_time);
    }
}

class recorder_t final {
  public:
    VkCommandBuffer command_buffer;
    VkClearValue color{0.0f, 0.0f, 0.0f, 1.0f};

  public:
    recorder_t(VkCommandBuffer _command_buffer, //
               VkRenderPass renderpass, VkFramebuffer framebuffer, VkExtent2D extent) noexcept(false)
        : command_buffer{_command_buffer} {
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        if (auto ec = vkBeginCommandBuffer(command_buffer, &begin))
            throw vulkan_exception_t{ec, "vkBeginCommandBuffer"};
        VkRenderPassBeginInfo render{};
        render.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render.renderPass = renderpass;
        render.framebuffer = framebuffer;
        render.renderArea.offset = {0, 0}; // consider shader (loads/stores)
        render.renderArea.extent = extent;
        render.clearValueCount = 1;
        render.pClearValues = &color;
        vkCmdBeginRenderPass(command_buffer, &render, VK_SUBPASS_CONTENTS_INLINE);
    }
    ~recorder_t() noexcept(false) {
        vkCmdEndRenderPass(command_buffer);
        if (auto ec = vkEndCommandBuffer(command_buffer))
            throw vulkan_exception_t{ec, "vkEndCommandBuffer"};
    }
};

TEST_CASE("RenderPass + Pipeline", "[vulkan]") {
    const char* layers[1]{"VK_LAYER_KHRONOS_validation"};
    const char* extensions[1]{"VK_KHR_surface"};
    vulkan_instance_t instance{"RenderPass + Pipeline", //
                               gsl::make_span(layers, 1), gsl::make_span(extensions, 1)};
    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance.handle, physical_device) == VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties meminfo{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &meminfo);

    // virtual device & queue
    VkDevice device{};
    uint32_t graphics_index = 0;
    REQUIRE(make_device(physical_device, device, //
                        graphics_index) == VK_SUCCESS);
    auto on_return_2 = gsl::finally([&device]() { //
        vkDestroyDevice(device, nullptr);
    });
    VkQueue queues[1]{};
    vkGetDeviceQueue(device, graphics_index, 0, queues + 0);
    REQUIRE(queues[0] != VK_NULL_HANDLE);

    // input data + shader + recording
    auto input = make_pipeline_input_2(device, meminfo, get_asset_dir());
    VkExtent2D min_image_extent{900, 900};

    // graphics pipeline + presentation
    vulkan_renderpass_t renderpass{device, VK_FORMAT_B8G8R8A8_UNORM};
    REQUIRE(renderpass.handle);
    vulkan_pipeline_t pipeline{renderpass, min_image_extent, *input};
    REQUIRE(pipeline.handle);
}

TEST_CASE("RenderPass + Pipeline + Render", "[vulkan][glfw]") {
    // glfw init + window
    const auto title = "RenderPass + Pipeline + Render";
    auto window = create_window_glfw(title);
    glfwMakeContextCurrent(window.get());

    // instance / physical device
    auto instance = make_vulkan_instance(window.get(), title);
    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance->handle, physical_device) == VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties meminfo{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &meminfo);

    // surface
    VkSurfaceKHR surface{};
    VkSurfaceCapabilitiesKHR capabilities{};
    REQUIRE(glfwCreateWindowSurface(instance->handle, window.get(), nullptr, &surface) //
            == VK_SUCCESS);
    REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface,
                                                      &capabilities) //
            == VK_SUCCESS);
    auto on_return_1 = gsl::finally([instance = instance->handle, surface]() {
        vkDestroySurfaceKHR(instance, surface, nullptr); //
    });
    constexpr auto surface_format = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr auto surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    {
        bool suitable = false;
        REQUIRE(check_surface_format(physical_device, surface, surface_format, surface_color_space, suitable) ==
                VK_SUCCESS);
        REQUIRE(suitable);
    }
    constexpr auto present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        bool suitable = false;
        REQUIRE(check_present_mode(physical_device, surface, present_mode, suitable) == VK_SUCCESS);
        REQUIRE(suitable);
    }

    // virtual device & queue
    VkDevice device{};
    uint32_t graphics_index = 0, presentation_index = 0;
    REQUIRE(make_device(physical_device, surface, device, //
                        graphics_index, presentation_index) == VK_SUCCESS);
    auto on_return_2 = gsl::finally([&device]() { //
        vkDestroyDevice(device, nullptr);
    });
    VkQueue queues[2]{};
    vkGetDeviceQueue(device, graphics_index, 0, queues + 0);
    vkGetDeviceQueue(device, presentation_index, 0, queues + 1);
    REQUIRE(queues[0] != VK_NULL_HANDLE);
    REQUIRE(queues[1] != VK_NULL_HANDLE);

    // input data + shader + recording
    auto input = make_pipeline_input_2(device, meminfo, get_asset_dir());

    // graphics pipeline + presentation
    vulkan_renderpass_t renderpass{device, surface_format};
    REQUIRE(renderpass.handle);
    vulkan_pipeline_t pipeline{renderpass, capabilities.maxImageExtent, *input};
    REQUIRE(pipeline.handle);

    // recreate swapchain and presentation multiple times
    for (auto i = 0; i < 5; ++i) {
        auto swapchain = make_unique<vulkan_swapchain_t>(device, surface, capabilities, surface_format,
                                                         surface_color_space, present_mode);
        auto presentation = make_unique<vulkan_presentation_t>(device, renderpass.handle, swapchain->handle,
                                                               capabilities, surface_format);
        auto on_render_end = gsl::finally([device]() {
            // device must be idle before making swapchain recreation
            REQUIRE(vkDeviceWaitIdle(device) == VK_SUCCESS);
        });

        // command buffer
        vulkan_command_pool_t command_pool{device, graphics_index, presentation->num_images};
        for (auto i = 0u; i < presentation->num_images; ++i) {
            // record: command buffer + renderpass + pipeline
            recorder_t recorder{command_pool.buffers[i], renderpass.handle, presentation->framebuffers[i],
                                capabilities.maxImageExtent};
            vkCmdBindPipeline(recorder.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
            input->record(pipeline.handle, recorder.command_buffer);
        }

        // synchronization + timer
        stop_watch_t timer{};
        vulkan_semaphore_t semaphore_1{device}; // image ready
        vulkan_semaphore_t semaphore_2{device}; // rendering
        vulkan_fence_t fence{device};
        auto repeat = 120;
        while (!glfwWindowShouldClose(window.get()) && repeat--) {
            glfwPollEvents();
            /// render: select command buffer for current image and submit to GFX queue
            uint32_t image_index{};
            {
                if (auto ec = vkAcquireNextImageKHR(device, swapchain->handle, UINT64_MAX, semaphore_1.handle,
                                                    VK_NULL_HANDLE, &image_index))
                    FAIL(ec);
                if (auto ec = render_submit(queues[0], //
                                            gsl::make_span(command_pool.buffers.get() + image_index, 1), fence.handle,
                                            semaphore_1.handle, semaphore_2.handle))
                    FAIL(ec);
            }
            /// present: semaphore for synchronization and request presentation
            {
                // glfwSwapBuffers(window);
                if (auto ec = present_submit(queues[1], image_index, swapchain->handle, semaphore_2.handle))
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
}
