#include <glm/glm.hpp>
#include <memory>
#include <vector>

#include "vulkan_1.h"

using namespace std;

struct input1_t : vulkan_pipeline_input_t {
    struct input_unit_t final {
        glm::vec2 position{};
        glm::vec3 color{};
    };

    static constexpr auto binding_count = 1;

  public:
    const VkDevice device{};
    vector<input_unit_t> vertices{};
    VkPipelineVertexInputStateCreateInfo info{};
    VkVertexInputBindingDescription desc{};
    VkVertexInputAttributeDescription attrs[2]{};
    VkBuffer buffers[binding_count]{};
    VkDeviceSize offsets[binding_count]{};
    VkBufferCreateInfo buffer_info{};
    VkDeviceMemory memory{};
    vulkan_shader_module_t vert, frag;

  public:
    input1_t(VkDevice _device, const VkPhysicalDeviceMemoryProperties& props,
             const fs::path& shader_dir) noexcept(false)
        : device{_device}, vertices{{{0.0f, -0.5f}, {1, 0, 0}}, {{0.5f, 0.5f}, {0, 1, 0}}, {{-0.5f, 0.5f}, {0, 0, 1}}},
          vert{device, shader_dir / "sample_vert.spv"}, frag{device, shader_dir / "sample_frag.spv"} {
        // ...
        make_input_description(info, desc, attrs);
        if (auto ec = create_buffer(buffers[0], buffer_info))
            throw vulkan_exception_t{ec, "vkCreateBuffer"};
        // we need these flags to use `write_memory`
        const auto desired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if (auto ec = create_memory(buffers[0], memory, buffer_info, desired, props)) {
            vkDestroyBuffer(device, buffers[0], nullptr);
            throw vulkan_exception_t{ec, "vkAllocateMemory"};
        }
        if (auto ec = write_memory(buffers[0], memory)) {
            vkFreeMemory(device, memory, nullptr);
            vkDestroyBuffer(device, buffers[0], nullptr);
            throw vulkan_exception_t{ec, "vkBindBufferMemory || vkMapMemory"};
        }
    }
    ~input1_t() noexcept {
        vkFreeMemory(device, memory, nullptr);
        vkDestroyBuffer(device, buffers[0], nullptr);
    }

    void setup_shader_stage(VkPipelineShaderStageCreateInfo (&stage)[2]) noexcept(false) override {
        stage[0].sType = stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[0].pName = stage[1].pName = "main";
        // for vertex shader
        stage[0].pSpecializationInfo = nullptr;
        stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage[0].module = vert.handle;
        // for fragment shader
        stage[1].pSpecializationInfo = nullptr;
        stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage[1].module = frag.handle;
    }

    static void make_input_description(VkPipelineVertexInputStateCreateInfo& info,
                                       VkVertexInputBindingDescription& desc,
                                       VkVertexInputAttributeDescription (&attrs)[2]) noexcept {
        desc.binding = 0;
        desc.stride = sizeof(input_unit_t);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // per vertex input
        // layout(location = 0) in vec2 i_position;
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32_SFLOAT; // vec2
        attrs[0].offset = 0;
        // layout(location = 1) in vec3 i_color;
        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attrs[1].offset = sizeof(input_unit_t::position);
        // ...
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info.vertexBindingDescriptionCount = 1;
        info.pVertexBindingDescriptions = &desc;
        info.vertexAttributeDescriptionCount = 2;
        info.pVertexAttributeDescriptions = attrs;
    }

    void setup_vertex_input_state(VkPipelineVertexInputStateCreateInfo& _info) noexcept override {
        _info = this->info;
    }

    VkResult create_buffer(VkBuffer& buffer, VkBufferCreateInfo& info) const noexcept {
        return create_vertex_buffer(device, buffer, info, sizeof(input_unit_t) * vertices.size());
    }
    VkResult create_memory(VkBuffer buffer, VkDeviceMemory& memory, const VkBufferCreateInfo& buffer_info,
                           VkFlags desired, const VkPhysicalDeviceMemoryProperties& props) const noexcept {
        return allocate_memory(device, buffer, memory, buffer_info, desired, props);
    }
    VkResult write_memory(VkBuffer buffer, VkDeviceMemory memory) const noexcept {
        return ::initialize_memory(device, buffer, memory, vertices.data());
    }

    void record(VkPipeline pipeline, VkCommandBuffer command_buffer) noexcept override {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        constexpr auto first_binding = 0;
        vkCmdBindVertexBuffers(command_buffer, first_binding, binding_count, buffers, offsets);
        constexpr auto num_instance = 1;
        constexpr auto first_vertex = 0;
        constexpr auto first_instance = 0;
        vkCmdDraw(command_buffer, static_cast<uint32_t>(vertices.size()), num_instance, first_vertex, first_instance);
    }
};

auto make_pipeline_input_1(VkDevice device, const VkPhysicalDeviceMemoryProperties& props, const fs::path& shader_dir)
    -> unique_ptr<vulkan_pipeline_input_t> {
    return make_unique<input1_t>(device, props, shader_dir);
}

struct input2_t : vulkan_pipeline_input_t {
    struct input_unit_t final {
        glm::vec2 position{};
        glm::vec3 color{};
    };

  public:
    const VkDevice device{};
    vector<input_unit_t> vertices{};
    const vector<uint16_t> indices{};
    VkPipelineVertexInputStateCreateInfo info{};
    VkVertexInputBindingDescription desc{};
    VkVertexInputAttributeDescription attrs[2]{};
    VkBuffer buffers[2]{}; // vertices, indices
    VkDeviceMemory memories[2]{};
    VkDeviceSize offsets[1]{}; // offset - vertex buffer 0
    vulkan_shader_module_t vert, frag;

  public:
    input2_t(VkDevice _device, const fs::path& shader_dir) noexcept(false)
        : device{_device}, vertices{{{-0.5f, -0.5f}, {1, 0, 0}},
                                    {{0.5f, -0.5f}, {0, 1, 0}},
                                    {{0.5f, 0.5f}, {0, 0, 1}},
                                    {{-0.5f, 0.5f}, {1, 1, 1}}},
          indices{0, 1, 2, 2, 3, 0}, //
          vert{device, shader_dir / "sample_vert.spv"}, frag{device, shader_dir / "sample_frag.spv"} {
        // ...
        make_input_description(info, desc, attrs);
    }
    ~input2_t() noexcept {
        if (memories[1])
            vkFreeMemory(device, memories[1], nullptr);
        if (buffers[1])
            vkDestroyBuffer(device, buffers[1], nullptr);
        if (memories[0])
            vkFreeMemory(device, memories[0], nullptr);
        if (buffers[0])
            vkDestroyBuffer(device, buffers[0], nullptr);
    }

    void update(const VkPhysicalDeviceMemoryProperties& props) noexcept(false) {
        // we need these flags to use `write_memory`
        const auto desired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBufferCreateInfo buffer_info{};
        // vertices
        const uint32_t vidx = 0;
        const uint32_t vbufsize = sizeof(input_unit_t) * vertices.size();
        if (auto ec = create_vertex_buffer(device, buffers[vidx], //
                                           buffer_info, vbufsize))
            throw vulkan_exception_t{ec, "vkCreateBuffer"};
        if (auto ec = allocate_memory(device, buffers[vidx], memories[vidx], buffer_info, desired, props))
            throw vulkan_exception_t{ec, "vkAllocateMemory"};
        if (auto ec = initialize_memory(device, buffers[vidx], memories[vidx], vertices.data()))
            throw vulkan_exception_t{ec, "vkBindBufferMemory || vkMapMemory"};
        // indices
        const uint32_t iidx = 1;
        const uint32_t ibufsize = sizeof(uint16_t) * indices.size();
        if (auto ec = create_index_buffer(device, buffers[iidx], //
                                          buffer_info, ibufsize))
            throw vulkan_exception_t{ec, "vkCreateBuffer"};
        if (auto ec = allocate_memory(device, buffers[iidx], memories[iidx], buffer_info, desired, props))
            throw vulkan_exception_t{ec, "vkAllocateMemory"};
        if (auto ec = initialize_memory(device, buffers[iidx], memories[iidx], indices.data()))
            throw vulkan_exception_t{ec, "vkBindBufferMemory || vkMapMemory"};
    }

    void setup_shader_stage(VkPipelineShaderStageCreateInfo (&stage)[2]) noexcept(false) override {
        stage[0].sType = stage[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stage[0].pName = stage[1].pName = "main";
        // for vertex shader
        stage[0].pSpecializationInfo = nullptr;
        stage[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        stage[0].module = vert.handle;
        // for fragment shader
        stage[1].pSpecializationInfo = nullptr;
        stage[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        stage[1].module = frag.handle;
    }

    static void make_input_description(VkPipelineVertexInputStateCreateInfo& info,
                                       VkVertexInputBindingDescription& desc,
                                       VkVertexInputAttributeDescription (&attrs)[2]) noexcept {
        desc.binding = 0;
        desc.stride = sizeof(input_unit_t);
        desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // per vertex input
        // layout(location = 0) in vec2 i_position;
        attrs[0].binding = 0;
        attrs[0].location = 0;
        attrs[0].format = VK_FORMAT_R32G32_SFLOAT; // vec2
        attrs[0].offset = 0;
        // layout(location = 1) in vec3 i_color;
        attrs[1].binding = 0;
        attrs[1].location = 1;
        attrs[1].format = VK_FORMAT_R32G32B32_SFLOAT; // vec3
        attrs[1].offset = sizeof(input_unit_t::position);
        // ...
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        info.vertexBindingDescriptionCount = 1;
        info.pVertexBindingDescriptions = &desc;
        info.vertexAttributeDescriptionCount = 2;
        info.pVertexAttributeDescriptions = attrs;
    }

    void setup_vertex_input_state(VkPipelineVertexInputStateCreateInfo& _info) noexcept override {
        _info = this->info;
    }

    void record(VkPipeline pipeline, VkCommandBuffer command_buffer) noexcept override {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        constexpr auto first_binding = 0;
        constexpr auto binding_count = 1;
        vkCmdBindVertexBuffers(command_buffer, //
                               first_binding, binding_count, buffers, offsets);
        constexpr auto index_offset = 0;
        vkCmdBindIndexBuffer(command_buffer, //
                             buffers[1], index_offset, VK_INDEX_TYPE_UINT16);
        constexpr auto num_instance = 1;
        constexpr auto first_index = 0;
        constexpr auto vertex_offset = 0;
        constexpr auto first_instance = 0;
        vkCmdDrawIndexed(command_buffer, static_cast<uint32_t>(indices.size()), num_instance, first_index,
                         vertex_offset, first_instance);
    }
};

auto make_pipeline_input_2(VkDevice device, const VkPhysicalDeviceMemoryProperties& props, const fs::path& shader_dir)
    -> unique_ptr<vulkan_pipeline_input_t> {
    auto impl = make_unique<input2_t>(device, shader_dir);
    impl->update(props);
    return impl;
}
