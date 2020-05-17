#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>

#include <gsl/gsl>
#include <iostream>

#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

/// @todo replace to Vulkan C++
class program_t {
  public:
    VkInstance instance{}; // vk::Instance instance{};
    VkApplicationInfo info{};
    VkInstanceCreateInfo createInfo{};

  public:
    program_t() noexcept(false) {
        info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        info.pApplicationName = "Hello Triangle";
        info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        info.pEngineName = "No Engine";
        info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        info.apiVersion = VK_API_VERSION_1_2;

        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &info;
        uint32_t glfwExtensionCount = 0;
        const char** glfwExtensions = nullptr;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        createInfo.enabledExtensionCount = glfwExtensionCount;
        createInfo.ppEnabledExtensionNames = glfwExtensions;
        createInfo.enabledLayerCount = 0;
        REQUIRE(vkCreateInstance(&createInfo, nullptr, &instance) ==
                VK_SUCCESS);
    }
    ~program_t() noexcept(false) {
        vkDestroyInstance(instance, nullptr);
    }

  public:
    uint32_t update(uint32_t w, uint32_t h) {
        return EXIT_SUCCESS;
    }
    VkResult render(void*) {
        return VK_SUCCESS;
    }
};

TEST_CASE("Vulkan Extenstions", "[vulkan]") {

    uint32_t count = 0;
    REQUIRE(vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) ==
            VK_SUCCESS);
    REQUIRE(count > 0);

    auto extensions = std::make_unique<VkExtensionProperties[]>(count);
    REQUIRE(vkEnumerateInstanceExtensionProperties(
                nullptr, &count, extensions.get()) == VK_SUCCESS);

    fputs("Vulkan Extensions\n", stdout);
    std::for_each(extensions.get(), extensions.get() + count,
                  [](const VkExtensionProperties& prop) {
                      fprintf(stdout, "  %s\n", prop.extensionName);
                  });
}

// https://vulkan-tutorial.com/en/Drawing_a_triangle/Setup/Validation_layers
TEST_CASE("Vulkan + GLFW3", "[vulkan]") {
    auto on_return = []() {
        REQUIRE(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        return gsl::finally(&glfwTerminate);
    }();

    auto window = std::unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>{
        glfwCreateWindow(160 * 6, 90 * 6, "Vulkan + GLFW3", nullptr, nullptr),
        &glfwDestroyWindow};
    REQUIRE(window != nullptr);

    program_t program{};

    auto repeat = 120;
    while (!glfwWindowShouldClose(window.get()) && repeat--) {
        glfwPollEvents();
        GLint w = -1, h = -1;
        glfwGetWindowSize(window.get(), &w, &h);
        if (auto ec = program.update(static_cast<uint32_t>(w),
                                     static_cast<uint32_t>(h)))
            FAIL(ec);
        if (auto ec = program.render(nullptr))
            FAIL(ec);
        glfwSwapBuffers(window.get());
    }
}
