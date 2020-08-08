// https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
// https://www.glfw.org/docs/3.3/vulkan_guide.html
// https://vulkan-tutorial.com/Introduction

#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include "vulkan_1.h"

using namespace std;

auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;

auto create_window_glfw(gsl::czstring<> name) noexcept -> unique_ptr<GLFWwindow, void (*)(GLFWwindow*)> {
    REQUIRE(glfwInit());
    REQUIRE(glfwVulkanSupported());
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
    return {glfwCreateWindow(90 * 5, 90 * 5, name, nullptr, nullptr), [](GLFWwindow* window) {
                glfwDestroyWindow(window);
                glfwTerminate();
            }};
}

auto make_vulkan_instance(GLFWwindow*, gsl::czstring<> name) -> std::unique_ptr<vulkan_instance_t> {
    const char* layers[1]{"VK_LAYER_KHRONOS_validation"};
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    REQUIRE(extensions);
    REQUIRE(count > 0);
    return make_unique<vulkan_instance_t>(name,                      //
                                          gsl::make_span(layers, 1), //
                                          gsl::make_span(extensions, count));
}

auto make_vulkan_surface(GLFWwindow* window, VkInstance instance, VkPhysicalDevice device, //
                         VkSurfaceKHR& surface, VkSurfaceCapabilitiesKHR& capabilities) {
    REQUIRE(glfwCreateWindowSurface(instance, window, nullptr, &surface) //
            == VK_SUCCESS);
    REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                                      &capabilities) //
            == VK_SUCCESS);
    return gsl::finally([=]() { vkDestroySurfaceKHR(instance, surface, nullptr); });
}

TEST_CASE("VkSurfaceKHR + GLFWwindow", "[vulkan][surface][glfw]") {
    auto stream = get_current_stream();

    const auto title = "glfw3";
    auto window = create_window_glfw(title);
    REQUIRE(window);
    auto instance = make_vulkan_instance(window.get(), title);
    REQUIRE(instance->handle);

    VkPhysicalDevice device{};
    REQUIRE(get_physical_device(instance->handle, device) == VK_SUCCESS);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(device, &props);
    stream->info("surface:");
    stream->info(" - device: {}", props.deviceName);

    VkSurfaceKHR surface{};
    VkSurfaceCapabilitiesKHR capabilities{};
    auto on_return = make_vulkan_surface(window.get(), instance->handle, device, //
                                         surface, capabilities);
    REQUIRE(surface);
    stream->info(" - max_image: {}", capabilities.maxImageCount);
    stream->info(" - max_extent:");
    stream->info("   - width : {}", capabilities.maxImageExtent.width);
    stream->info("   - height: {}", capabilities.maxImageExtent.height);
    stream->info(" - transforms:");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
        stream->info("   - {}", "IDENTITY");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR)
        stream->info("   - {}", "ROTATE_90");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR)
        stream->info("   - {}", "ROTATE_180");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR)
        stream->info("   - {}", "ROTATE_270");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_BIT_KHR)
        stream->info("   - {}", "HORIZONTAL_MIRROR");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_90_BIT_KHR)
        stream->info("   - {}", "HORIZONTAL_MIRROR_ROTATE_90");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_180_BIT_KHR)
        stream->info("   - {}", "HORIZONTAL_MIRROR_ROTATE_180");
    if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_HORIZONTAL_MIRROR_ROTATE_270_BIT_KHR)
        stream->info("   - {}", "HORIZONTAL_MIRROR_ROTATE_270");
    stream->info(" - composite_alpha:");
    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
        stream->info("   - {}", "OPAQUE");
    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
        stream->info("   - {}", "PRE_MULTIPLIED");
    if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
        stream->info("   - {}", "POST_MULTIPLIED");

    uint32_t num_formats = 0;
    REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, nullptr) == VK_SUCCESS);
    auto formats = make_unique<VkSurfaceFormatKHR[]>(num_formats);
    REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &num_formats, formats.get()) == VK_SUCCESS);
    for (auto i = 0u; i < num_formats; ++i) {
        stream->info(" - format:");
        stream->info("   - flag: {}", formats[i].format);
        stream->info("   - color_space: {}", formats[i].colorSpace);
    }

    uint32_t num_modes = 0;
    REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &num_modes, nullptr) == VK_SUCCESS);
    auto modes = make_unique<VkPresentModeKHR[]>(num_modes);
    REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &num_modes, modes.get()) == VK_SUCCESS);
    stream->info(" - modes:");
    for (auto i = 0u; i < num_modes; ++i) {
        stream->info("   - {}", modes[i]);
    }
}

TEST_CASE("VkDevice + GLFWwindow", "[vulkan]") {
    const auto title = "glfw3";
    auto window = create_window_glfw(title);
    REQUIRE(window);
    auto instance = make_vulkan_instance(window.get(), title);
    REQUIRE(instance->handle);

    VkPhysicalDevice gpu{};
    REQUIRE(get_physical_device(instance->handle, gpu) == VK_SUCCESS);
    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
    auto properties = std::make_unique<VkQueueFamilyProperties[]>(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, properties.get());
    REQUIRE(count >= 1);

    VkSurfaceKHR surface{};
    VkSurfaceCapabilitiesKHR capabilities{};
    auto on_return = make_vulkan_surface(window.get(), instance->handle, gpu, //
                                         surface, capabilities);

    VkDeviceQueueCreateInfo queues[2]{};
    float priority = 0.123456f;
    queues[0].sType = queues[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queues[0].pQueuePriorities = queues[1].pQueuePriorities = &priority;
    queues[0].queueFamilyIndex = queues[1].queueFamilyIndex = UINT32_MAX;
    for (auto i = 0u; i < count; ++i) {
        // graphics queue
        if (queues[0].queueFamilyIndex == UINT32_MAX) {
            const auto& prop = properties[i];
            if (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                queues[0].queueFamilyIndex = i;
                queues[0].queueCount = 1;
                continue;
            }
        }
        // present queue
        //   index should be unique. if we have another, advance
        if (queues[1].queueFamilyIndex == UINT32_MAX) {
            VkBool32 support = false;
            REQUIRE(vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &support) == VK_SUCCESS);
            if (support) {
                queues[1].queueFamilyIndex = i;
                queues[1].queueCount = 1;
            }
        }
    }
    REQUIRE(queues[0].queueFamilyIndex < count); // selected?
    REQUIRE(queues[1].queueFamilyIndex < count); // selected?

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.queueCreateInfoCount = 2;
    info.pQueueCreateInfos = queues;
    const char* extension_names[1]{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    info.ppEnabledExtensionNames = extension_names;
    info.enabledExtensionCount = 1;
    VkPhysicalDeviceFeatures features{};
    info.pEnabledFeatures = &features;

    VkDevice device = VK_NULL_HANDLE;
    REQUIRE(vkCreateDevice(gpu, &info, nullptr, &device) == VK_SUCCESS);
    auto on_return_2 = gsl::finally([device]() { //
        vkDestroyDevice(device, nullptr);
    });
    VkQueue handles[2]{};
    vkGetDeviceQueue(device, queues[0].queueFamilyIndex, 0, handles + 0);
    REQUIRE(handles[0] != VK_NULL_HANDLE);
    vkGetDeviceQueue(device, queues[1].queueFamilyIndex, 0, handles + 1);
    REQUIRE(handles[1] != VK_NULL_HANDLE);
}
