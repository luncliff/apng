#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>

#include <chrono>
#include <filesystem>
#include <gsl/gsl>
#include <iostream>
#include <string_view>
#include <thread>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace fs = std::filesystem;

auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)>;
auto read(FILE* stream, size_t& rsz) -> std::unique_ptr<std::byte[]>;
auto read_all(const fs::path& p, size_t& fsize) -> std::unique_ptr<std::byte[]>;
fs::path get_asset_dir() noexcept;

using namespace std;
#include "graphics.h"

TEST_CASE("GLFW Extensions", "[vulkan][glfw3]") {
    if (glfwInit() == GLFW_FALSE) {
        const char* message = nullptr;
        CAPTURE(glfwGetError(&message)); // get the recent error code
        FAIL(message);
    }
    auto on_return = gsl::finally(&glfwTerminate);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    uint32_t count = 0;
    const char** names = glfwGetRequiredInstanceExtensions(&count);
    REQUIRE(count > 0);

    fprintf(stdout, "GLFW Extensions - Vulkan Instance\n");
    for (auto i = 0u; i < count; ++i) {
        fprintf(stdout, "  %s\n", names[i]);
    }
}

TEST_CASE("Vulkan Extenstions", "[vulkan]") {
    const char* layer_name = nullptr;
    SECTION("no layer name") {
        layer_name = nullptr;
    }
    SECTION("VK_LAYER_KHRONOS_validation") {
        layer_name = "VK_LAYER_KHRONOS_validation";
    }
    uint32_t count = 0;
    REQUIRE(vkEnumerateInstanceExtensionProperties(layer_name, &count,
                                                   nullptr) == VK_SUCCESS);
    REQUIRE(count > 0);
    auto extensions = make_unique<VkExtensionProperties[]>(count);
    REQUIRE(vkEnumerateInstanceExtensionProperties(
                layer_name, &count, extensions.get()) == VK_SUCCESS);

    fprintf(stdout, "Vulkan Extensions: %s\n",
            layer_name ? layer_name : "(unspecified)");
    for_each(extensions.get(), extensions.get() + count,
             [](const VkExtensionProperties& prop) {
                 fprintf(stdout, "  %s\n", prop.extensionName);
             });
}

TEST_CASE("Vulkan Layers", "[vulkan]") {
    uint32_t count = 0;
    REQUIRE(vkEnumerateInstanceLayerProperties(&count, nullptr) == VK_SUCCESS);
    auto layers = make_unique<VkLayerProperties[]>(count);
    REQUIRE(vkEnumerateInstanceLayerProperties(&count, layers.get()) ==
            VK_SUCCESS);

    fprintf(stdout, "Vulkan Layers\n");
    for_each(layers.get(), layers.get() + count, //
             [](const VkLayerProperties& prop) {
                 fprintf(stdout, "  %s %8x\n", prop.layerName,
                         prop.specVersion);
             });
}

VkResult get_physical_device(VkInstance instance, VkPhysicalDevice& device) {
    uint32_t count = 1;
    switch (auto code = vkEnumeratePhysicalDevices(instance, &count, &device)) {
    case VK_INCOMPLETE:
        return VK_SUCCESS;
    default:
        return code;
    }
}

TEST_CASE("Vulkan PhysicalDevice", "[vulkan]") {
    vulkan_instance_t instance{"test: physical device", nullptr};
    VkPhysicalDevice device{};
    REQUIRE(get_physical_deivce(instance.handle, device) == VK_SUCCESS);
    REQUIRE(device != VK_NULL_HANDLE);

    SECTION("VkPhysicalDeviceProperties") {
        fprintf(stdout, "Vulkan Physical Devices\n");
        SECTION("vkGetPhysicalDeviceFeatures") {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(device, &props);
            fprintf(stdout, "  %*s\n",
                    strnlen(props.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE),
                    props.deviceName);
        }
        SECTION("vkEnumerateDeviceExtensionProperties") {
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(device, &props);
            uint32_t num_extension = 0;
            REQUIRE(vkEnumerateDeviceExtensionProperties(
                        device, nullptr, &num_extension, nullptr) ==
                    VK_SUCCESS);
            auto extensions =
                make_unique<VkExtensionProperties[]>(num_extension);
            REQUIRE(vkEnumerateDeviceExtensionProperties(
                        device, nullptr, &num_extension, extensions.get()) ==
                    VK_SUCCESS);
            for_each(extensions.get(), extensions.get() + num_extension,
                     [](const VkExtensionProperties& ext) {
                         fprintf(stdout, "  - %*s\n",
                                 strnlen(ext.extensionName,
                                         VK_MAX_EXTENSION_NAME_SIZE),
                                 ext.extensionName);
                     });
        }
    }
    SECTION("VkQueueFamilyProperties") {
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        REQUIRE(count > 0);
        auto properties = make_unique<VkQueueFamilyProperties[]>(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count,
                                                 properties.get());
        float priority = 0.010f;
        SECTION("raw") {
            VkDeviceQueueCreateInfo queue_info{};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex =
                get_graphics_queue_available(properties.get(), count);
            queue_info.queueCount = 1;
            queue_info.pQueuePriorities = &priority;
            VkDeviceCreateInfo device_info{};
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.enabledExtensionCount = 0;
            device_info.enabledLayerCount = 0;
            VkPhysicalDeviceFeatures features{};
            device_info.pEnabledFeatures = &features;
            device_info.queueCreateInfoCount = 1;
            device_info.pQueueCreateInfos = &queue_info;
            {
                VkDevice virtual_device{};
                REQUIRE(vkCreateDevice(device, &device_info, nullptr,
                                       &virtual_device) == VK_SUCCESS);
                REQUIRE(virtual_device != VK_NULL_HANDLE);
                auto on_return_1 = gsl::finally([virtual_device]() { //
                    vkDestroyDevice(virtual_device, nullptr);
                });
                // reference queue from it
                VkQueue queue{};
                vkGetDeviceQueue(virtual_device, queue_info.queueFamilyIndex, 0,
                                 &queue);
                REQUIRE(queue);
            }
        }
        SECTION("wrapper") {
            VkDevice virtual_device{};
            uint32_t queue_index = UINT16_MAX;
            REQUIRE(make_device(device, virtual_device, queue_index,
                                priority) == VK_SUCCESS);
            REQUIRE(virtual_device != VK_NULL_HANDLE);
            auto on_return_1 = gsl::finally([virtual_device]() { //
                vkDestroyDevice(virtual_device, nullptr);
            });
            // reference queue from it
            VkQueue queue{};
            vkGetDeviceQueue(virtual_device, queue_index, 0, &queue);
            REQUIRE(queue);
        }
    }
}

auto make_vulkan_surface(GLFWwindow* window, //
                         VkInstance instance, VkPhysicalDevice device,
                         VkSurfaceKHR& surface,
                         VkSurfaceCapabilitiesKHR& capabilities) {
    REQUIRE(glfwCreateWindowSurface(instance, window, nullptr, &surface) //
            == VK_SUCCESS);
    REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                                      &capabilities) //
            == VK_SUCCESS);
    // GLFW won't destroy it
    return gsl::finally(
        [=]() { vkDestroySurfaceKHR(instance, surface, nullptr); });
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
    VkResult record(VkPipeline pipeline) {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                          pipeline);
        constexpr auto num_vertices = 3;
        constexpr auto num_instance = 1;
        constexpr auto first_vertex = 0;
        constexpr auto first_instance = 0;
        vkCmdDraw(command_buffer,             //
                  num_vertices, num_instance, //
                  first_vertex, first_instance);
        return VK_SUCCESS;
    }
};

// https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
// https://www.glfw.org/docs/3.3/vulkan_guide.html
// https://vulkan-tutorial.com/Introduction
TEST_CASE("Vulkan + GLFWwindow", "[vulkan][glfw3]") {
    auto on_return = []() {
        REQUIRE(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        // swapchain update not implemented
        return gsl::finally(&glfwTerminate);
    }();
    REQUIRE(glfwVulkanSupported());
    // instance / physical device
    vulkan_instance_t instance{"test: glfw3",
                               glfwGetRequiredInstanceExtensions};
    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance.handle, physical_device) ==
            VK_SUCCESS);
    // window
    auto window = unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>{
        glfwCreateWindow(160 * 6, 90 * 6, "Vulkan + GLFWwindow", nullptr,
                         nullptr),
        &glfwDestroyWindow};
    REQUIRE(window != nullptr);
    // surface
    VkSurfaceKHR surface{};
    VkSurfaceCapabilitiesKHR capabilities{};
    auto on_return_2 = make_vulkan_surface(window.get(), //
                                           instance.handle, physical_device,
                                           surface, capabilities);
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
    // virtual device
    VkDevice device{};
    auto on_return_3 = gsl::finally([&device]() { //
        vkDestroyDevice(device, nullptr);
    });
    SECTION("GFX/Presentation") {
        // device & queue
        uint32_t graphics_index = 0, presentation_index = 0;
        REQUIRE(make_device(physical_device, surface, device, //
                            graphics_index, presentation_index) == VK_SUCCESS);
        VkQueue queues[2]{};
        vkGetDeviceQueue(device, graphics_index, 0, queues + 0);
        vkGetDeviceQueue(device, presentation_index, 0, queues + 1);
        REQUIRE(queues[0] != VK_NULL_HANDLE);
        REQUIRE(queues[1] != VK_NULL_HANDLE);
        // graphics pipeline + presentation
        vulkan_renderpass_t renderpass{device, surface_format};
        const auto asset_dir = get_asset_dir();
        vulkan_shader_module_t vert{device, asset_dir / "sample_vert.spv"};
        vulkan_shader_module_t frag{device, asset_dir / "sample_frag.spv"};
        vulkan_pipeline_t pipeline{renderpass, capabilities, //
                                   vert.handle, frag.handle};
        vulkan_swapchain_t swapchain{device, //
                                     surface,
                                     capabilities,
                                     surface_format, //
                                     surface_color_space,
                                     present_mode};
        vulkan_presentation_t presentation{device, renderpass.handle,
                                           swapchain.handle, capabilities,
                                           surface_format};
        // command buffer
        vulkan_command_pool_t command_pool{device, graphics_index,
                                           presentation.num_images};
        for (auto i = 0u; i < presentation.num_images; ++i) {
            // record: command buffer + renderpass + pipeline
            recorder_t r{command_pool.buffers[i], renderpass.handle,
                         presentation.framebuffers[i],
                         capabilities.maxImageExtent};
            r.record(pipeline.handle);
        }
        // synchronization + timer
        vulkan_semaphore_t semaphore_1{device}, semaphore_2{device};
        vulkan_fence_t fence{device};
        process_timer_t timer{};
        // game loop
        auto repeat = 120;
        while (!glfwWindowShouldClose(window.get()) && repeat--) {
            /// update
            glfwPollEvents();
            GLint w = -1, h = -1;
            glfwGetWindowSize(window.get(), &w, &h);
            /// render: select command buffer for current image
            ///         and submit to GFX queue
            uint32_t image_index{};
            {
                if (auto ec = vkAcquireNextImageKHR(
                        device, swapchain.handle, UINT64_MAX,
                        semaphore_1.handle, VK_NULL_HANDLE, &image_index))
                    FAIL(ec);
                VkSubmitInfo submit{};
                submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                VkSemaphore wait_group[] = {semaphore_1.handle};
                submit.pWaitSemaphores = wait_group;
                submit.waitSemaphoreCount = 1;
                const VkPipelineStageFlags stages[] = {
                    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
                submit.pWaitDstStageMask = stages;
                submit.pCommandBuffers =
                    addressof(command_pool.buffers[image_index]);
                submit.commandBufferCount = 1;
                VkSemaphore signal_group[] = {semaphore_2.handle};
                submit.pSignalSemaphores = signal_group;
                submit.signalSemaphoreCount = 1;
                if (auto ec = vkQueueSubmit(queues[0], 1, &submit, //
                                            fence.handle))
                    FAIL(ec);
            }
            /// present: semaphore for synchronization and request presentation
            ///          @todo glfwSwapBuffers(window.get());
            {
                VkPresentInfoKHR info{};
                info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                VkSemaphore wait_group[] = {semaphore_2.handle};
                info.pWaitSemaphores = wait_group;
                info.waitSemaphoreCount = 1;
                VkSwapchainKHR swapchains[] = {swapchain.handle};
                info.pSwapchains = swapchains;
                info.swapchainCount = 1;
                info.pImageIndices = &image_index;
                if (auto ec = vkQueuePresentKHR(queues[1], &info))
                    FAIL(ec);
                if (auto ec = vkQueueWaitIdle(queues[1]))
                    FAIL(ec);
            }
            VkFence fences[1]{fence.handle};
            uint64_t nano = UINT64_MAX;
            if (auto ec = vkWaitForFences(device, 1, fences, VK_TRUE, nano))
                FAIL(ec);
            if (auto ec = vkResetFences(device, 1, fences))
                FAIL(ec);
            sleep_for_fps(timer, 120);
        }
        // end of game loop
        REQUIRE(vkDeviceWaitIdle(device) == VK_SUCCESS);
    }
}
