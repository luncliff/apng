// https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Conclusion
// https://www.glfw.org/docs/3.3/vulkan_guide.html
// https://vulkan-tutorial.com/Introduction

#include <catch2/catch.hpp>
#include <spdlog/spdlog.h>
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image.h>
#include <stb_image_write.h>
// #include <tiny_gltf.h>

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

VkResult read_image_data(VkDevice device, const VkPhysicalDeviceMemoryProperties& meminfo, //
                         VkExtent2D& extent, int& component,                               //
                         VkBuffer& buffer, VkDeviceMemory& memory,                         //
                         const fs::path& fpath) {
    auto stream = get_current_stream();
    auto fin = open(fpath);
    int width = 0, height = 0;
    component = STBI_rgb_alpha;
    auto blob = std::unique_ptr<void, void (*)(void*)>{stbi_load_from_file(fin.get(), //
                                                                           &width, &height, &component, component),
                                                       &stbi_image_free};
    if (blob == nullptr)
        throw std::runtime_error{stbi_failure_reason()};
    extent.width = width;
    extent.height = height;

    VkBufferCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    info.size = width * height * component;
    info.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (auto ec = vkCreateBuffer(device, &info, nullptr, &buffer))
        throw vulkan_exception_t{ec, "vkCreateBuffer"};

    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(device, buffer, &requirements);
    stream->info("VkBuffer:");
    stream->info(" - usage: {:b}", info.usage);
    stream->info(" - size: {}", requirements.size);
    stream->info(" - alignment: {}", requirements.alignment);
    {
        const auto desired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = requirements.size;
        for (auto i = 0u; i < meminfo.memoryTypeCount; ++i) {
            // match type?
            const auto typebit = (1 << i);
            if (requirements.memoryTypeBits & typebit) {
                // match prop?
                const auto mtype = meminfo.memoryTypes[i];
                if ((mtype.propertyFlags & desired) == desired)
                    info.memoryTypeIndex = i;
            }
        }
        if (auto ec = vkAllocateMemory(device, &info, nullptr, &memory))
            throw vulkan_exception_t{ec, "vkAllocateMemory"};
        stream->info("VkDeviceMemory:");
        stream->info(" - required: {}", requirements.size);
        stream->info(" - type_index: {}", info.memoryTypeIndex);
    }
    if (auto ec = vkBindBufferMemory(device, buffer, memory, 0))
        return ec;
    void* mapping = nullptr;
    if (auto ec = vkMapMemory(device, memory, 0, requirements.size, 0, &mapping))
        return ec;
    memcpy(mapping, blob.get(), requirements.size);
    vkUnmapMemory(device, memory);
    return VK_SUCCESS;
}

VkResult make_image(VkDevice device, const VkPhysicalDeviceMemoryProperties& meminfo, //
                    const VkExtent2D& image_extent, VkFormat image_format,            //
                    VkImage& image, VkDeviceMemory& memory) {
    auto stream = get_current_stream();
    VkImageCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    info.imageType = VK_IMAGE_TYPE_2D;
    info.extent = {image_extent.width, image_extent.height, 1};
    info.mipLevels = info.arrayLayers = 1;
    info.format = image_format;
    info.tiling = VK_IMAGE_TILING_OPTIMAL; // request exactly same size with buffer
    info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    // this is an image for read(sampling)
    info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    info.samples = VK_SAMPLE_COUNT_1_BIT;
    info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (auto ec = vkCreateImage(device, &info, nullptr, &image))
        return ec;
    VkMemoryRequirements requirements{};
    vkGetImageMemoryRequirements(device, image, &requirements);
    stream->info("VkImage:");
    stream->info(" - type: {}", "VK_IMAGE_TYPE_2D");
    stream->info(" - format: {}", info.format);
    stream->info(" - mipmap: {}", info.mipLevels);
    stream->info(" - usage: {:b}", info.usage);
    stream->info(" - size: {}", requirements.size);
    stream->info(" - alignment: {}", requirements.alignment);
    {
        VkMemoryAllocateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        info.allocationSize = requirements.size;
        const auto desired = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        for (auto i = 0u; i < meminfo.memoryTypeCount; ++i) {
            // match type?
            const auto typebit = (1 << i);
            if (requirements.memoryTypeBits & typebit) {
                // match prop?
                const auto mtype = meminfo.memoryTypes[i];
                if ((mtype.propertyFlags & desired) == desired)
                    info.memoryTypeIndex = i;
            }
        }
        if (auto ec = vkAllocateMemory(device, &info, nullptr, &memory))
            return ec;
        stream->info("VkDeviceMemory:");
        stream->info(" - required: {}", requirements.size);
        stream->info(" - type_index: {}", info.memoryTypeIndex);
    }
    return vkBindImageMemory(device, image, memory, 0);
}

TEST_CASE("create VkImage from image file", "[image]") {
    auto stream = get_current_stream();

    const char* layers[1]{"VK_LAYER_KHRONOS_validation"};
    vulkan_instance_t instance{"app1", gsl::make_span(layers, 1), {}};

    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance.handle, physical_device) == VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties meminfo{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &meminfo);

    VkDevice device{};
    uint32_t qfi = UINT32_MAX;
    REQUIRE(make_device(physical_device, device, qfi) == VK_SUCCESS);
    REQUIRE(qfi != UINT32_MAX);
    auto on_return_0 = gsl::finally([device]() {
        vkDestroyDevice(device, nullptr); //
    });

    VkBuffer buffers[1]{};
    VkImage images[1]{};
    VkDeviceMemory memories[2]{}; // 0 for buffer, 1 for image
    auto on_return_1 = gsl::finally([device, &buffers, &images, &memories]() {
        if (buffers[0])
            vkDestroyBuffer(device, buffers[0], nullptr);
        if (images[0])
            vkDestroyImage(device, images[0], nullptr);
        for (auto i : {0, 1})
            vkFreeMemory(device, memories[i], nullptr);
    });
    const auto fpath = get_asset_dir() / "image_1080_608.png";
    stream->info("file: {}", fpath.generic_u8string());
    VkExtent2D image_extent{};
    int component{};
    REQUIRE_NOTHROW(read_image_data(device, meminfo, image_extent, component, //
                                    buffers[0], memories[0], fpath) == VK_SUCCESS);
    REQUIRE(buffers[0]);
    REQUIRE(memories[0]);
    REQUIRE(component == 4);
    REQUIRE(image_extent.width == 1080);
    REQUIRE(image_extent.height == 608);
    REQUIRE(make_image(device, meminfo, image_extent, VK_FORMAT_R8G8B8A8_UNORM, //
                       images[0], memories[1]) == VK_SUCCESS);
}
