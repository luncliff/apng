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

TEST_CASE("Render Offscreen", "[vulkan]") {
    // instance / physical device
    const char* layers[1]{"VK_LAYER_KHRONOS_validation"};
    vulkan_instance_t instance{"Render Offscreen", gsl::make_span(layers, 1), {}};
    VkPhysicalDevice physical_device{};
    REQUIRE(get_physical_device(instance.handle, physical_device) == VK_SUCCESS);
    VkPhysicalDeviceMemoryProperties meminfo{};
    vkGetPhysicalDeviceMemoryProperties(physical_device, &meminfo);

    // virtual device & queue
    VkDevice device{};
    uint32_t index = 0;
    REQUIRE(make_device(physical_device, device, index) == VK_SUCCESS);
    auto on_return_2 = gsl::finally([&device]() { //
        vkDestroyDevice(device, nullptr);
    });
    VkQueue queues[1]{};
    vkGetDeviceQueue(device, index, 0, queues + 0);
    REQUIRE(queues[0] != VK_NULL_HANDLE);

    // input data + shader
    auto input = make_pipeline_input_1(device, meminfo, get_asset_dir());
    VkExtent2D image_extent{1000, 1000};

    // graphics pipeline + presentation
    constexpr auto surface_format = VK_FORMAT_B8G8R8A8_UNORM;
    vulkan_renderpass_t renderpass{device, surface_format};
    REQUIRE(renderpass.handle);
    vulkan_pipeline_t pipeline{renderpass, image_extent, *input};
    REQUIRE(pipeline.handle);

    const auto num_images = 2u;
    auto image_memories = make_unique<VkDeviceMemory[]>(num_images);
    auto images = make_unique<VkImage[]>(num_images);
    auto on_return_3 = gsl::finally([device,                                            //
                                     images = gsl::make_span(images.get(), num_images), //
                                     image_memories = gsl::make_span(image_memories.get(), num_images)]() {
        for (auto image : images)
            vkDestroyImage(device, image, nullptr);
        for (auto memory : image_memories)
            vkFreeMemory(device, memory, nullptr);
    });
    for (auto i = 0u; i < num_images; ++i) {
        VkImage& handle = images[i];
        VkDeviceMemory& memory = image_memories[i];

        VkImageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        info.imageType = VK_IMAGE_TYPE_2D;
        info.extent.width = image_extent.width;
        info.extent.height = image_extent.height;
        info.extent.depth = 1;
        info.mipLevels = 1;
        info.arrayLayers = 1;
        info.format = surface_format;
        // info.tiling = tiling;
        info.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
        info.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        info.samples = VK_SAMPLE_COUNT_1_BIT;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        REQUIRE(vkCreateImage(device, &info, nullptr, &handle) == VK_SUCCESS);
        VkMemoryRequirements requirements{};
        vkGetImageMemoryRequirements(device, handle, &requirements);
        VkMemoryAllocateInfo meminfo{};
        meminfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        meminfo.allocationSize = requirements.size;
        // meminfo.memoryTypeIndex;
        REQUIRE(vkAllocateMemory(device, &meminfo, nullptr, &memory) == VK_SUCCESS);
        REQUIRE(vkBindImageMemory(device, handle, memory, 0) == VK_SUCCESS);
    }
    auto image_views = make_unique<VkImageView[]>(num_images);
    auto on_return_4 = gsl::finally([device, image_views = gsl::make_span(image_views.get(), num_images)]() {
        for (auto view : image_views)
            vkDestroyImageView(device, view, nullptr);
    });
    for (auto i = 0u; i < num_images; ++i) {
        VkImageViewCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        info.image = images[i];
        info.viewType = VK_IMAGE_VIEW_TYPE_2D; // 1D, 2D, CUBE ...
        info.format = surface_format;
        info.components = {}; // VK_COMPONENT_SWIZZLE_IDENTITY == 0
        info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        info.subresourceRange.baseMipLevel = 0;
        info.subresourceRange.levelCount = 1;
        info.subresourceRange.baseArrayLayer = 0;
        info.subresourceRange.layerCount = 1;
        REQUIRE(vkCreateImageView(device, &info, nullptr, //
                                  &image_views[i]) == VK_SUCCESS);
    }
    auto framebuffers = make_unique<VkFramebuffer[]>(num_images);
    auto on_return_5 = gsl::finally([device, framebuffers = gsl::make_span(framebuffers.get(), num_images)]() {
        for (auto fb : framebuffers)
            vkDestroyFramebuffer(device, fb, nullptr);
    });
    for (auto i = 0u; i < num_images; ++i) {
        VkFramebufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        info.renderPass = renderpass.handle;
        VkImageView attachments[] = {image_views[i]};
        info.attachmentCount = 1;
        info.pAttachments = attachments;
        info.width = image_extent.width;
        info.height = image_extent.height;
        info.layers = 1;
        REQUIRE(vkCreateFramebuffer(device, &info, nullptr, &framebuffers[i]) == VK_SUCCESS);
    }

    // command buffer
    vulkan_command_pool_t command_pool{device, index, num_images};
    for (auto i = 0u; i < num_images; ++i) {
        recorder_t recorder{command_pool.buffers[i], renderpass.handle, framebuffers[i], image_extent};
        vkCmdBindPipeline(recorder.command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.handle);
        input->record(pipeline.handle, recorder.command_buffer);
    }
    // render + wait
    vulkan_fence_t fence{device};
    for (auto i = 0u; i < num_images; ++i) {
        REQUIRE(render_submit(queues[0],                                         //
                              gsl::make_span(command_pool.buffers.get() + i, 1), //
                              fence.handle, VK_NULL_HANDLE, VK_NULL_HANDLE) == VK_SUCCESS);
        // wait the fence for 1 sec
        uint64_t timeout = 1'000'000'000;
        REQUIRE(vkWaitForFences(fence.device, 1, &fence.handle, VK_TRUE, timeout) == VK_SUCCESS);
        REQUIRE(vkResetFences(fence.device, 1, &fence.handle) == VK_SUCCESS);
    }
    REQUIRE(vkDeviceWaitIdle(device) == VK_SUCCESS);
}

TEST_CASE("Render Surface", "[vulkan][glfw]") {
    // glfw init + window
    const auto title = "Render Surface";
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

    // input data + shader
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

        auto repeat = 120u;
        while (!glfwWindowShouldClose(window.get()) && repeat--) {
            glfwPollEvents();
            /// render: select command buffer for current image and submit to GFX queue
            uint32_t image_index{};
            {
                if (auto ec = vkAcquireNextImageKHR(device, swapchain->handle, UINT64_MAX, semaphore_1.handle,
                                                    VK_NULL_HANDLE, &image_index))
                    FAIL(ec);
                if (auto ec = render_submit(queues[0],                                                   //
                                            gsl::make_span(command_pool.buffers.get() + image_index, 1), //
                                            fence.handle, semaphore_1.handle, semaphore_2.handle))
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
