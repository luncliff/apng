#define CATCH_CONFIG_MAIN
#include <catch2/catch.hpp>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include "graphics.h"

using namespace std;

fs::path get_asset_dir() noexcept {
#if defined(ASSET_DIR)
    return {ASSET_DIR};
#else
    return fs::current_path();
#endif
}

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

TEST_CASE("Vulkan PhysicalDevice", "[vulkan]") {
    vulkan_instance_t instance{"test: physical device", nullptr};
    VkPhysicalDevice device{};
    REQUIRE(get_physical_device(instance.handle, device) == VK_SUCCESS);
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
