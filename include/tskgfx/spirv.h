/**
 * @file renderer.h
 * @brief This file contains the utilities for spirv.
 *
 * @author Moka
 * @date 2024-11-03
 */

#ifndef SPRIV_H_
#define SPRIV_H_

#include <vulkan/vulkan_core.h>

namespace tsk {

bool parse_spirv(const void* spirv_code,
                 size_t spirv_nbytes,
                 VkDescriptorSetLayoutBinding* bindings,
                 uint32_t* n_bindings,
                 VkPushConstantRange* push_constant_ranges,
                 uint32_t* n_push_constant_ranges);

}  // namespace tsk

#endif
