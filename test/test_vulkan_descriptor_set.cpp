// https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
// https://www.glfw.org/docs/3.3/vulkan_guide.html
// https://vulkan-tutorial.com/Introduction

#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <tiny_gltf.h>

#include "vulkan_1.h"

using namespace std;

fs::path get_asset_dir() noexcept;
auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;

auto open_glfw() -> gsl::final_action<void (*)()>;

auto make_vulkan_instance_glfw(gsl::czstring<> name) -> vulkan_instance_t;

auto create_window_glfw(gsl::czstring<> name) noexcept -> unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>;

class vulkan_surface_owner_t final {
  public:
    unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> impl;
    VkInstance instance;
    VkSurfaceKHR handle;
    VkSurfaceCapabilitiesKHR capabilities;

  public:
    vulkan_surface_owner_t(gsl::czstring<> _title, //
                           VkInstance _instance, VkPhysicalDevice _device) noexcept(false);
    ~vulkan_surface_owner_t() noexcept;
};

TEST_CASE("descriptor set", "[vulkan][glfw]") {
    auto stream = get_current_stream();

    // instance / physical device
    auto glfw = open_glfw();
    auto instance = make_vulkan_instance_glfw("instance0");
    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance.handle, physical_device) == VK_SUCCESS);

    // surface
    vulkan_surface_owner_t surface{"window0", instance.handle, physical_device};
    const VkFormat surface_format = VK_FORMAT_B8G8R8A8_UNORM;
    const VkColorSpaceKHR surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    const VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    REQUIRE(check_support(physical_device, surface.handle, surface_format, surface_color_space, present_mode) ==
            VK_SUCCESS);

    // virtual device & queue
    VkDevice device{};
    VkDeviceQueueCreateInfo qinfos[2]{};
    REQUIRE(create_device(physical_device, &surface.handle, 1, device, qinfos) == VK_SUCCESS);
    auto on_return_0 = gsl::finally([&device]() { //
        vkDestroyDevice(device, nullptr);
    });
    VkQueue queues[2]{};
    vkGetDeviceQueue(device, qinfos[0].queueFamilyIndex, qinfos[0].queueCount - 1, queues + 0);
    vkGetDeviceQueue(device, qinfos[1].queueFamilyIndex, qinfos[1].queueCount - 1, queues + 1);

    // input data + shader
    VkPhysicalDeviceMemoryProperties meminfo{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &meminfo);
    auto input = make_pipeline_input_3(device, meminfo, get_asset_dir());

    // graphics pipeline + presentation
    vulkan_renderpass_t renderpass{device, surface_format};
    vulkan_pipeline_t pipeline{device, renderpass.handle, surface.capabilities.maxImageExtent, *input};

    auto swapchain = make_unique<vulkan_swapchain_t>(device, surface.handle, surface.capabilities, //
                                                     surface_format, surface_color_space, present_mode);
    auto presentation = make_unique<vulkan_presentation_t>(device, renderpass.handle, //
                                                           swapchain->handle, surface.capabilities, surface_format);
    auto on_render_end = gsl::finally([device]() {
        // device must be idle before making swapchain recreation
        REQUIRE(vkDeviceWaitIdle(device) == VK_SUCCESS);
    });

    // command buffer
    //  recording will be performed in 'update' steps
    vulkan_command_pool_t command_pool{device, qinfos[0].queueFamilyIndex, presentation->num_images};

    // synchronization + timer
    stop_watch_t timer{};
    vulkan_semaphore_t semaphore_1{device}; // image ready
    vulkan_semaphore_t semaphore_2{device}; // rendering
    vulkan_fence_t fence{device};

    auto repeat = 120u;
    GLFWwindow* window = surface.impl.get();
    while (!glfwWindowShouldClose(window) && repeat--) {
        /// update
        glfwPollEvents();
        if (auto ec = input->update())
            FAIL(ec);

        /// render: select command buffer for current image and submit to GFX queue
        uint32_t idx{};
        {
            if (auto ec = vkAcquireNextImageKHR(device, swapchain->handle, UINT64_MAX, //
                                                semaphore_1.handle, VK_NULL_HANDLE, &idx))
                FAIL(ec);
            // record + submit
            {
                vulkan_command_recorder_t recorder{command_pool.buffers[idx], renderpass.handle,
                                                   presentation->framebuffers[idx],
                                                   surface.capabilities.maxImageExtent};
                vkCmdBindPipeline(recorder.commands, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
                input->record(recorder.commands, pipeline.handle, pipeline.layout);
            }
            if (auto ec = render_submit(queues[0],                                           //
                                        gsl::make_span(command_pool.buffers.get() + idx, 1), //
                                        fence.handle, semaphore_1.handle, semaphore_2.handle))
                FAIL(ec);
        }
        /// present: semaphore for synchronization and request presentation
        {
            // glfwSwapBuffers(window);
            if (auto ec = present_submit(queues[1], idx, swapchain->handle, semaphore_2.handle))
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
