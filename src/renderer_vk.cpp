#include "tskgfx/renderer_vk.h"

#include "tskgfx/renderer.h"
#include "tskgfx/tskgfx.h"
#include "tskgfx/spirv.h"

#include <tsk/file.h>
#include <tsk/murmur_hash_3.h>

#include <VkBootstrap.h>
#include <vulkan/vulkan_core.h>

#define VMA_IMPLEMENTATION
#include <vma/vk_mem_alloc.h>

#include <assert.h>
#include <unordered_map>

#ifdef TUSK_DEBUG
#define VK_CHECK(call)                                         \
    do {                                                       \
        VkResult result = (call);                              \
        if (result != VK_SUCCESS) {                            \
            fprintf(stderr, "Vulkan Error: %d in %s at line %d\n", \
                    result, __FILE__, __LINE__);               \
            exit(EXIT_FAILURE);                                \
        }                                                      \
    } while (0)
#else
#define VK_CHECK(call)
#endif

/// ~ Vulkan helper functions. ~

/// @brief Transitions the layout of a vulkan image form current to new.
///
/// @attention does so in a complete blocking with no regard for usage.
static void transition_image(VkCommandBuffer cmd, VkImage image, VkImageAspectFlags aspect_flags, VkImageLayout current_layout, VkImageLayout new_layout) {
    VkImageMemoryBarrier2 image_barrier = {};
    image_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;

    image_barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;

    image_barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    image_barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;

    image_barrier.oldLayout = current_layout;
    image_barrier.newLayout= new_layout;

    image_barrier.image = image;
    image_barrier.subresourceRange.aspectMask = aspect_flags;
    image_barrier.subresourceRange.baseMipLevel = 0;
    image_barrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    image_barrier.subresourceRange.baseArrayLayer = 0;
    image_barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
    dependency_info.imageMemoryBarrierCount = 1;
    dependency_info.pImageMemoryBarriers = &image_barrier;

    vkCmdPipelineBarrier2(cmd, &dependency_info);
}

/// @brief Copies an image to another image. 
///
/// @attention assumes src and dst extend are in the transfer layouts. 
/// Restricted to 2D images.
static void copy_image_to_image(VkCommandBuffer cmd, VkImage src, VkImage dst, VkImageAspectFlags aspect_flags, VkExtent2D src_extent, VkExtent2D dst_extent) {
    VkImageBlit2 blit_region = {};
    blit_region.sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2;

    blit_region.srcOffsets[1].x = src_extent.width;
    blit_region.srcOffsets[1].y = src_extent.height;
    blit_region.srcOffsets[1].z = 1;

    blit_region.dstOffsets[1].x = dst_extent.width;
    blit_region.dstOffsets[1].y = dst_extent.height;
    blit_region.dstOffsets[1].z = 1;

    blit_region.srcSubresource.aspectMask = aspect_flags;
    blit_region.srcSubresource.layerCount = 1;
    blit_region.srcSubresource.baseArrayLayer = 0;
    blit_region.srcSubresource.mipLevel = 0;

    blit_region.dstSubresource.aspectMask = aspect_flags;
    blit_region.dstSubresource.layerCount = 1;
    blit_region.dstSubresource.baseArrayLayer = 0;
    blit_region.dstSubresource.mipLevel = 0;

    VkBlitImageInfo2 blit_info = {};
    blit_info.sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2;
    blit_info.srcImage = src;
    blit_info.dstImage = dst;

    blit_info.regionCount = 1;
    blit_info.pRegions = &blit_region;
    blit_info.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    blit_info.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;

    vkCmdBlitImage2(cmd, &blit_info);
}

namespace tsk {

class RenderContextVk : public RenderContextI {
public:

    RenderContextVk() = default;
    virtual ~RenderContextVk() = default;

    virtual bool init(const AppConfig& config) override;
    virtual void shutdown() override;
    virtual void frame() override;

    virtual void create_texture_2d(TextureHandle handle, const TextureInfo& info) override;
    virtual void update_texture_2d(TextureHandle th, uint32_t offset, uint32_t size, void* data) override;
    virtual void destroy(TextureHandle handle) override;

    virtual void create_shader(ShaderHandle handle, const char* path) override;

    virtual void create_program(ProgramHandle handle, ShaderHandle csh) override;
    virtual void create_program(ProgramHandle handle, ShaderHandle vsh, ShaderHandle fsh) override;

    virtual void create_descriptor(DescriptorHandle handle, DescriptorType type, uint16_t num, const char* name) override;

    virtual void create_uniform_buffer(BufferHandle bh, uint32_t size, void* data) override;
    virtual void create_vertex_buffer(BufferHandle bh, VertexLayoutHandle vlh, uint32_t size, void* data) override;
    virtual void create_index_buffer(BufferHandle bh, uint32_t size, void* data) override;

    virtual void update_buffer(BufferHandle handle, uint32_t offset, uint32_t size, void* data) override;

    virtual void destroy_buffer(BufferHandle) override;

    virtual void submit(Frame* frame) override;
private:
    void resize_swapchain();

    void rebuild_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, int width, int height);
    void destroy_swapchain();
};

RenderContextI* create_render_context() {
    return new RenderContextVk();
}

// ~ Render Context
AppConfig config;
Frame* render_frame;
RenderContextDirtyFlags dirty = RenderContextDirtyFlags::k_none;

/// ~ Vulkan Render Context ~
struct DrawPushConstants {
    float viewproj[16];
    float model[16];
    float camera_pos[4];
    VkDeviceAddress vbo;
};

// Vulkan Core.
VkInstance instance;
VkDebugUtilsMessengerEXT debug_messenger;
VkPhysicalDevice physical_device;
VkDevice device;
VkSurfaceKHR surface;

// Queues.
VkQueue graphics_queue;
uint32_t graphics_queue_index;

// Swapchain.
VkSwapchainKHR swapchain;
VkFormat swapchain_image_format;
VkExtent2D swapchain_extent;
std::vector<VkImage> swapchain_images;
std::vector<VkImageView> swapchain_image_views;

// Frame contexts.
VkCommandPool command_pools[k_frame_overlap];
VkCommandBuffer command_buffers[k_frame_overlap];
VkSemaphore render_semaphores[k_frame_overlap];
VkSemaphore swapchain_semaphore[k_frame_overlap];
VkFence render_fence[k_frame_overlap];
int current_frame;

// Rendering resources.
VmaAllocator allocator;
TextureVk final_color_texture;
TextureVk final_depth_texture;

void (*imgui_draw_fn)(VkCommandBuffer) = nullptr;

// Descriptors.
VkDescriptorPool descriptor_pool;

// ~ Resources ~

// [Resource] : pipelines.
std::unordered_map<VkPipelineLayout, VkPipeline> pipeline_cache;
std::unordered_map<uint64_t, VkShaderModule> shader_module_cache;

// [Resource] : shader programs.
ProgramVk program_cache[256] = {};
ShaderVk shader_cache[256] = {};

// [Resource] : buffers.
BufferVk buffer_cache[256] = {};

BufferHandle dirty_buffers[256] = {};
void* buffer_data_ptrs[256] = {};
int dirty_buffers_head = 0;

// [Resource] : textures
TextureVk texture_cache[256] = {};

TextureHandle dirty_textures[256] = {};
void* texture_data_ptrs[256] = {};
int dirty_textures_head = 0;

// [Resources] : samplers
VkSampler texture_sampler_cache[256] = {};

// [Resource] : descriptors.
DescriptorInfo descriptor_set_info_cache[256] = {};
std::unordered_map<uint32_t, VkDescriptorSet> ds_set_cache;

// Default resources.
tsk::TextureHandle white_rgba_th;

inline const VkDeviceSize BufferVk::allocated_size() const {return allocation->GetSize(); }

void BufferVk::create(VkBufferUsageFlags usage, VkDeviceSize requested_size, bool mappable) {
    assert(!valid() && "Buffer already initialized!");
    assert(requested_size > 0 && "Buffer cannot be created with size <= 0");

    VkBufferCreateInfo buffer_create_info = {};
    buffer_create_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_create_info.size = requested_size;
    buffer_create_info.usage = usage;

    VmaAllocationCreateInfo alloc_create_info = {};
    alloc_create_info.requiredFlags = 0;
    alloc_create_info.usage = VMA_MEMORY_USAGE_AUTO;

    if(mappable) {
         // Warning: Declares that mapped memory will only be written sequentially, e.g. using memcpy() or a loop writing number-by-number, never read or accessed randomly, 
         // so a memory type can be selected that is uncached and write-combined.
        alloc_create_info.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    VK_CHECK(vmaCreateBuffer(allocator, &buffer_create_info, &alloc_create_info, &buffer, &allocation, nullptr));

    // Update address if requested.
    if((usage & VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) == VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT) {
        VkBufferDeviceAddressInfo buffer_device_address_info {};
        buffer_device_address_info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        buffer_device_address_info.buffer = buffer;
        address = vkGetBufferDeviceAddress(device, &buffer_device_address_info);
    }

    // Update size.
    device_size = requested_size;
}

void BufferVk::update(VkCommandBuffer cmd, uint32_t offset, uint32_t size, void* data) {
    assert(offset + size <= allocation->GetSize() && "Cannot updated buffer with data. Not enough size!");

    VkMemoryPropertyFlags memory_properties = {};
    vmaGetAllocationMemoryProperties(allocator, allocation, &memory_properties);

    // Check if no explicit sync is needed.
    if ((memory_properties & (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) == 
        (VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {

        void* mapped_data;
        VK_CHECK(vmaMapMemory(allocator, allocation, &mapped_data));
        memcpy(static_cast<char*>(mapped_data) + offset, data, size);
        vmaUnmapMemory(allocator, allocation);

        return;
    }

    // TODO: Create cached and reuseable with dynamic src offset. Especially if transient buffers.
    BufferVk staging_buffer;
    staging_buffer.create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, true);

    void* mapped_data;
    VK_CHECK(vmaMapMemory(allocator, staging_buffer.allocation, &mapped_data));
    memcpy(static_cast<char*>(mapped_data) + offset, data, size);
    vmaUnmapMemory(allocator, staging_buffer.allocation);

    // Buffer copy command. 
    {
        VkBufferCopy copy_region = {};
        copy_region.size = size;
        copy_region.srcOffset = 0;
        copy_region.dstOffset = offset;
        vkCmdCopyBuffer(cmd, staging_buffer.buffer, buffer, 1, &copy_region); 
        
        VkMemoryBarrier2KHR buffer_memory_barrier = {};
        buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;

        buffer_memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
        buffer_memory_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;

        buffer_memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
        buffer_memory_barrier.dstAccessMask =  VK_ACCESS_2_MEMORY_READ_BIT_KHR;

        VkDependencyInfo dependency_info = {};
        dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

        dependency_info.memoryBarrierCount = 1;
        dependency_info.pMemoryBarriers = &buffer_memory_barrier;;

        vkCmdPipelineBarrier2(cmd, &dependency_info);
    }

    // TODO: Destroy after commands are handled.
    // vmaDestroyBuffer(allocator, staging_buffer.buffer, staging_buffer.allocation);
}

void BufferVk::destroy() { 
    assert(valid() && "Cannot destroy buffer that has not been initialized!");
    vmaDestroyBuffer(allocator, buffer, allocation);
}

void TextureVk::create(VkImageUsageFlags usage, VkExtent3D extent, VkFormat format, VkImageAspectFlags aspect) { 
    assert(!valid() && "Texture already initialized!");

    VkImageCreateInfo image_info = {};
    image_info.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;

    image_info.usage = usage;
    image_info.extent = extent;
    image_info.format = format;

    image_info.imageType = VK_IMAGE_TYPE_2D;
    image_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_info.samples = VK_SAMPLE_COUNT_1_BIT;
    image_info.tiling = VK_IMAGE_TILING_OPTIMAL;

    image_info.mipLevels = 1;
    image_info.arrayLayers = 1;

    VmaAllocationCreateInfo alloc_info = {};
    alloc_info.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    alloc_info.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(allocator, &image_info, &alloc_info, &image, &allocation, nullptr));

    VkImageViewCreateInfo view_info = {};
    view_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    view_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view_info.format = format;
    view_info.image = image;
    view_info.subresourceRange.aspectMask = aspect;
    view_info.subresourceRange.baseMipLevel = 0;
    view_info.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    view_info.subresourceRange.baseArrayLayer = 0;
    view_info.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

    VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &image_view));

    // Assign properties.
    this->extent = extent;
    this->format = format;
}

void TextureVk::update(VkCommandBuffer cmd, uint32_t offset, uint32_t size, void* data) {
    assert(offset + size <= allocation->GetSize() && "Cannot updated buffer with data. Not enough size!");

    // TODO: Create cached and reuseable with dynamic src offset. Especially if transient buffers.
    BufferVk staging_buffer;
    staging_buffer.create(VK_BUFFER_USAGE_TRANSFER_SRC_BIT, size, true);
    staging_buffer.update(cmd, offset, size, data);

    transition_image(cmd, image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkBufferImageCopy image_copy = {};
    image_copy.imageExtent = extent;
    image_copy.imageOffset = {};
    image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.imageSubresource.mipLevel = 0;
    image_copy.imageSubresource.baseArrayLayer = 0 ;
    image_copy.imageSubresource.layerCount = 1;

    image_copy.bufferOffset = 0;
    image_copy.bufferRowLength = 0;
    image_copy.bufferImageHeight = 0;

    vkCmdCopyBufferToImage(cmd, staging_buffer.buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);

    VkMemoryBarrier2KHR buffer_memory_barrier = {};
    buffer_memory_barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2_KHR;

    buffer_memory_barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT_KHR;
    buffer_memory_barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT_KHR;

    buffer_memory_barrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_ATTRIBUTE_INPUT_BIT_KHR | VK_PIPELINE_STAGE_2_INDEX_INPUT_BIT_KHR | VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT_KHR;
    buffer_memory_barrier.dstAccessMask =  VK_ACCESS_2_MEMORY_READ_BIT_KHR;

    VkDependencyInfo dependency_info = {};
    dependency_info.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;

    dependency_info.memoryBarrierCount = 1;
    dependency_info.pMemoryBarriers = &buffer_memory_barrier;;

    vkCmdPipelineBarrier2(cmd, &dependency_info);

    // TODO: Destroy after commands are handled.
    // vmaDestroyBuffer(allocator, staging_buffer.buffer, staging_buffer.allocation);
}

void TextureVk::destroy() { 
    assert(valid() && "Cannot destroy that has not been initialized!");

    vmaDestroyImage(allocator, image, allocation);
    vkDestroyImageView(device, image_view, nullptr);
}

void ShaderVk::create(const char* path) {
    assert(path != nullptr && "Passed invalid path!");

    if(const size_t n_bytes = tsk::file_read(path, nullptr, 0))  {
        std::vector<char> buffer(n_bytes);
        if(tsk::file_read(path, buffer.data(), buffer.size())) {
            VkShaderModuleCreateInfo shader_module_info = {};
            shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            shader_module_info.codeSize = buffer.size();
            shader_module_info.pCode = reinterpret_cast<uint32_t*>(buffer.data());

            assert(parse_spirv(buffer.data(), buffer.size(), bindings, &n_bindings, pc_ranges, &n_pc_ranges));

            VK_CHECK(vkCreateShaderModule(device, &shader_module_info, nullptr, &module));

            // TODO: Hash by reflection
            // Hash & cache.
            shader_module_cache[std::hash<std::string>{}(path)] = module;
        }
    }
}

void ProgramVk::create(const char* csh_path) { 
    assert(!valid() && "already intialized!");
    assert(csh_path && "Must have valid csh shader!");

    ShaderVk compute_shader;
    compute_shader.create(csh_path);

    // TODO: Get descriptor layouts and bindings from shader.
    // Allocate and define descriptor sets and bindings.
    VkDescriptorSetLayoutBinding binding = {};
    binding.binding = 0;
    binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    binding.descriptorCount = 1;
    binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {};
    descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.bindingCount = 1;
    descriptor_set_layout_info.pBindings = &binding;

    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_info, nullptr, &descriptor_set_layout));

    // Create pipeline layout.
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    // Create pipeline.
    VkPipelineShaderStageCreateInfo shader_stage_info = {};
    shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shader_stage_info.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    shader_stage_info.module = compute_shader.module;
    shader_stage_info.pName = "main";

    VkComputePipelineCreateInfo info {};
    info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    info.layout = pipeline_layout;
    info.stage = shader_stage_info;

    // Create & Cache.
    VkPipeline pipeline;
    VK_CHECK(vkCreateComputePipelines(device, nullptr, 1, &info, nullptr, &pipeline));
    pipeline_cache[pipeline_layout] = pipeline;
}

void ProgramVk::create(const ShaderVk& vs, const ShaderVk& fs) { 
    assert(!valid() && "Program already intialized!");
    assert(vs.valid() || fs.valid() && "Must have atleast one valid vs/fs shader!");

    n_bindings = 0;
    VkDescriptorSetLayoutBinding bindings[k_max_program_set_bindings] = {};

    n_pc_ranges = 0;
    VkPushConstantRange pc_ranges[k_max_pc_ranges] = {};

    if(vs.valid()) {
        for(uint32_t i = 0; i < vs.n_bindings; i++) {
            bindings[vs.bindings[i].binding] = vs.bindings[i];
            bindings[vs.bindings[i].binding].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;

            n_bindings++;
        }

        // TODO: Support multiple pc ranges
        for(uint32_t i = 0; i < vs.n_pc_ranges; i++) {
            n_pc_ranges++;
            pc_ranges[i] = vs.pc_ranges[i];
            pc_ranges[i].stageFlags |= VK_SHADER_STAGE_VERTEX_BIT;
        }
    }

    if(fs.valid()) {
        for(uint32_t i = 0; i < fs.n_bindings; i++) {
            // Check if shaders compatible and apply shader stages.
            if(bindings[fs.bindings[i].binding].stageFlags == 0) {
                bindings[fs.bindings[i].binding] = fs.bindings[i];
                n_bindings++;
            }

            bindings[fs.bindings[i].binding].stageFlags |= VK_SHADER_STAGE_FRAGMENT_BIT;
        }
    }

    VkDescriptorSetLayoutCreateInfo descriptor_set_layout_info = {};
    descriptor_set_layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptor_set_layout_info.bindingCount = n_bindings;
    descriptor_set_layout_info.pBindings = bindings;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &descriptor_set_layout_info, nullptr, &descriptor_set_layout));

    // Create pipeline layout.
    VkPipelineLayoutCreateInfo pipeline_layout_info = {};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 1;
    pipeline_layout_info.pSetLayouts = &descriptor_set_layout;

    pipeline_layout_info.pushConstantRangeCount = n_pc_ranges;
    pipeline_layout_info.pPushConstantRanges= pc_ranges;

    VK_CHECK(vkCreatePipelineLayout(device, &pipeline_layout_info, nullptr, &pipeline_layout));

    // Create pipeline.
    VkGraphicsPipelineCreateInfo info = {};
    info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    info.flags = 0;

    VkPipelineShaderStageCreateInfo shader_stage_create_info[2] = {};

    if(vs.valid()) {
        shader_stage_create_info[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[0].flags = 0;
        shader_stage_create_info[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shader_stage_create_info[0].module = vs.module;
        shader_stage_create_info[0].pName = "main";
        shader_stage_create_info[0].pSpecializationInfo = nullptr;
    }

    if(fs.valid()) {
        shader_stage_create_info[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shader_stage_create_info[1].flags = 0;
        shader_stage_create_info[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shader_stage_create_info[1].module = fs.module;
        shader_stage_create_info[1].pName = "main";
        shader_stage_create_info[1].pSpecializationInfo = nullptr;
    }

    info.stageCount = (vs.valid() ? 1 : 0) + (fs.valid() ? 1 : 0);
    info.pStages = &shader_stage_create_info[0] + (!vs.valid() && fs.valid() ? 1 : 0);

    VkPipelineVertexInputStateCreateInfo vertex_input_state_create_info = {};
    vertex_input_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    info.pVertexInputState = &vertex_input_state_create_info;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_state_create_info = {};
    input_assembly_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_state_create_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_state_create_info.primitiveRestartEnable = VK_FALSE;
    info.pInputAssemblyState = &input_assembly_state_create_info;

    VkPipelineTessellationStateCreateInfo tessellation_state_create_info = {};
    tessellation_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    info.pTessellationState = &tessellation_state_create_info;

    VkPipelineViewportStateCreateInfo viewport_state_create_info = {};
    viewport_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_state_create_info.viewportCount = 1;
    viewport_state_create_info.scissorCount = 1;

    info.pViewportState = &viewport_state_create_info;

    VkPipelineRasterizationStateCreateInfo rasterization_state_create_info = {};
    rasterization_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterization_state_create_info.flags = 0;
    rasterization_state_create_info.depthClampEnable = VK_FALSE;
    rasterization_state_create_info.rasterizerDiscardEnable = VK_FALSE;
    rasterization_state_create_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterization_state_create_info.cullMode = VK_CULL_MODE_NONE;
    rasterization_state_create_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterization_state_create_info.depthBiasEnable = VK_FALSE;
    rasterization_state_create_info.depthBiasConstantFactor = 1.0f;
    rasterization_state_create_info.depthBiasClamp = 0;
    rasterization_state_create_info.depthBiasSlopeFactor = 0;
    rasterization_state_create_info.lineWidth = 1.0f;

    info.pRasterizationState = &rasterization_state_create_info;

    VkPipelineMultisampleStateCreateInfo multisample_state_create_info = {};
    multisample_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample_state_create_info.flags = 0;
    multisample_state_create_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisample_state_create_info.sampleShadingEnable = VK_FALSE;
    multisample_state_create_info.minSampleShading = 1.0f;
    multisample_state_create_info.pSampleMask = nullptr;
    multisample_state_create_info.alphaToCoverageEnable = VK_FALSE;
    multisample_state_create_info.alphaToOneEnable = VK_FALSE;

    info.pMultisampleState = &multisample_state_create_info;
 
    VkPipelineDepthStencilStateCreateInfo depth_stencil_state_create_info = {};
    depth_stencil_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depth_stencil_state_create_info.depthTestEnable = VK_TRUE;
    depth_stencil_state_create_info.depthWriteEnable = VK_TRUE;

    // Inverted depth for accuracy.
    depth_stencil_state_create_info.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;

    depth_stencil_state_create_info.depthBoundsTestEnable = VK_FALSE;
    depth_stencil_state_create_info.stencilTestEnable = VK_FALSE;

    depth_stencil_state_create_info.front = {};
    depth_stencil_state_create_info.back = {};
    depth_stencil_state_create_info.minDepthBounds = 0.0f;
    depth_stencil_state_create_info.maxDepthBounds = 1.0f;

    info.pDepthStencilState = &depth_stencil_state_create_info;

    VkPipelineColorBlendStateCreateInfo color_blend_state_create_info = {};
    color_blend_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_state_create_info.flags = 0;
    color_blend_state_create_info.logicOpEnable = VK_FALSE;
    color_blend_state_create_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_state_create_info.attachmentCount = 1;

    VkPipelineColorBlendAttachmentState color_blend_attachment_state = {};
    color_blend_attachment_state.blendEnable = VK_TRUE;
    color_blend_attachment_state.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment_state.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    color_blend_attachment_state.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    color_blend_attachment_state.colorBlendOp = VK_BLEND_OP_ADD;
    color_blend_attachment_state.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    color_blend_attachment_state.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    color_blend_attachment_state.alphaBlendOp = VK_BLEND_OP_ADD;

    color_blend_state_create_info.pAttachments = &color_blend_attachment_state;

    info.pColorBlendState = &color_blend_state_create_info;

    VkDynamicState dynamic_state[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamic_state_create_info = {};
    dynamic_state_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_create_info.pDynamicStates = dynamic_state;
    dynamic_state_create_info.dynamicStateCount = 2;

    info.pDynamicState = &dynamic_state_create_info;

    info.layout = pipeline_layout;

    info.renderPass = VK_NULL_HANDLE;
    info.basePipelineHandle = VK_NULL_HANDLE;

    VkPipelineRenderingCreateInfo render_info = {};
    render_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;

    VkFormat color_attachment_format = final_color_texture.format;
    render_info.colorAttachmentCount = 1;
    render_info.pColorAttachmentFormats = &color_attachment_format;

    VkFormat depth_attachment_format = VK_FORMAT_D32_SFLOAT;
    render_info.depthAttachmentFormat = depth_attachment_format;

    info.pNext = &render_info;

    // Create & Cache.
    VkPipeline pipeline = VK_NULL_HANDLE;
    VK_CHECK(vkCreateGraphicsPipelines(device, nullptr, 1, &info, nullptr, &pipeline));
    pipeline_cache[pipeline_layout] = pipeline;
}

VkPipeline get_pipeline(const ProgramVk& program) {
    auto it = pipeline_cache.find(program.pipeline_layout);

    if(it == pipeline_cache.end()) {
        return VK_NULL_HANDLE;
    }

    return it->second;
}

VkDescriptorSet get_descriptor_set(VkCommandBuffer cmd, ProgramHandle ph, DescriptorHandle* dhs, uint32_t dh_count) {
    const ProgramVk& program = program_cache[ph];
    assert(dh_count == program.n_bindings && "[TSKGFX]: Bindings not compatible with program!");

    // Murmur hash descriptors with program as seed.
    uint32_t ds_hash;
    tsk::murmur_hash3_x86_32(dhs, dh_count, ph, &ds_hash);
    auto it = ds_set_cache.find(ds_hash);

    if(it != ds_set_cache.end()) {
        return it->second;
    }

    // Allocate descriptor set.
    VkDescriptorSetAllocateInfo alloc_info = {};
    alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    alloc_info.descriptorPool = descriptor_pool;
    alloc_info.descriptorSetCount = 1;
    alloc_info.pSetLayouts = &program.descriptor_set_layout;

    VkDescriptorSet ds;
    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &ds));

    VkDescriptorImageInfo image_infos[k_max_desciptors] = {};
    VkDescriptorBufferInfo buffer_infos[k_max_desciptors] = {};

    // Update descriptor set.
    std::vector<VkWriteDescriptorSet> writes(dh_count);
    for(uint32_t i = 0; i < dh_count; i++) {
        const DescriptorHandle dh = dhs[i];
        const DescriptorInfo& d_info = descriptor_set_info_cache[dh];
        assert(d_info.valid() && "[TSKGFX]: Cannot bind descriptor to null reference");

        writes[i] = {};
        writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[i].descriptorType = VkDescriptorType(d_info.type);
        writes[i].descriptorCount = 1;
        writes[i].dstBinding = i;
        writes[i].dstSet = ds;

        switch(writes[i].descriptorType) {
            case(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER):
            case(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER): {
                const BufferVk& ub = buffer_cache[d_info.resource_handle_index];

                buffer_infos[i] = {};
                buffer_infos[i].buffer = ub.buffer;
                buffer_infos[i].offset = 0;
                buffer_infos[i].range = VK_WHOLE_SIZE;

                writes[i].pBufferInfo = &buffer_infos[i];
            }break;

            case(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER): {
                TextureHandle th = {d_info.resource_handle_index};

                // Use default uknown texture.
                if(th.idx == tsk::k_invalid_handle) {
                    th = white_rgba_th;
                }

                TextureVk& texture = texture_cache[th];

                VkSamplerCreateInfo sampler_info = {};
                sampler_info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
                sampler_info.minFilter = VK_FILTER_NEAREST;
                sampler_info.magFilter = VK_FILTER_NEAREST;

                /*sampler_info.mipmapMode;*/

                sampler_info.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                sampler_info.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
                sampler_info.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

                sampler_info.mipLodBias = 0;

                sampler_info.anisotropyEnable = VK_FALSE;
                sampler_info.maxAnisotropy = 0;

                /*VkBool32                compareEnable;*/
                /*VkCompareOp             compareOp;*/
                /*float                   minLod;*/
                /*float                   maxLod;*/
                /*VkBorderColor           borderColor;*/
                /*VkBool32                unnormalizedCoordinates;*/
                VkSampler sampler = VK_NULL_HANDLE;
                VK_CHECK(vkCreateSampler(device, &sampler_info, nullptr, &sampler));
                texture_sampler_cache[th] = sampler;

                image_infos[i] = {};
                image_infos[i].imageView = texture.image_view;
                image_infos[i].imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                image_infos[i].sampler = sampler;

                writes[i].pImageInfo = &image_infos[i];

                // TODO: Move to texture update.
                // Transition for use. 
                transition_image(cmd, texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }break;

            case(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE): {
                VkDescriptorImageInfo info = {};
                info.imageView = texture_cache[d_info.resource_handle_index].image_view;
                info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                writes[i].pImageInfo = &info;
            }break;

            default:
                assert(false && "[TSKGFX]: Unsupported descriptor type!");
                break;
        };
    }

    vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    return ds_set_cache[ds_hash] = ds;
}

// TODO: Move to Client. 
ProgramVk compute_program;

bool RenderContextVk::init(const AppConfig& app_config) {
    // Store config.
    config = app_config;

    // Build context.
    vkb::InstanceBuilder instance_builder;
    auto inst_ret = instance_builder.set_app_name(app_config.app_name)
#ifdef TUSK_DEBUG
                    .request_validation_layers()
                    .use_default_debug_messenger()
#endif
                    .require_api_version(1,3,0)
                    .build();
    vkb::Instance vkb_instance = inst_ret.value();

    instance = vkb_instance.instance;
    debug_messenger = vkb_instance.debug_messenger;

    // TODO: Create platform specific surface.
#ifdef TUSK_WIN32
    VkWin32SurfaceCreateInfoKHR surface_create_info = {};
    surface_create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
    surface_create_info.hinstance = nullptr;
    surface_create_info.hwnd = (HWND)app_config.nwh;
    surface_create_info.hinstance = (HINSTANCE)app_config.ndt;

    vkCreateWin32SurfaceKHR(instance, &surface_create_info, nullptr, &surface);
#endif

    // Select and create device.
    VkPhysicalDeviceVulkan13Features features13 = {};
    features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
    features13.dynamicRendering = true;
    features13.synchronization2 = true;

    VkPhysicalDeviceVulkan12Features features12 = {};
    features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
    features12.bufferDeviceAddress = true;

    vkb::PhysicalDeviceSelector selector {vkb_instance};
    vkb::PhysicalDevice vkb_physical_device  = selector
        .set_minimum_version(1, 3)
        .set_required_features_13(features13)
        .set_required_features_12(features12)
        .set_surface(surface)
        .select()
        .value();

    vkb::DeviceBuilder device_builder {vkb_physical_device};
    vkb::Device vkb_device = device_builder.build().value();

    physical_device = vkb_physical_device.physical_device;
    device = vkb_device;

    // Build swapchain and swapchain images.
    rebuild_swapchain(physical_device,device, surface, app_config.width, app_config.height);

    // Get Queues.
    graphics_queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
    graphics_queue_index = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

    // Create frame context.
    {
        // Creating command pool.
        {
            VkCommandPoolCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
            create_info.queueFamilyIndex = graphics_queue_index;

            for(int i = 0; i < k_frame_overlap; i++) {
                VkCommandPool command_pool;
                VK_CHECK(vkCreateCommandPool(device, &create_info, nullptr, &command_pool));

                // Creating command buffer from pool.
                VkCommandBufferAllocateInfo alloc_info = {};
                alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
                alloc_info.commandPool = command_pool;
                alloc_info.commandBufferCount = 1;
                alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

                VkCommandBuffer command_buffer;
                VK_CHECK(vkAllocateCommandBuffers(device, &alloc_info, &command_buffer));

                command_pools[i] = command_pool;
                command_buffers[i] = command_buffer;
            }
        }

        // Create frame sync structures.
        for(int i = 0; i < k_frame_overlap; i++) {
            {
                VkSemaphoreCreateInfo create_info {};
                create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

                VkSemaphore semaphore;
                VK_CHECK(vkCreateSemaphore(device, &create_info, nullptr, &semaphore));

                swapchain_semaphore[i] = semaphore;
            }

            {
                VkSemaphoreCreateInfo create_info {};
                create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

                VkSemaphore semaphore;
                VK_CHECK(vkCreateSemaphore(device, &create_info, nullptr, &semaphore));

                render_semaphores[i] = semaphore;
            }

            {
                VkFenceCreateInfo create_info {};
                create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
                create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

                VkFence fence;
                VK_CHECK(vkCreateFence(device, &create_info, nullptr, &fence));

                render_fence[i] = fence;
            }
        }
    }

    // Initialize vmaAllocator.
    VmaAllocatorCreateInfo allocator_info = {};
    allocator_info.instance = instance;
    allocator_info.physicalDevice = physical_device;
    allocator_info.device = device;
    allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator));

    // TODO: Allocate based of shaders. 
    // Create descriptor pools and descriptors.
    std::vector<VkDescriptorPoolSize> pool_sizes = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 32},

        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 32},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 32},
    };

    VkDescriptorPoolCreateInfo pool_info = {};
    pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    pool_info.maxSets = k_max_desciptors;
    pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
    pool_info.pPoolSizes = pool_sizes.data();
    VK_CHECK(vkCreateDescriptorPool(device, &pool_info, nullptr, &descriptor_pool));

    // Rendering resources.
    assert(app_config.width * app_config.height != 0 && "Cannot have app dimensions of 0!");
    final_color_texture.create(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                              {static_cast<uint32_t>(app_config.width), static_cast<uint32_t>(app_config.height), 1}, 
                              VK_FORMAT_R16G16B16A16_SFLOAT, 
                              VK_IMAGE_ASPECT_COLOR_BIT);

    final_depth_texture.create(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                              {static_cast<uint32_t>(app_config.width), static_cast<uint32_t>(app_config.height), 1}, 
                              VK_FORMAT_D32_SFLOAT, 
                              VK_IMAGE_ASPECT_DEPTH_BIT);

    // TODO: Move to Client. 
    // Create compute pipeline.
    {
        /*compute_program.create("assets/shaders/draw.comp.spv");*/
        /*tsk::create_descriptor("compute_buffer", tsk::DescriptorType::k_storage_image, final_color_texture.handle);*/
    }

    // Default textures.
    {
        TextureInfo default_white_texture = {};
        default_white_texture.width = 256;
        default_white_texture.height = 256;
        default_white_texture.depth = 1;
        default_white_texture.format = tsk::Format::k_r8g8b8a8_unorm;
        white_rgba_th = tsk::create_texture_2d(default_white_texture);

        // TODO: Let own update.
        // TODO: Delete.
        uint8_t* white_data = static_cast<uint8_t*>(malloc(256 * 256 * 4)); 
        memset(white_data, 0xFF, 256 * 256 * 4);
        tsk::update(white_rgba_th, 0, sizeof(white_data), white_data);
    }

    return true;
}

void RenderContextVk::shutdown() {
    // Wait for device to finish commands.
    vkDeviceWaitIdle(device);

    // ~ Clean gpu resources ~
    // TODO: Staging buffers.

    // Managed
    for(BufferVk& buffer : buffer_cache) {
        if(!buffer.valid()) continue;
        buffer.destroy();
    }

    final_depth_texture.destroy();
    final_color_texture.destroy();

    vmaDestroyAllocator(allocator);

    for(auto& it : pipeline_cache) {
        vkDestroyPipelineLayout(device, it.first, nullptr);
        vkDestroyPipeline(device, it.second, nullptr);
    }

    for(auto& it : shader_module_cache) {
        vkDestroyShaderModule(device, it.second, nullptr);
    }

    vkDestroyDescriptorPool(device, descriptor_pool, nullptr);

    // Clean frame context.
    {
        for(int i = 0; i < k_frame_overlap; i++) {
            // Commands clean.
            vkDestroyCommandPool(device, command_pools[i], nullptr);

            vkDestroySemaphore(device, swapchain_semaphore[i], nullptr);
            vkDestroySemaphore(device, render_semaphores[i], nullptr);
            vkDestroyFence(device, render_fence[i], nullptr);
        }
    }

    // Swapchain & surface clean.
    destroy_swapchain();

    //device & instance clean.
#ifdef TUSK_DEBUG
    vkb::destroy_debug_utils_messenger(instance, debug_messenger);
#endif
    vkDestroyDevice(device, nullptr);
    vkDestroyInstance(instance, nullptr);
}

void RenderContextVk::frame() {
    if((dirty &= RenderContextDirtyFlags::k_swapchain) == RenderContextDirtyFlags::k_swapchain) {
        resize_swapchain();
    }

    // Wait till gpu has finished rendering previous frame.
    VK_CHECK(vkWaitForFences(device, 1, &render_fence[current_frame], VK_TRUE, UINT64_MAX));
    VK_CHECK(vkResetFences(device, 1, &render_fence[current_frame]));

    // Ask for image index to render to.and signal swapchain_semaphore.
    uint32_t swapchain_index;
    VkResult sc_acquire_res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, swapchain_semaphore[current_frame], VK_NULL_HANDLE, &swapchain_index);
    if(sc_acquire_res == VK_ERROR_OUT_OF_DATE_KHR) {
        dirty |= RenderContextDirtyFlags::k_swapchain;
        return;
    }

    // Reset & begin command buffer.
    VkCommandBuffer cmd = command_buffers[current_frame];
    vkResetCommandBuffer(cmd, 0);

    VkCommandBufferBeginInfo begin_info = {};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VK_CHECK(vkBeginCommandBuffer(cmd, &begin_info));

    // ~ Updated Resources ~
    // TODO: Pool to resource udpates to single pipeline barrier.
    for(int i = 0; i < dirty_buffers_head; i++) {
        BufferHandle bh = dirty_buffers[i];

        BufferVk& buffer = buffer_cache[bh];
        void* data = buffer_data_ptrs[bh];

        buffer.update(cmd, 0, buffer.size(), buffer_data_ptrs[bh]);
    }
    dirty_buffers_head = 0;

    for(int i = 0; i < dirty_textures_head; i++) {

        TextureHandle th = dirty_textures[i];

        TextureVk& texture = texture_cache[th];
        void* data = texture_data_ptrs[th];

        uint32_t format_size = 4;
        texture.update(cmd, 0, texture.extent.width * texture.extent.height * texture.extent.depth * format_size, texture_data_ptrs[th]);
    }
    dirty_textures_head = 0;

    transition_image(cmd, final_color_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

    // [Commmand]: Clear command.
    VkClearColorValue clear = {{1.0f, 1.0f, 1.0f, 1.0f}};

    VkImageSubresourceRange clear_range = {};
    clear_range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    clear_range.baseMipLevel = 0;
    clear_range.levelCount = VK_REMAINING_MIP_LEVELS;
    clear_range.baseArrayLayer = 0;
    clear_range.layerCount = VK_REMAINING_ARRAY_LAYERS;
    vkCmdClearColorImage(cmd, final_color_texture.image, VK_IMAGE_LAYOUT_GENERAL, &clear, 1, &clear_range);

    const uint32_t offsets = 0;

    /*transition_image(cmd, final_color_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);*/
    /**/
    // Compute Draw
    /*VkDescriptorSet descriptor_set = get_descriptor_Set(compute_program);*/
    /*vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, get_pipeline(compute_program));*/
    /*vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, compute_program.pipeline_layout, 0, 1, &descriptor_set, 0, &offsets);*/
    /*vkCmdDispatch(cmd, std::ceil(final_color_texture.extent.width / 16.0f), std::ceil(final_color_texture.extent.height / 16.0f), 1); */

    transition_image(cmd, final_color_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    transition_image(cmd, final_depth_texture.image, VK_IMAGE_ASPECT_DEPTH_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

    // Final render target.
    {
        VkRenderingAttachmentInfo color_attachment_info = {};
        color_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        color_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        color_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        color_attachment_info.imageView = final_color_texture.image_view;
        color_attachment_info.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkRenderingAttachmentInfo depth_attachment_info = {};
        depth_attachment_info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        depth_attachment_info.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depth_attachment_info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depth_attachment_info.imageView = final_depth_texture.image_view;
        depth_attachment_info.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Inverted depth far for accuracy.
        depth_attachment_info.clearValue.depthStencil.depth = 0.0f;

        VkRenderingInfo rendering_info = {};
        rendering_info.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
        rendering_info.layerCount = 1;
        rendering_info.renderArea = VkRect2D{{0,0}, {final_color_texture.extent.width, final_color_texture.extent.height}};
        rendering_info.colorAttachmentCount = 1;
        rendering_info.pColorAttachments = &color_attachment_info;
        rendering_info.pDepthAttachment = &depth_attachment_info;

        static VkDescriptorSet ds_sets_consumable[128] = {VK_NULL_HANDLE};

        // Updated and store descriptor sets.
        for(uint32_t i = 0; i < render_frame->draw_count; i++) {
            RenderDraw& draw = render_frame->draws[i];
            ds_sets_consumable[i] = get_descriptor_set(cmd, draw.ph, draw.dhs, draw.dh_count);
        }

        vkCmdBeginRendering(cmd, &rendering_info);

        ProgramHandle last_ph;
        for(uint32_t draw_count = 0; draw_count < render_frame->draw_count; draw_count++) {
            RenderDraw& draw = render_frame->draws[draw_count];
            const ProgramVk program = program_cache[draw.ph];

            if(last_ph != draw.ph) {
                vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, get_pipeline(program));
                vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, program.pipeline_layout, 0, 1, &ds_sets_consumable[draw_count], 0, &offsets);

                VkViewport viewport = {0.0f, 0.0f, static_cast<float>(final_color_texture.extent.width), static_cast<float>(final_color_texture.extent.height), 0.0f, 1.0f};
                vkCmdSetViewport(cmd, 0, 1, &viewport);

                VkRect2D scissor = {{0, 0}, {final_color_texture.extent.width, final_color_texture.extent.height}};
                vkCmdSetScissor(cmd, 0, 1, &scissor);

                last_ph = draw.ph;
            }

            DrawPushConstants pc = {};
            memcpy(pc.viewproj, draw.viewproj_mtx, sizeof(float) * 16);
            memcpy(pc.model, draw.transform_matrix, sizeof(float) * 16);
            memcpy(pc.camera_pos, draw.camera_pos, sizeof(float) * 3);

            const BufferVk& vb = buffer_cache[draw.vbh];
            pc.vbo = vb.address;

            const BufferVk& ib = buffer_cache[draw.ibh];
            vkCmdBindIndexBuffer(cmd, ib.buffer, 0, VK_INDEX_TYPE_UINT32);

            vkCmdPushConstants(cmd, program.pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(DrawPushConstants), &pc);
            vkCmdDrawIndexed(cmd, ib.size() / sizeof(uint32_t), 1, 0, 0, 0);

            draw.clear();
        }

        render_frame->draw_count = 0;

        // TODO: Move to blit pass.
        (*imgui_draw_fn)(cmd);

        vkCmdEndRendering(cmd);
    }

    transition_image(cmd, final_color_texture.image, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    transition_image(cmd, swapchain_images[swapchain_index], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    // [Commmand]: Copy image to image command.
    copy_image_to_image(cmd, final_color_texture.image, 
                        swapchain_images[swapchain_index], 
                        VK_IMAGE_ASPECT_COLOR_BIT, 
                        {final_color_texture.extent.width, final_color_texture.extent.height}, 
                        swapchain_extent);

    // Transition swapchain to presentable.
    transition_image(cmd, swapchain_images[swapchain_index], VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

    VK_CHECK(vkEndCommandBuffer(cmd));

    // Submit command buffer.
    VkCommandBufferSubmitInfo command_buffer_submit = {};
    command_buffer_submit.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
    command_buffer_submit.deviceMask = 0;
    command_buffer_submit.commandBuffer= cmd;

    VkSemaphoreSubmitInfo wait_semaphore_info = {};
    wait_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    wait_semaphore_info.semaphore = swapchain_semaphore[current_frame];
    wait_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR;

    VkSemaphoreSubmitInfo signal_semaphore_info = {};
    signal_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    signal_semaphore_info.semaphore = render_semaphores[current_frame];
    signal_semaphore_info.stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;

    VkSubmitInfo2 submit = {};
    submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
    submit.commandBufferInfoCount = 1;
    submit.pCommandBufferInfos = &command_buffer_submit;

    submit.waitSemaphoreInfoCount = 1;
    submit.pWaitSemaphoreInfos = &wait_semaphore_info;

    submit.signalSemaphoreInfoCount = 1;
    submit.pSignalSemaphoreInfos = &signal_semaphore_info;
    
    // Submit and signal render fence when complete.
    vkQueueSubmit2(graphics_queue, 1, &submit, render_fence[current_frame]);

    VkPresentInfoKHR present_info = {};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = &swapchain;

    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = &render_semaphores[current_frame];

    present_info.pImageIndices = &swapchain_index;

    VkResult present_res = vkQueuePresentKHR(graphics_queue, &present_info);
    if(present_res == VK_ERROR_OUT_OF_DATE_KHR) {
        dirty |= RenderContextDirtyFlags::k_swapchain;
        return;
    }

    // Increase frame.
    current_frame = (current_frame + 1) % 2;
}

void RenderContextVk::create_texture_2d(TextureHandle handle, const TextureInfo& info) {
    // TODO: Defer to infer usage.
    VkImageUsageFlags image_usage_flags = 0;
    VkImageAspectFlags image_aspect_flags = 0;

    image_usage_flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    image_aspect_flags |= VK_IMAGE_ASPECT_COLOR_BIT;

    texture_cache[handle].create(image_usage_flags,
                              {static_cast<uint32_t>(info.width), static_cast<uint32_t>(info.height), 1}, 
                              VkFormat(info.format), 
                              image_aspect_flags);
}

void RenderContextVk::update_texture_2d(TextureHandle th, uint32_t offset, uint32_t size, void* data) {
    // TODO: Account for offset and size
    dirty_textures[dirty_textures_head] = th;
    ++dirty_textures_head;

    // TODO: Optional no keeping data ptr or copy.
    // Store data.
    texture_data_ptrs[th] = data;
}

void RenderContextVk::destroy(TextureHandle handle) {
}

void RenderContextVk::create_shader(ShaderHandle handle, const char* path) {
    shader_cache[handle].create(path);
}

void RenderContextVk::create_program(ProgramHandle handle, ShaderHandle vsh, ShaderHandle fsh) {
    // TODO: Cache bound check;
    const ShaderVk& vs = shader_cache[vsh];
    const ShaderVk& fs = shader_cache[fsh];

    program_cache[handle].create(vs, fs);
}

void RenderContextVk::create_program(ProgramHandle handle, ShaderHandle csh) {
    const ShaderVk& fs = shader_cache[csh];
    // program_cache[handle].create(csh);
}

void RenderContextVk::create_descriptor(DescriptorHandle handle, DescriptorType type, uint16_t rh, const char* name) {
    assert(strlen(descriptor_set_info_cache[handle].name) == 0 && "Attemping to override descriptor.");

    descriptor_set_info_cache[handle].type = type;
    descriptor_set_info_cache[handle].resource_handle_index = rh;

    strcpy_s(descriptor_set_info_cache[handle].name, name);
};

void RenderContextVk::create_uniform_buffer(BufferHandle bh, uint32_t size, void* data) {
    buffer_cache[bh].create(VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size, true);
}

void RenderContextVk::create_vertex_buffer(BufferHandle bh, VertexLayoutHandle vlh, uint32_t size, void* data) {
    buffer_cache[bh].create(VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, size);
};

void RenderContextVk::create_index_buffer(BufferHandle bh, uint32_t size, void* data)  {
    buffer_cache[bh].create(VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, size);
}

void RenderContextVk::update_buffer(BufferHandle handle, uint32_t offset, uint32_t size, void* data) {
    // TODO: Account for offset and size
    dirty_buffers[dirty_buffers_head] = handle;
    ++dirty_buffers_head;

    // TODO: Optional no keeping data ptr or copy.
    // Store data.
    buffer_data_ptrs[handle] = data;
}

void RenderContextVk::destroy_buffer(BufferHandle bh) {
}

void RenderContextVk::submit(Frame* frame) {
    render_frame = frame;
}

void RenderContextVk::destroy_swapchain() {
    vkDestroySwapchainKHR(device, swapchain, nullptr);
    for(size_t i = 0; i < swapchain_images.size(); i++) {
        vkDestroyImageView(device, swapchain_image_views[i], nullptr);
    }
}

void RenderContextVk::resize_swapchain() {
    vkDeviceWaitIdle(device);

    destroy_swapchain();

#ifdef TUSK_WIN32
    RECT rect;
    if(GetWindowRect((HWND)config.nwh, &rect))
    {
      int width = rect.right - rect.left;
      int height = rect.bottom - rect.top;

      rebuild_swapchain(physical_device,device, surface, width, height);
    }
#endif

    dirty &= ~RenderContextDirtyFlags::k_swapchain;
}

void RenderContextVk::rebuild_swapchain(VkPhysicalDevice physical_device, VkDevice device, VkSurfaceKHR surface, int width, int height) {
    // Create swapchain and swapchain images.
    VkSurfaceFormatKHR surface_format = {};
    surface_format.format = VK_FORMAT_B8G8R8A8_UNORM;
    surface_format.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

    vkb::SwapchainBuilder swapchain_builder{physical_device, device, surface};
    vkb::Swapchain vkb_swapchain = swapchain_builder
        .set_desired_format(surface_format)
        .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
        .set_desired_extent(width, height)
        .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
        .build()
        .value();

    swapchain = vkb_swapchain.swapchain;
    swapchain_extent = vkb_swapchain.extent;
    swapchain_image_format = surface_format.format;
    swapchain_images = vkb_swapchain.get_images().value();
    swapchain_image_views = vkb_swapchain.get_image_views().value();
}

}
