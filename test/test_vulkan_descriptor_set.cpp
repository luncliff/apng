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
    stream->info(" - VkBuffer:");
    stream->info("   - usage: {:b}", info.usage);
    stream->info("   - size: {}", requirements.size);
    stream->info("   - alignment: {}", requirements.alignment);
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
        stream->info(" - VkDeviceMemory:");
        stream->info("   - required: {}", requirements.size);
        stream->info("   - type_index: {}", info.memoryTypeIndex);
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
    stream->info(" - VkImage:");
    stream->info("   - type: {}", "VK_IMAGE_TYPE_2D");
    stream->info("   - format: {}", info.format);
    stream->info("   - mipmap: {}", info.mipLevels);
    stream->info("   - usage: {:b}", info.usage);
    stream->info("   - size: {}", requirements.size);
    stream->info("   - alignment: {}", requirements.alignment);
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
        stream->info(" - VkDeviceMemory:");
        stream->info("   - required: {}", requirements.size);
        stream->info("   - type_index: {}", info.memoryTypeIndex);
    }
    return vkBindImageMemory(device, image, memory, 0);
}

TEST_CASE("create VkImage from image file", "[image]") {
    auto stream = get_current_stream();
    stream->info("VkImage from file:");

    const char* layers[1]{"VK_LAYER_KHRONOS_validation"};
    vulkan_instance_t instance{"app1", gsl::make_span(layers, 1), {}};

    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance.handle, physical_device) == VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties meminfo{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &meminfo);

    VkDevice device{};
    VkDeviceQueueCreateInfo qinfo{};
    REQUIRE(create_device(physical_device, device, qinfo) == VK_SUCCESS);
    REQUIRE(qinfo.queueFamilyIndex != UINT32_MAX);
    auto on_return_0 = gsl::finally([device]() {
        vkDestroyDevice(device, nullptr); //
    });
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, qinfo.queueFamilyIndex, 0, &queue);

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
    stream->info(" - file: {}", fpath.generic_u8string());
    VkFormat image_format = VK_FORMAT_R8G8B8A8_UNORM; // VK_FORMAT_R8G8B8A8_SRGB;
    VkExtent2D image_extent{};
    int component{};
    REQUIRE_NOTHROW(read_image_data(device, meminfo, image_extent, component, //
                                    buffers[0], memories[0], fpath) == VK_SUCCESS);
    REQUIRE(buffers[0]);
    REQUIRE(memories[0]);
    REQUIRE(component == 4);
    REQUIRE(image_extent.width == 1080);
    REQUIRE(image_extent.height == 608);
    REQUIRE(make_image(device, meminfo, image_extent, image_format, //
                       images[0], memories[1]) == VK_SUCCESS);

    SECTION("transfer multiple VkCommandBuffer 1 by 1") {
        vulkan_command_pool_t command_pool{device, qinfo.queueFamilyIndex, 3};
        // when using multiple command buffer and they are submitted at once,
        // theire execution may in out of order. submit 1 by 1
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        {
            // image layout transition before copy ( UNDEFINED -> TRANSFER_DST_OPTIMAL )
            auto command_buffer = command_pool.buffers[0];
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command_buffer;
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            REQUIRE(vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS);
            {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // image's current layout
                barrier.srcAccessMask = 0;
                barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // destination
                barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;     //  of copy(write access)
                barrier.image = images[0];
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0, barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0, barrier.subresourceRange.layerCount = 1;
                vkCmdPipelineBarrier(command_buffer, //
                                     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                     //
                                     0, nullptr, // memory
                                     0, nullptr, // buffer
                                     1, &barrier // image
                );
            }
            REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
            REQUIRE(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
        }
        {
            // copy buffer to image: ( BUFFER_TRANSFER_SRC -> IMAGE_LAYOUT_TRANSFER_DST )
            auto command_buffer = command_pool.buffers[1];
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command_buffer;
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            REQUIRE(vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS);
            {
                VkBufferImageCopy regions[1]{};
                regions[0].bufferOffset = 0; // from VkBuffer
                regions[0].bufferRowLength = 0;
                regions[0].bufferImageHeight = 0;
                regions[0].imageOffset = {0, 0, 0}; // to VkImage
                regions[0].imageExtent = {image_extent.width, image_extent.height, 1};
                regions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                regions[0].imageSubresource.mipLevel = 0;
                regions[0].imageSubresource.baseArrayLayer = 0;
                regions[0].imageSubresource.layerCount = 1;
                vkCmdCopyBufferToImage(command_buffer, //
                                       buffers[0], images[0],
                                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // see above layout
                                       1, regions);
            }
            REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
            REQUIRE(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
        }
        {
            // image layout transition after copy ( TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL )
            auto command_buffer = command_pool.buffers[2];
            submit.commandBufferCount = 1;
            submit.pCommandBuffers = &command_buffer;
            VkCommandBufferBeginInfo begin{};
            begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
            REQUIRE(vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS);
            {
                VkImageMemoryBarrier barrier{};
                barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // was copy destination
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // shader can read
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
                barrier.image = images[0];
                barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                barrier.subresourceRange.baseMipLevel = 0, barrier.subresourceRange.levelCount = 1;
                barrier.subresourceRange.baseArrayLayer = 0, barrier.subresourceRange.layerCount = 1;
                vkCmdPipelineBarrier(command_buffer, //
                                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                                     //
                                     0, nullptr, // memory
                                     0, nullptr, // buffer
                                     1, &barrier // image
                );
            }
            REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
            REQUIRE(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
        }
        REQUIRE(vkQueueWaitIdle(queue) == VK_SUCCESS);
    }
    SECTION("transfer in 1 VkCommandBuffer") {
        // combine to 1 command buffer in order and submit once
        vulkan_command_pool_t command_pool{device, qinfo.queueFamilyIndex, 3};
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        auto command_buffer = command_pool.buffers[0];
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command_buffer;
        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        REQUIRE(vkBeginCommandBuffer(command_buffer, &begin) == VK_SUCCESS);
        {
            // image layout transition before copy ( UNDEFINED -> TRANSFER_DST_OPTIMAL )
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED; // image's current layout
            barrier.srcAccessMask = 0;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // destination
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;     //  of copy(write access)
            barrier.image = images[0];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0, barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0, barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(command_buffer, //
                                 VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                                 //
                                 0, nullptr, // memory
                                 0, nullptr, // buffer
                                 1, &barrier // image
            );
        }
        {
            // copy buffer to image: ( BUFFER_TRANSFER_SRC -> IMAGE_LAYOUT_TRANSFER_DST )
            VkBufferImageCopy regions[1]{};
            regions[0].bufferOffset = 0; // from VkBuffer
            regions[0].bufferRowLength = 0;
            regions[0].bufferImageHeight = 0;
            regions[0].imageOffset = {0, 0, 0}; // to VkImage
            regions[0].imageExtent = {image_extent.width, image_extent.height, 1};
            regions[0].imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            regions[0].imageSubresource.mipLevel = 0;
            regions[0].imageSubresource.baseArrayLayer = 0;
            regions[0].imageSubresource.layerCount = 1;
            vkCmdCopyBufferToImage(command_buffer, //
                                   buffers[0], images[0],
                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // see above layout
                                   1, regions);
        }
        {
            // image layout transition after copy ( TRANSFER_DST_OPTIMAL -> SHADER_READ_ONLY_OPTIMAL )
            VkImageMemoryBarrier barrier{};
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL; // was copy destination
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL; // shader can read
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.image = images[0];
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = 0, barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0, barrier.subresourceRange.layerCount = 1;
            vkCmdPipelineBarrier(command_buffer, //
                                 VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                                 //
                                 0, nullptr, // memory
                                 0, nullptr, // buffer
                                 1, &barrier // image
            );
        }
        REQUIRE(vkEndCommandBuffer(command_buffer) == VK_SUCCESS);
        REQUIRE(vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE) == VK_SUCCESS);
        REQUIRE(vkQueueWaitIdle(queue) == VK_SUCCESS);
    }

    // create an image view and sampler
    VkImageView image_view = VK_NULL_HANDLE;
    {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = images[0];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D;
        info.format = image_format;
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        REQUIRE(vkCreateImageView(device, &info, nullptr, &image_view) == VK_SUCCESS);
    }
    auto on_return_2 = gsl::finally([device, image_view]() { vkDestroyImageView(device, image_view, nullptr); });

    VkSampler sampler = VK_NULL_HANDLE;
    {
        VkSamplerCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        info.magFilter = VK_FILTER_LINEAR;
        info.minFilter = VK_FILTER_LINEAR;
        info.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
        info.anisotropyEnable = VK_FALSE;
        info.maxAnisotropy = 1;
        info.borderColor = VK_BORDER_COLOR_INT_TRANSPARENT_BLACK;
        // info.unnormalizedCoordinates = VK_TRUE;  // [0, width)
        info.unnormalizedCoordinates = VK_FALSE; // [0, 1)
        info.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        info.mipLodBias = 0;
        info.minLod = info.maxLod = 0;
        REQUIRE(vkCreateSampler(device, &info, nullptr, &sampler) == VK_SUCCESS);
    }
    auto on_return_3 = gsl::finally([device, sampler]() { vkDestroySampler(device, sampler, nullptr); });
    // ...
}
