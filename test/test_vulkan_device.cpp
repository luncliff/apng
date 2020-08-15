#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>

#include "vulkan_1.h"

#if __has_include(<GLFW/glfw3.h>)
#include <GLFW/glfw3.h>
#endif

auto get_current_stream() noexcept -> std::shared_ptr<spdlog::logger>;

TEST_CASE("VkInstance", "[vulkan]") {
    std::initializer_list<gsl::czstring<>> layers{"VK_LAYER_KHRONOS_validation"};
    std::initializer_list<gsl::czstring<>> extensions{"VK_KHR_surface"};
    vulkan_instance_t instance{__func__, //
                               gsl::make_span(layers.begin(), layers.end()),
                               gsl::make_span(extensions.begin(), extensions.end())};
    REQUIRE(instance.handle);
    REQUIRE(instance.info.apiVersion == VK_API_VERSION_1_2);
}

#if __has_include(<GLFW/glfw3.h>)
TEST_CASE("VkInstance with GLFW", "[vulkan][glfw]") {
    if (glfwInit() == GLFW_FALSE) {
        const char* message = nullptr;
        CAPTURE(glfwGetError(&message)); // get the recent error code
        FAIL(message);
    }
    auto on_return = gsl::finally(&glfwTerminate);

    const char* layers[1]{"VK_LAYER_KHRONOS_validation"};
    uint32_t count = 0;
    const char** extensions = glfwGetRequiredInstanceExtensions(&count);
    REQUIRE(extensions);
    REQUIRE(count > 0);

    vulkan_instance_t instance{__func__, gsl::make_span(layers, 1), gsl::make_span(extensions, count)};
    REQUIRE(instance.handle);
    REQUIRE(instance.info.apiVersion == VK_API_VERSION_1_2);
}
#endif

TEST_CASE("VkPhysicalDevice", "[vulkan]") {
    auto stream = get_current_stream();
    vulkan_instance_t instance{__func__, {}, {}};

    uint32_t num_devices = 0;
    REQUIRE(vkEnumeratePhysicalDevices(instance.handle, &num_devices, nullptr) == VK_SUCCESS);
    REQUIRE(num_devices > 0);
    auto devices = std::make_unique<VkPhysicalDevice[]>(num_devices);
    REQUIRE(vkEnumeratePhysicalDevices(instance.handle, &num_devices, devices.get()) == VK_SUCCESS);

    for (VkPhysicalDevice device : gsl::make_span(devices.get(), num_devices)) {
        REQUIRE(device != VK_NULL_HANDLE);
        VkPhysicalDeviceProperties props{};
        vkGetPhysicalDeviceProperties(device, &props);
        stream->info("physical_device:");
        stream->info(" - name: {}", props.deviceName);
        stream->info(" - api: {:x}", props.apiVersion);
        stream->info(" - driver: {:x}", props.driverVersion);

        const VkPhysicalDeviceLimits& limits = props.limits;
        stream->info(" - limits:");
        stream->info("   - max_bound_descriptor_set: {}", limits.maxBoundDescriptorSets);
        stream->info("   - max_descriptor_set_input_attachment: {}", limits.maxDescriptorSetInputAttachments);
        stream->info("   - max_color_attachment: {}", limits.maxColorAttachments);
        stream->info("   - framebuffer_color_sample_count: {}", limits.framebufferColorSampleCounts);
        stream->info("   - framebuffer_depth_sample_count: {}", limits.framebufferDepthSampleCounts);
        stream->info("   - framebuffer_stencil_sample_count: {}", limits.framebufferStencilSampleCounts);
        stream->info("   - max_framebuffer_width: {}", limits.maxFramebufferWidth);
        stream->info("   - max_framebuffer_height: {}", limits.maxFramebufferWidth);
        stream->info("   - max_framebuffer_layers: {}", limits.maxFramebufferLayers);
        stream->info("   - max_clip_distance: {}", limits.maxClipDistances);

        VkPhysicalDeviceMemoryProperties memory{};
        vkGetPhysicalDeviceMemoryProperties(device, &memory);
        stream->info(" - memory:");
        stream->info("   - types: {:b}", memory.memoryTypeCount);
        stream->info("   - heaps: {:b}", memory.memoryHeapCount);

        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        auto properties = std::make_unique<VkQueueFamilyProperties[]>(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, properties.get());
        REQUIRE(count > 0);
        for (auto i = 0u; i < count; ++i) {
            stream->info(" - queue_family:");
            const VkQueueFamilyProperties& prop = properties[i];
            // stream->info("   - count: {}", prop.queueCount);
            if (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                stream->info("   - {}", "GRAPHICS");
            if (prop.queueFlags & VK_QUEUE_COMPUTE_BIT)
                stream->info("   - {}", "COMPUTE");
            if (prop.queueFlags & VK_QUEUE_TRANSFER_BIT)
                stream->info("   - {}", "TRANSFER");
            if (prop.queueFlags & VK_QUEUE_SPARSE_BINDING_BIT)
                stream->info("   - {}", "SPARSE_BINDING");
            if (prop.queueFlags & VK_QUEUE_PROTECTED_BIT)
                stream->info("   - {}", "PROTECTED");
        }

        REQUIRE(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) == VK_SUCCESS);
        auto extensions = std::make_unique<VkExtensionProperties[]>(count);
        REQUIRE(vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.get()) == VK_SUCCESS);
        stream->info(" - extensions:");
        for (auto i = 0u; i < count; ++i)
            stream->info("   - {}", extensions[i].extensionName);
    }
}

TEST_CASE("VkDevice", "[vulkan]") {
    const char* layers[1]{"VK_LAYER_KHRONOS_validation"};
    vulkan_instance_t instance{__func__, gsl::make_span(layers, 1), {}};

    VkPhysicalDevice gpu{};
    REQUIRE(get_physical_device(instance.handle, gpu) == VK_SUCCESS);
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(gpu, &props);

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, nullptr);
    auto properties = std::make_unique<VkQueueFamilyProperties[]>(count);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &count, properties.get());

    VkDeviceQueueCreateInfo queues[2]{};
    float priority = 0.012f;
    queues[0].sType = queues[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queues[0].pQueuePriorities = queues[1].pQueuePriorities = &priority;
    queues[0].queueFamilyIndex = queues[1].queueFamilyIndex = UINT32_MAX;
    queues[0].queueCount = queues[1].queueCount = 1;
    for (auto i = 0u; i < count; ++i) {
        const auto& prop = properties[i];
        if (prop.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            queues[0].queueFamilyIndex = i;
            continue;
        }
        if (prop.queueFlags & VK_QUEUE_COMPUTE_BIT) {
            queues[1].queueFamilyIndex = i;
            continue;
        }
    }
    REQUIRE(queues[0].queueFamilyIndex < count);

    VkDeviceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    info.pQueueCreateInfos = queues;
    info.queueCreateInfoCount = 2;
    if (queues[1].queueFamilyIndex > count)
        info.queueCreateInfoCount -= 1;
    VkPhysicalDeviceFeatures features{};
    info.pEnabledFeatures = &features;

    VkDevice device = VK_NULL_HANDLE;
    REQUIRE(vkCreateDevice(gpu, &info, nullptr, &device) == VK_SUCCESS);
    auto on_return = gsl::finally([device]() { //
        vkDestroyDevice(device, nullptr);
    });
    VkQueue handles[2]{};
    vkGetDeviceQueue(device, queues[0].queueFamilyIndex, 0, handles + 0);
    REQUIRE(handles[0] != VK_NULL_HANDLE);
    if (queues[1].queueFamilyIndex < count) {
        vkGetDeviceQueue(device, queues[1].queueFamilyIndex, 0, handles + 1);
        REQUIRE(handles[1] != VK_NULL_HANDLE);
    }
}
