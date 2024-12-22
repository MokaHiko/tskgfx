/**
 * @file renderer_vk.h
 * @brief This file contains the definition for the tgfx renderer.
 *
 * This library is responsible for managing application configuration,
 * rendering, and other functionalities within the TUSK framework.
 *
 * @author Moka
 * @date 2024-11-03
 */

#ifndef RENDERER_VK_H_
#define RENDERER_VK_H_

#ifdef TUSK_WIN32
#define VK_USE_PLATFORM_WIN32_KHR
#elif TUSK_UNIX
#elif TUSK_MACOS
#endif

#include <vma/vk_mem_alloc.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

namespace tsk {

constexpr int k_max_desciptors = 12;
constexpr int k_max_program_ds_sets = 8;
constexpr int k_max_program_set_bindings = 16;
constexpr int k_max_pc_ranges = 1;

struct TextureVk {
  VkExtent3D extent;
  VkFormat format;

  VkImage image;
  VkImageView image_view;
  VmaAllocation allocation;

  /*@returns 'true' if the texture is valid and ready for usage.*/
  inline const bool valid() const {
    return image != VK_NULL_HANDLE && image_view != VK_NULL_HANDLE;
  }

  void create(VkImageUsageFlags usage,
              VkExtent3D extent,
              VkFormat format,
              VkImageAspectFlags aspect);

  void update(VkCommandBuffer cmd, uint32_t offset, uint32_t size, void* data);

  void destroy();
};

struct ShaderVk {
  VkShaderModule module = VK_NULL_HANDLE;

  VkDescriptorSetLayoutBinding bindings[k_max_program_set_bindings];
  uint32_t n_bindings = 0;

  VkPushConstantRange pc_ranges[k_max_pc_ranges];
  uint32_t n_pc_ranges = 0;

  // @returns 'true' if the shader is valid and ready for usage.
  inline const bool valid() const { return module != VK_NULL_HANDLE; }

  void create(const char* path);

  void destroy();
};

struct BufferVk {
 public:
  VkBuffer buffer = VK_NULL_HANDLE;
  VkDeviceAddress address = -1;

  /*@returns 'true' if the buffer is valid and ready for usage.*/
  inline const bool valid() const { return buffer != VK_NULL_HANDLE; }

  /*@returns The valid size of the bufer.*/
  inline const VkDeviceSize size() const { return device_size; }

  /*@returns The actual size allocated by the buffer.*/
  /**/
  /*@note Any padding is considered unusable. */
  inline const VkDeviceSize allocated_size() const;

  void create(VkBufferUsageFlags usage,
              VkDeviceSize buffer_size,
              bool mappable = false);

  void update(VkCommandBuffer cmd, uint32_t offset, uint32_t size, void* data);

  void destroy();

 private:
  VmaAllocation allocation = VK_NULL_HANDLE;
  VkDeviceSize device_size = 0;
};

/*@brief Defines the state and required to create and identify a pipeline.*/
struct ProgramVk {
  VkDescriptorSetLayout descriptor_set_layout;
  VkPipelineLayout pipeline_layout;

  uint8_t n_bindings = 0;
  uint8_t n_pc_ranges = 0;

  /*@returns 'true' of the texture is valid and ready for usage.*/
  inline const bool valid() const {
    return pipeline_layout != VK_NULL_HANDLE &&
           descriptor_set_layout != VK_NULL_HANDLE;
  }

  void create(const ShaderVk& cs);
  void create(const ShaderVk& vs, const ShaderVk& fs);

  void destroy();
};

// Vulkan core.
extern VkInstance instance;
extern VkPhysicalDevice physical_device;
extern VkDevice device;

// Queues.
extern VkQueue graphics_queue;
extern uint32_t graphics_queue_index;

extern VkDescriptorPool descriptor_pool;

// ImGui draws.
extern void (*imgui_draw_fn)(VkCommandBuffer);

};  // namespace tsk

#endif
