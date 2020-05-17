#define CATCH_CONFIG_FAST_COMPILE
#include <catch2/catch.hpp>
#include <filesystem>
#include <gsl/gsl>
#include <iostream>
#include <string_view>

//#include <vulkan/vulkan.hpp>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

namespace fs = std::filesystem;

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

    fprintf(stdout, "GLFW Extensions\n");
    for (auto i = 0u; i < count; ++i) {
        fprintf(stdout, "  %s\n", names[i]);
    }
}

/// @todo replace to Vulkan C++
class application_t {
  public:
    VkInstance instance{}; // vk::Instance instance{};
    VkApplicationInfo info{};
    std::unique_ptr<VkLayerProperties[]> layers{};
    VkAllocationCallbacks* allocator = nullptr;

  private:
    static VKAPI_ATTR VkBool32 VKAPI_CALL
    on_debug_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
                     VkDebugUtilsMessageTypeFlagsEXT mtype,
                     const VkDebugUtilsMessengerCallbackDataEXT* mdata, //
                     void* user_data) {
        switch (severity) {
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
        default:
            return VK_FALSE;
        }
    }

  public:
    explicit application_t(const char* id) noexcept(false) {
        info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        info.pApplicationName = id;
        info.applicationVersion = 0x00'01;
        info.pEngineName = "No Engine";
        info.engineVersion = VK_API_VERSION_1_2;
        info.apiVersion = VK_API_VERSION_1_2;

        VkInstanceCreateInfo request{};
        request.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        request.pApplicationInfo = &info;
        std::vector<gsl::czstring<>> extensions{};
        {
            uint32_t count = 0;
            const char** names = glfwGetRequiredInstanceExtensions(&count);
            for (auto i = 0u; i < count; ++i) {
                extensions.emplace_back(names[i]);
            }
            // extensions.emplace_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
            request.enabledExtensionCount =
                static_cast<uint32_t>(extensions.size());
            request.ppEnabledExtensionNames = extensions.data();
        }
        std::vector<gsl::czstring<>> names{};
        {
            uint32_t count = 0;
            REQUIRE(vkEnumerateInstanceLayerProperties(&count, nullptr) ==
                    VK_SUCCESS);
            layers = std::make_unique<VkLayerProperties[]>(count);
            REQUIRE(vkEnumerateInstanceLayerProperties(&count, layers.get()) ==
                    VK_SUCCESS);
            for (auto i = 0u; i < count; ++i) {
                using namespace std::string_view_literals;
                const auto& layer = layers[i];
                if ("VK_LAYER_KHRONOS_validation"sv == layer.layerName) {
                    names.emplace_back(layer.layerName);
                }
            }
        }
        if (names.size()) {
            /// @todo: change to parameter
            /// The validation layers will print debug messages to the standard output by default
            request.ppEnabledLayerNames = names.data();
            request.enabledLayerCount =
                gsl::narrow_cast<uint32_t>(names.size());
        }
        if (auto ec = vkCreateInstance(&request, allocator, &instance))
            FAIL(ec);
    }
    ~application_t() noexcept(false) {
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

using namespace std;

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
    application_t program{"test: physical device"};
    /// @todo use vk::PhysicalDevice
    uint32_t count = 0;
    REQUIRE(vkEnumeratePhysicalDevices(program.instance, &count, nullptr) ==
            VK_SUCCESS);
    REQUIRE(count > 0);
    auto devices = make_unique<VkPhysicalDevice[]>(count);
    REQUIRE(vkEnumeratePhysicalDevices(program.instance, &count,
                                       devices.get()) == VK_SUCCESS);
    SECTION("Get Device Properties") {
        fprintf(stdout, "Vilkan Physical Devices\n");
        for (auto i = 0u; i < count; ++i) {
            VkPhysicalDevice device = devices[i];
            REQUIRE(device != VK_NULL_HANDLE);
            VkPhysicalDeviceProperties props{};
            vkGetPhysicalDeviceProperties(device, &props);
        }
        SECTION("vkGetPhysicalDeviceFeatures") {
            for (auto i = 0u; i < count; ++i) {
                VkPhysicalDevice& device = devices[i];
                VkPhysicalDeviceFeatures features{};
                vkGetPhysicalDeviceFeatures(device, &features);
            }
        }
        SECTION("vkEnumerateDeviceExtensionProperties") {
            for (auto i = 0u; i < count; ++i) {
                VkPhysicalDevice device = devices[i];
                VkPhysicalDeviceProperties props{};
                vkGetPhysicalDeviceProperties(device, &props);
                fprintf(
                    stdout, "  %*s\n",
                    strnlen(props.deviceName, VK_MAX_PHYSICAL_DEVICE_NAME_SIZE),
                    props.deviceName);

                uint32_t num_extension = 0;
                REQUIRE(vkEnumerateDeviceExtensionProperties(
                            device, nullptr, &num_extension, nullptr) ==
                        VK_SUCCESS);
                auto extensions =
                    make_unique<VkExtensionProperties[]>(num_extension);
                REQUIRE(vkEnumerateDeviceExtensionProperties(
                            device, nullptr, &num_extension,
                            extensions.get()) == VK_SUCCESS);
                for_each(extensions.get(), extensions.get() + num_extension,
                         [](const VkExtensionProperties& ext) {
                             fprintf(stdout, "  - %*s\n",
                                     strnlen(ext.extensionName,
                                             VK_MAX_EXTENSION_NAME_SIZE),
                                     ext.extensionName);
                         });
            }
        }
    }
    SECTION("Get Queue Families") {
        const auto device = devices[0];
        uint32_t count = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, nullptr);
        REQUIRE(count > 0);
        auto qfs = make_unique<VkQueueFamilyProperties[]>(count);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &count, qfs.get());
        uint32_t qfi{};
        for (auto i = 0u; i < count; ++i) {
            const auto& family = qfs[i];
            REQUIRE(family.queueCount > 0);
            if (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                qfi = i;
            }
        }
        SECTION("Create Logical Device & Queue") {
            VkPhysicalDeviceFeatures features{};
            // ... feature request ...
            VkDeviceQueueCreateInfo queue_info{};
            queue_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info.queueFamilyIndex = qfi;
            queue_info.queueCount = 1;
            float priority = 0.012f;
            queue_info.pQueuePriorities = &priority;
            VkDeviceCreateInfo device_info{};
            device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            device_info.pQueueCreateInfos = &queue_info;
            device_info.queueCreateInfoCount = 1;
            device_info.pEnabledFeatures = &features;
            device_info.enabledExtensionCount = 0; // no extension / layer
            device_info.enabledLayerCount = 0;
            /// @todo: vk::Device
            /// The queues are automatically created along with the logical device
            VkDevice device{};
            REQUIRE(vkCreateDevice(devices[0], &device_info, nullptr,
                                   &device) == VK_SUCCESS);
            auto on_return_1 = gsl::finally([device]() { //
                vkDestroyDevice(device, nullptr);
            });
            REQUIRE(device);
            // create queue from it
            VkQueue queue{};
            vkGetDeviceQueue(device, queue_info.queueFamilyIndex, 0, &queue);
            REQUIRE(queue);
        }
    }
}

VkDevice make_device(VkPhysicalDevice card, VkSurfaceKHR surface,
                     VkDeviceQueueCreateInfo (&infos)[2],
                     float priority = 0.012f) {
    // gfx queue
    infos[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    infos[0].queueCount = 1;
    infos[0].pQueuePriorities = &priority;
    // present queue
    infos[1].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    infos[1].queueCount = 1;
    infos[1].pQueuePriorities = &priority;

    uint32_t count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(card, &count, nullptr);
    auto qfs = make_unique<VkQueueFamilyProperties[]>(count);
    vkGetPhysicalDeviceQueueFamilyProperties(card, &count, qfs.get());
    for (auto i = 0u; i < count; ++i) {
        if (qfs[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            infos[0].queueFamilyIndex = i;
        }
        VkBool32 present = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(card, i, surface, &present);
        if (present) {
            infos[1].queueFamilyIndex = i;
        }
        // both info is ready ?
        if (infos[0].queueFamilyIndex && infos[1].queueFamilyIndex)
            break;
    }
    VkDeviceCreateInfo req{};
    req.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    req.pQueueCreateInfos = infos;
    req.queueCreateInfoCount = 2;
    VkPhysicalDeviceFeatures features{};
    req.pEnabledFeatures = &features;
    {
        // Physical Device should have extension VK_KHR_SWAPCHAIN_EXTENSION_NAME
        const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        req.ppEnabledExtensionNames = &extension;
        req.enabledExtensionCount = 1;
    }
    // ...
    VkDevice device{};
    REQUIRE(vkCreateDevice(card, &req, nullptr, &device) == VK_SUCCESS);
    return device;
}

// https://www.glfw.org/docs/3.3/vulkan_guide.html
// https://vulkan-tutorial.com/Introduction
TEST_CASE("Vulkan + GLFW3", "[vulkan][glfw3]") {
    auto on_return = []() {
        REQUIRE(glfwInit());
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE,
                       GLFW_FALSE); // swapchain update not implemented
        return gsl::finally(&glfwTerminate);
    }();
    REQUIRE(glfwVulkanSupported());
    // instance / physical device
    application_t program{"test: glfw3"};
    VkPhysicalDevice devices[5]{};
    uint32_t count = 5;
    REQUIRE(vkEnumeratePhysicalDevices(program.instance, &count, devices) ==
            VK_SUCCESS);
    REQUIRE(count > 0); // for this TC, use 1st device

    // window / surface
    auto window = unique_ptr<GLFWwindow, void (*)(GLFWwindow*)>{
        glfwCreateWindow(160 * 6, 90 * 6, "Vulkan + GLFW3", nullptr, nullptr),
        &glfwDestroyWindow};
    REQUIRE(window != nullptr);
    VkSurfaceKHR surface{};
    REQUIRE(glfwCreateWindowSurface(program.instance, window.get(), nullptr,
                                    &surface) == VK_SUCCESS);
    auto on_return_2 = gsl::finally([instance = program.instance, surface]() {
        vkDestroySurfaceKHR(instance, surface,
                            nullptr); // GLFW won't destroy it
    });
    VkSurfaceCapabilitiesKHR capabilities{};
    REQUIRE(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
                devices[0], surface, &capabilities) == VK_SUCCESS);

    constexpr auto surface_format = VK_FORMAT_B8G8R8A8_SRGB;
    constexpr auto surface_color_space = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    {
        uint32_t num_formats = 0;
        REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR(
                    devices[0], surface, &num_formats, nullptr) == VK_SUCCESS);
        auto formats = make_unique<VkSurfaceFormatKHR[]>(num_formats);
        REQUIRE(vkGetPhysicalDeviceSurfaceFormatsKHR(
                    devices[0], surface, &num_formats, formats.get()) ==
                VK_SUCCESS);
        bool suitable = false;
        for (auto i = 0u; i < num_formats; ++i) {
            const auto& format = formats[i];
            if (format.format == surface_format &&
                format.colorSpace == surface_color_space)
                suitable = true;
        }
        REQUIRE(suitable);
    }
    constexpr auto present_mode = VK_PRESENT_MODE_FIFO_KHR;
    {
        uint32_t num_modes = 0;
        REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    devices[0], surface, &num_modes, nullptr) == VK_SUCCESS);
        auto modes = make_unique<VkPresentModeKHR[]>(num_modes);
        REQUIRE(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    devices[0], surface, &num_modes, modes.get()) ==
                VK_SUCCESS);
        bool suitable = false;
        for (auto i = 0u; i < num_modes; ++i) {
            switch (modes[i]) {
            case present_mode:
                suitable = true;
            default:
                continue;
            }
        }
        REQUIRE(suitable);
    }
    VkDeviceQueueCreateInfo queue_info[2]{};
    VkDevice device = make_device(devices[0], surface, queue_info);
    REQUIRE(device);
    auto on_return_3 = gsl::finally([device]() { //
        vkDestroyDevice(device, nullptr);
    });

    SECTION("Pipeline") {
        const auto asset_dir = fs::path{ASSET_DIR};
        auto read_all(const fs::path& p, size_t& fsize)
            ->std::unique_ptr<std::byte[]>;

        VkShaderModuleCreateInfo request{};
        request.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;

        const auto vspath = asset_dir / "sample_vert.spv";
        REQUIRE(fs::exists(vspath));
        const auto vsbin = read_all(vspath, request.codeSize);
        request.pCode = reinterpret_cast<const uint32_t*>(vsbin.get());
        VkShaderModule vs{};
        REQUIRE(vkCreateShaderModule(device, &request, nullptr, &vs) ==
                VK_SUCCESS);
        auto on_return_1 = gsl::finally([device, shader = vs]() {
            vkDestroyShaderModule(device, shader, nullptr);
        });

        const auto fspath = asset_dir / "sample_frag.spv";
        REQUIRE(fs::exists(fspath));
        const auto fsbin = read_all(fspath, request.codeSize);
        request.pCode = reinterpret_cast<const uint32_t*>(fsbin.get());
        VkShaderModule fs{};
        REQUIRE(vkCreateShaderModule(device, &request, nullptr, &fs) ==
                VK_SUCCESS);
        auto on_return_2 = gsl::finally([device, shader = fs]() {
            vkDestroyShaderModule(device, shader, nullptr);
        });

        // shader stage creation
        VkPipelineShaderStageCreateInfo shader{};
        shader.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader.pSpecializationInfo = nullptr;
        // for vertex shader
        shader.stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader.module = vs;
        shader.pName = "main";
        // for fragment shader
        shader.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader.module = fs;
        shader.pName = "main";

        VkPipelineVertexInputStateCreateInfo vinput{};
        vinput.sType =
            VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        if (vinput.vertexBindingDescriptionCount) // Optional
            vinput.pVertexBindingDescriptions = nullptr;
        if (vinput.vertexAttributeDescriptionCount) // Optional
            vinput.pVertexAttributeDescriptions = nullptr;

        VkPipelineInputAssemblyStateCreateInfo assembly{};
        assembly.sType =
            VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        assembly.primitiveRestartEnable = VK_FALSE;

        VkPipelineViewportStateCreateInfo viewport{};
        viewport.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        VkViewport vp{};
        vp.x = vp.y = 0.0f;
        vp.width = static_cast<float>(capabilities.maxImageExtent.width);
        vp.height = static_cast<float>(capabilities.maxImageExtent.height);
        vp.minDepth = 0.0f;
        vp.maxDepth = 1.0f;
        VkRect2D scissor{
            .offset = {0, 0},
            .extent = capabilities.maxImageExtent,
        };
        viewport.viewportCount = 1;
        viewport.pViewports = &vp;
        viewport.scissorCount = 1;
        viewport.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType =
            VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f;          // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f;    // Optional

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType =
            VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f;          // Optional
        multisampling.pSampleMask = nullptr;            // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE;      // Optional

        VkPipelineColorBlendAttachmentState blender{};
        blender.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blender.blendEnable = VK_TRUE;
        blender.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        blender.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        blender.colorBlendOp = VK_BLEND_OP_ADD;
        blender.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        blender.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        blender.alphaBlendOp = VK_BLEND_OP_ADD;

        VkPipelineColorBlendStateCreateInfo blending{};
        blending.sType =
            VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        blending.logicOpEnable = VK_FALSE;
        blending.logicOp = VK_LOGIC_OP_COPY; // Optional
        blending.attachmentCount = 1;
        blending.pAttachments = &blender;

        VkPipelineLayout layout{};
        VkPipelineLayoutCreateInfo layout_info{};
        layout_info.sType =
            VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layout_info.setLayoutCount = 0;            // Optional
        layout_info.pSetLayouts = nullptr;         // Optional
        layout_info.pushConstantRangeCount = 0;    // Optional
        layout_info.pPushConstantRanges = nullptr; // Optional
        REQUIRE(vkCreatePipelineLayout(device, &layout_info, nullptr,
                                       &layout) == VK_SUCCESS);
        vkDestroyPipelineLayout(device, layout, nullptr);
    }
    SECTION("RenderPass") {
        VkAttachmentDescription colors{};
        colors.format = surface_format;
        colors.samples = VK_SAMPLE_COUNT_1_BIT;
        colors.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colors.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        // color/depth 
        colors.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colors.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        // stencil
        colors.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colors.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;

        VkAttachmentReference color_ref{};
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &color_ref;

        VkRenderPassCreateInfo request{};
        request.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        request.attachmentCount = 1;
        request.pAttachments = &colors;
        request.subpassCount = 1;
        request.pSubpasses = &subpass;

        VkRenderPass renderpass{};
        REQUIRE(vkCreateRenderPass(device, &request, nullptr, &renderpass) ==
                VK_SUCCESS);
        vkDestroyRenderPass(device, renderpass, nullptr);
    }
    /// @todo https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion

    // ...
    SECTION("VkSwapchainKHR") {
        VkQueue queues[2]{}; // gfx, present
        vkGetDeviceQueue(device, queue_info[0].queueFamilyIndex, 0, queues + 0);
        vkGetDeviceQueue(device, queue_info[1].queueFamilyIndex, 0, queues + 1);
        REQUIRE(queues[0] != VK_NULL_HANDLE);
        REQUIRE(queues[1] != VK_NULL_HANDLE);
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        {
            VkSwapchainCreateInfoKHR req{};
            req.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            req.surface = surface;
            req.minImageCount = capabilities.minImageCount + 1;
            req.imageFormat = surface_format;
            req.imageColorSpace = surface_color_space;
            req.imageExtent = capabilities.maxImageExtent;
            req.imageArrayLayers = 1;
            req.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
            req.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
            req.preTransform = capabilities.currentTransform; // rotation/flip
            req.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            req.presentMode = present_mode;
            req.clipped = VK_TRUE;
            req.oldSwapchain = swapchain; // must update swapchain if resized
            REQUIRE(vkCreateSwapchainKHR(device, &req, nullptr, &swapchain) ==
                    VK_SUCCESS);
        }
        REQUIRE(swapchain != VK_NULL_HANDLE);
        auto on_return_4 = gsl::finally([device, swapchain]() {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
        });

        uint32_t num_images = 0;
        REQUIRE(vkGetSwapchainImagesKHR(device, swapchain, &num_images,
                                        nullptr) == VK_SUCCESS);
        REQUIRE(num_images == capabilities.minImageCount + 1);
        auto images = make_unique<VkImage[]>(num_images);
        REQUIRE(vkGetSwapchainImagesKHR(device, swapchain, &num_images,
                                        images.get()) == VK_SUCCESS);
        auto image_views = make_unique<VkImageView[]>(num_images);
        for (auto i = 0u; i < num_images; ++i) {
            VkImageViewCreateInfo req{};
            req.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            req.image = images[i];
            req.viewType = VK_IMAGE_VIEW_TYPE_2D; // 1D, 2D, CUBE ...
            req.format = surface_format;
            req.components = {}; // VK_COMPONENT_SWIZZLE_IDENTITY == 0
            req.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            req.subresourceRange.baseMipLevel = 0;
            req.subresourceRange.levelCount = 1;
            req.subresourceRange.baseArrayLayer = 0;
            req.subresourceRange.layerCount = 1;
            if (auto ec = vkCreateImageView(device, &req, nullptr, //
                                            &image_views[i]))
                FAIL(ec);
        }
        auto on_return = gsl::finally(
            [device, count = num_images, views = image_views.get()]() {
                for (auto i = 0u; i < count; ++i)
                    vkDestroyImageView(device, views[i], nullptr);
            });
    }

    SECTION("Render Loop (GLFW)") {
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
}

auto open(const fs::path& p) -> std::unique_ptr<FILE, int (*)(FILE*)> {
    auto fpath = p.generic_wstring();
    FILE* fp{};
    if (auto ec = _wfopen_s(&fp, fpath.c_str(), L"rb"))
        FAIL(ec);
    return {fp, &fclose};
}

auto read(FILE* stream, size_t& rsz) -> std::unique_ptr<std::byte[]> {
    struct _stat64 info {};
    if (_fstat64(_fileno(stream), &info) != 0)
        throw std::system_error{errno, std::system_category(), "_fstat64"};
    //rsz = info.st_size;
    auto blob = std::make_unique<std::byte[]>(info.st_size);
    rsz = 0;
    while (!feof(stream)) {
        auto b = blob.get() + rsz;
        const auto sz = info.st_size - rsz;
        rsz += fread_s(b, sz, sizeof(std::byte), sz, stream);
        if (auto ec = ferror(stream))
            throw std::system_error{ec, std::system_category(), "fread_s"};
        if (rsz == info.st_size)
            break;
    }
    return blob;
}

auto read_all(const fs::path& p, size_t& fsize)
    -> std::unique_ptr<std::byte[]> {
    auto fin = open(p);
    return read(fin.get(), fsize);
}
