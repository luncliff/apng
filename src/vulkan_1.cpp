#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
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

    void setup_vertex_input_state(VkPipelineVertexInputStateCreateInfo& info) noexcept override {
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

    VkResult make_pipeline_layout(VkDevice device, VkPipelineLayout& layout) noexcept override {
        return vulkan_pipeline_t::make_pipeline_layout(device, layout);
    }

    VkResult create_buffer(VkBuffer& buffer, VkBufferCreateInfo& info) const noexcept {
        return create_vertex_buffer(device, buffer, info, sizeof(input_unit_t) * vertices.size());
    }
    VkResult create_memory(VkBuffer buffer, VkDeviceMemory& memory, const VkBufferCreateInfo& buffer_info,
                           VkFlags desired, const VkPhysicalDeviceMemoryProperties& props) const noexcept {
        return allocate_memory(device, buffer, memory, buffer_info, desired, props);
    }
    VkResult write_memory(VkBuffer buffer, VkDeviceMemory memory) const noexcept {
        constexpr auto offset = 0;
        if (auto ec = vkBindBufferMemory(device, buffer, memory, offset))
            return ec;
        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffer, &requirements);
        return update_memory(device, memory, requirements, vertices.data(), offset);
    }

    void record(VkCommandBuffer command_buffer, VkPipeline pipeline, VkPipelineLayout) noexcept override {
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
    VkVertexInputBindingDescription desc{};
    VkVertexInputAttributeDescription attrs[2]{};
    VkBuffer buffers[2]{}; // vertices, indices
    VkDeviceMemory memories[2]{};
    VkDeviceSize offsets[1]{}; // offset - vertex buffer 0
    vulkan_shader_module_t vert, frag;

  public:
    input2_t(VkDevice _device, const fs::path& shader_dir) noexcept(false)
        : device{_device}, vertices{{{-0.8f, -0.5f}, {1, 0, 0}},
                                    {{0.8f, -0.5f}, {0, 1, 0}},
                                    {{0.8f, 0.5f}, {0, 0, 1}},
                                    {{-0.8f, 0.5f}, {1, 1, 1}}},
          indices{0, 1, 2, 2, 3, 0},                    //
          vert{device, shader_dir / "sample_vert.spv"}, //
          frag{device, shader_dir / "sample_frag.spv"} {
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

    void allocate(const VkPhysicalDeviceMemoryProperties& props) noexcept(false) {
        // we need these flags to use `write_memory`
        const auto desired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBufferCreateInfo buffer_info{};
        VkMemoryRequirements requirements{};
        // vertices
        {
            const uint32_t vidx = 0;
            const uint32_t vbufsize = sizeof(input_unit_t) * vertices.size();
            if (auto ec = create_vertex_buffer(device, buffers[vidx], //
                                               buffer_info, vbufsize))
                throw vulkan_exception_t{ec, "vkCreateBuffer"};
            if (auto ec = allocate_memory(device, buffers[vidx], memories[vidx], buffer_info, desired, props))
                throw vulkan_exception_t{ec, "vkAllocateMemory"};
            if (auto ec = vkBindBufferMemory(device, buffers[vidx], memories[vidx], 0))
                throw vulkan_exception_t{ec, "vkBindBufferMemory"};
            vkGetBufferMemoryRequirements(device, buffers[vidx], &requirements);
            if (auto ec = update_memory(device, memories[vidx], requirements, vertices.data(), 0))
                throw vulkan_exception_t{ec, "vkMapMemory"};
        }
        // indices
        {
            const uint32_t iidx = 1;
            const uint32_t ibufsize = sizeof(uint16_t) * indices.size();
            if (auto ec = create_index_buffer(device, buffers[iidx], //
                                              buffer_info, ibufsize))
                throw vulkan_exception_t{ec, "vkCreateBuffer"};
            if (auto ec = allocate_memory(device, buffers[iidx], memories[iidx], buffer_info, desired, props))
                throw vulkan_exception_t{ec, "vkAllocateMemory"};
            if (auto ec = vkBindBufferMemory(device, buffers[iidx], memories[iidx], 0))
                throw vulkan_exception_t{ec, "vkBindBufferMemory"};
            vkGetBufferMemoryRequirements(device, buffers[iidx], &requirements);
            if (auto ec = update_memory(device, memories[iidx], requirements, indices.data(), 0))
                throw vulkan_exception_t{ec, "vkMapMemory"};
        }
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

    void setup_vertex_input_state(VkPipelineVertexInputStateCreateInfo& info) noexcept override {
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

    VkResult make_pipeline_layout(VkDevice device, VkPipelineLayout& layout) noexcept override {
        return vulkan_pipeline_t::make_pipeline_layout(device, layout);
    }

    void record(VkCommandBuffer command_buffer, VkPipeline pipeline, VkPipelineLayout) noexcept override {
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
    impl->allocate(props);
    return impl;
}

struct input3_t : vulkan_pipeline_input_t {
    struct input_unit_t final {
        glm::vec2 position{};
        glm::vec3 color{};
    };
    struct uniform_t final {
        glm::mat4 model;
        glm::mat4 view;
        glm::mat4 projection;
    };

  public:
    const VkDevice device{};

    VkDescriptorSetLayout descriptor_layout{};
    VkDescriptorPool descriptor_pool{};
    VkDescriptorSet descriptors[1]{};

    VkVertexInputBindingDescription desc{};
    VkVertexInputAttributeDescription attrs[2]{};

    VkBuffer buffers[3]{}; // uniform, vertices, indices
    VkDeviceMemory memories[3]{};
    VkDeviceSize offsets[1]{}; // offset - vertex buffer 0
    vulkan_shader_module_t vert, frag;

  public:
    input3_t(VkDevice _device, const fs::path& shader_dir) noexcept(false)
        : device{_device},                                      //
          vert{device, shader_dir / "sample_uniform_vert.spv"}, //
          frag{device, shader_dir / "bypass_frag.spv"} {
        {
            VkDescriptorSetLayoutBinding binding{};
            binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            binding.descriptorCount = 1;
            binding.binding = 0;
            binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
            VkDescriptorSetLayoutCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            info.bindingCount = 1;
            info.pBindings = &binding;
            if (auto ec = vkCreateDescriptorSetLayout(device, &info, nullptr, &descriptor_layout))
                throw vulkan_exception_t{ec, "vkCreateDescriptorSetLayout"};
        }
        {
            VkDescriptorPoolSize requirement{};
            requirement.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            requirement.descriptorCount = 1;
            VkDescriptorPoolCreateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            info.poolSizeCount = 1;
            info.pPoolSizes = &requirement;
            info.maxSets = 1; // use only 1
            if (auto ec = vkCreateDescriptorPool(device, &info, nullptr, &descriptor_pool)) {
                vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
                throw vulkan_exception_t{ec, "vkCreateDescriptorPool"};
            }
        }
        {
            VkDescriptorSetAllocateInfo info{};
            info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            info.descriptorPool = descriptor_pool;
            info.descriptorSetCount = 1;
            info.pSetLayouts = &descriptor_layout;
            if (auto ec = vkAllocateDescriptorSets(device, &info, descriptors + 0)) {
                vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
                vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
                throw vulkan_exception_t{ec, "vkAllocateDescriptorSets"};
            }
        }
    }
    ~input3_t() noexcept {
        vkDestroyDescriptorPool(device, descriptor_pool, nullptr);
        vkDestroyDescriptorSetLayout(device, descriptor_layout, nullptr);
        for (auto i : {2, 1, 0}) {
            if (memories[i])
                vkFreeMemory(device, memories[i], nullptr);
            if (buffers[i])
                vkDestroyBuffer(device, buffers[i], nullptr);
        }
    }

    void allocate(const VkPhysicalDeviceMemoryProperties& props) noexcept(false) {
        const auto desired = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBufferCreateInfo buffer_info{};
        VkMemoryRequirements requirements{};
        // uniform
        {
            uniform_t ubo{};
            ubo.model = ubo.view = ubo.projection = glm::mat4{1};
            ubo.projection[1][1] *= -1; // GL -> Vulkan
            if (auto ec = create_uniform_buffer(device, buffers[0], buffer_info, sizeof(uniform_t)))
                throw vulkan_exception_t{ec, "vkCreateBuffer"};
            if (auto ec = allocate_memory(device, buffers[0], memories[0], buffer_info, desired, props))
                throw vulkan_exception_t{ec, "vkAllocateMemory"};
            if (auto ec = vkBindBufferMemory(device, buffers[0], memories[0], 0))
                throw vulkan_exception_t{ec, "vkBindBufferMemory"};
            vkGetBufferMemoryRequirements(device, buffers[0], &requirements);
            if (auto ec = update_memory(device, memories[0], requirements, &ubo, 0))
                throw vulkan_exception_t{ec, "vkMapMemory"};
            // descriptor set must be updated (before being used with Command Buffer)
            VkDescriptorBufferInfo change{};
            change.buffer = buffers[0];
            change.offset = 0;
            change.range = sizeof(uniform_t);
            VkWriteDescriptorSet write{};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = descriptors[0];
            write.dstBinding = 0;
            write.dstArrayElement = 0; // descriptors can be array
            write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            write.descriptorCount = 1;
            write.pBufferInfo = &change;
            vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        }
        // vertices
        {
            const vector<input_unit_t> vertices{{{-0.8f, -0.9f}, {1, 0, 0}},
                                                {{0.8f, -0.9f}, {0, 1, 0}},
                                                {{0.8f, 0.9f}, {0, 0, 1}},
                                                {{-0.8f, 0.9f}, {1, 1, 1}}};
            if (auto ec = create_vertex_buffer(device, buffers[1], //
                                               buffer_info, sizeof(input_unit_t) * vertices.size()))
                throw vulkan_exception_t{ec, "vkCreateBuffer"};
            if (auto ec = allocate_memory(device, buffers[1], memories[1], buffer_info, desired, props))
                throw vulkan_exception_t{ec, "vkAllocateMemory"};
            if (auto ec = vkBindBufferMemory(device, buffers[1], memories[1], 0))
                throw vulkan_exception_t{ec, "vkBindBufferMemory"};
            vkGetBufferMemoryRequirements(device, buffers[1], &requirements);
            if (auto ec = update_memory(device, memories[1], requirements, vertices.data(), 0))
                throw vulkan_exception_t{ec, "vkMapMemory"};
        }
        // indices
        {
            const vector<uint16_t> indices{0, 1, 2, 2, 3, 0};
            if (auto ec = create_index_buffer(device, buffers[2], //
                                              buffer_info, sizeof(uint16_t) * indices.size()))
                throw vulkan_exception_t{ec, "vkCreateBuffer"};
            if (auto ec = allocate_memory(device, buffers[2], memories[2], buffer_info, desired, props))
                throw vulkan_exception_t{ec, "vkAllocateMemory"};
            if (auto ec = vkBindBufferMemory(device, buffers[2], memories[2], 0))
                throw vulkan_exception_t{ec, "vkBindBufferMemory"};
            vkGetBufferMemoryRequirements(device, buffers[2], &requirements);
            if (auto ec = update_memory(device, memories[2], requirements, indices.data(), 0))
                throw vulkan_exception_t{ec, "vkMapMemory"};
        }
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

    void setup_vertex_input_state(VkPipelineVertexInputStateCreateInfo& info) noexcept override {
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

    VkResult make_pipeline_layout(VkDevice device, VkPipelineLayout& layout) noexcept override {
        VkPipelineLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        info.setLayoutCount = 1;
        info.pSetLayouts = &descriptor_layout;
        info.pushConstantRangeCount = 0;
        info.pPushConstantRanges = nullptr;
        return vkCreatePipelineLayout(device, &info, nullptr, &layout);
    }

    VkResult update() noexcept(false) override {
        uniform_t ubo{};
        const float time = static_cast<float>(clock()) / 1900;
        const auto Z = glm::vec3(0, 0, 1);
        ubo.model = glm::rotate(glm::mat4(1), glm::radians(time), Z);
        ubo.view = glm::lookAt(glm::vec3(2, 2, 2), glm::vec3(0), Z);
        ubo.projection = glm::perspective(glm::radians(45.0f), 1.0f / 1, 0.1f, 10.0f);
        ubo.projection[1][1] *= -1; // GL -> Vulkan

        VkMemoryRequirements requirements{};
        vkGetBufferMemoryRequirements(device, buffers[0], &requirements);
        if (auto ec = update_memory(device, memories[0], requirements, &ubo, 0))
            return ec;

        VkDescriptorBufferInfo change{};
        change.buffer = buffers[0];
        change.offset = 0;
        change.range = sizeof(uniform_t);
        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstSet = descriptors[0];
        write.dstBinding = 0;
        write.dstArrayElement = 0; // descriptors can be array
        write.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        write.descriptorCount = 1;
        write.pBufferInfo = &change;
        vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
        return VK_SUCCESS;
    }

    void record(VkCommandBuffer command_buffer, VkPipeline pipeline,
                VkPipelineLayout pipeline_layout) noexcept override {
        vkCmdBindPipeline(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        auto binding = 0u;
        vkCmdBindDescriptorSets(command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipeline_layout, //
                                binding, 1, descriptors, 0, nullptr);
        // ...
        auto location = 0u;
        constexpr auto binding_count = 1;
        vkCmdBindVertexBuffers(command_buffer, //
                               location, binding_count, buffers + 1, offsets);
        constexpr auto index_offset = 0;
        vkCmdBindIndexBuffer(command_buffer, //
                             buffers[2], index_offset, VK_INDEX_TYPE_UINT16);
        constexpr auto num_instance = 1;
        constexpr auto first_index = 0;
        constexpr auto vertex_offset = 0;
        constexpr auto first_instance = 0;
        constexpr auto indices_size = 6u;
        vkCmdDrawIndexed(command_buffer, indices_size, num_instance, first_index, vertex_offset, first_instance);
    }
};

auto make_pipeline_input_3(VkDevice device, const VkPhysicalDeviceMemoryProperties& props, const fs::path& shader_dir)
    -> unique_ptr<vulkan_pipeline_input_t> {
    auto impl = make_unique<input3_t>(device, shader_dir);
    impl->allocate(props);
    return impl;
}
