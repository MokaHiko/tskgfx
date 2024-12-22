#include "tskgfx/spirv.h"

#include <spdlog/spdlog.h>
#include <spirv_reflect/spirv_reflect.h>
#include <vulkan/vulkan_core.h>

#include <cassert>
#include <unordered_map>

#include "spirv_reflect/include/spirv/unified1/spirv.h"

namespace tsk {

static constexpr const char* k_ds_type_to_string[]{
    "SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER",
    "SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER",
    "SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE",
    "SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE",
    "SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER",
    "SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER",
    "SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER",
    "SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER",
    "SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC",
    "SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC",
    "SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT",

    // "VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR",
};

std::unordered_map<VkFormat, const char*> k_fmt_to_string = {
    {VK_FORMAT_UNDEFINED, "SPV_REFLECT_FORMAT_UNDEFINED"},
    {VK_FORMAT_R16_UINT, "SPV_REFLECT_FORMAT_R16_UINT"},
    {VK_FORMAT_R16_SINT, "SPV_REFLECT_FORMAT_R16_SINT"},
    {VK_FORMAT_R16_SFLOAT, "SPV_REFLECT_FORMAT_R16_SFLOAT"},
    {VK_FORMAT_R16G16_UINT, "SPV_REFLECT_FORMAT_R16G16_UINT"},
    {VK_FORMAT_R16G16_SINT, "SPV_REFLECT_FORMAT_R16G16_SINT"},
    {VK_FORMAT_R16G16_SFLOAT, "SPV_REFLECT_FORMAT_R16G16_SFLOAT"},
    {VK_FORMAT_R16G16B16_UINT, "SPV_REFLECT_FORMAT_R16G16B16_UINT"},
    {VK_FORMAT_R16G16B16_SINT, "SPV_REFLECT_FORMAT_R16G16B16_SINT"},
    {VK_FORMAT_R16G16B16_SFLOAT, "SPV_REFLECT_FORMAT_R16G16B16_SFLOAT"},
    {VK_FORMAT_R16G16B16A16_UINT, "SPV_REFLECT_FORMAT_R16G16B16A16_UINT"},
    {VK_FORMAT_R16G16B16A16_SINT, "SPV_REFLECT_FORMAT_R16G16B16A16_SINT"},
    {VK_FORMAT_R16G16B16A16_SFLOAT, "SPV_REFLECT_FORMAT_R16G16B16A16_SFLOAT"},
    {VK_FORMAT_R32_UINT, "SPV_REFLECT_FORMAT_R32_UINT"},
    {VK_FORMAT_R32_SINT, "SPV_REFLECT_FORMAT_R32_SINT"},
    {VK_FORMAT_R32_SFLOAT, "SPV_REFLECT_FORMAT_R32_SFLOAT"},
    {VK_FORMAT_R32G32_UINT, "SPV_REFLECT_FORMAT_R32G32_UINT"},
    {VK_FORMAT_R32G32_SINT, "SPV_REFLECT_FORMAT_R32G32_SINT"},
    {VK_FORMAT_R32G32_SFLOAT, "SPV_REFLECT_FORMAT_R32G32_SFLOAT"},
    {VK_FORMAT_R32G32B32_UINT, "SPV_REFLECT_FORMAT_R32G32B32_UINT"},
    {VK_FORMAT_R32G32B32_SINT, "SPV_REFLECT_FORMAT_R32G32B32_SINT"},
    {VK_FORMAT_R32G32B32_SFLOAT, "SPV_REFLECT_FORMAT_R32G32B32_SFLOAT"},
    {VK_FORMAT_R32G32B32A32_UINT, "SPV_REFLECT_FORMAT_R32G32B32A32_UINT"},
    {VK_FORMAT_R32G32B32A32_SINT, "SPV_REFLECT_FORMAT_R32G32B32A32_SINT"},
    {VK_FORMAT_R32G32B32A32_SFLOAT, "SPV_REFLECT_FORMAT_R32G32B32A32_SFLOAT"},
    {VK_FORMAT_R64_UINT, "SPV_REFLECT_FORMAT_R64_UINT"},
    {VK_FORMAT_R64_SINT, "SPV_REFLECT_FORMAT_R64_SINT"},
    {VK_FORMAT_R64_SFLOAT, "SPV_REFLECT_FORMAT_R64_SFLOAT"},
    {VK_FORMAT_R64G64_UINT, "SPV_REFLECT_FORMAT_R64G64_UINT"},
    {VK_FORMAT_R64G64_SINT, "SPV_REFLECT_FORMAT_R64G64_SINT"},
    {VK_FORMAT_R64G64_SFLOAT, "SPV_REFLECT_FORMAT_R64G64_SFLOAT"},
    {VK_FORMAT_R64G64B64_UINT, "SPV_REFLECT_FORMAT_R64G64B64_UINT"},
    {VK_FORMAT_R64G64B64_SINT, "SPV_REFLECT_FORMAT_R64G64B64_SINT"},
    {VK_FORMAT_R64G64B64_SFLOAT, "SPV_REFLECT_FORMAT_R64G64B64_SFLOAT"},
    {VK_FORMAT_R64G64B64A64_UINT, "SPV_REFLECT_FORMAT_R64G64B64A64_UINT"},
    {VK_FORMAT_R64G64B64A64_SINT, "SPV_REFLECT_FORMAT_R64G64B64A64_SINT"},
    {VK_FORMAT_R64G64B64A64_SFLOAT, "SPV_REFLECT_FORMAT_R64G64B64A64_SFLOAT"}};

std::string type_desc_to_string(const SpvReflectTypeDescription& td) {
  std::string out = "";

  if ((td.type_flags & SPV_REFLECT_TYPE_FLAG_VECTOR) ==
      SPV_REFLECT_TYPE_FLAG_VECTOR) {
    out = "vec";
    out += std::to_string(td.traits.numeric.vector.component_count);
    return out;
  }

  if ((td.type_flags & SPV_REFLECT_TYPE_FLAG_FLOAT) ==
      SPV_REFLECT_TYPE_FLAG_FLOAT) {
    out += "float";
    out += std::to_string(td.traits.numeric.scalar.width);
    return out;
  }

  if ((td.type_flags & SPV_REFLECT_TYPE_FLAG_INT) ==
      SPV_REFLECT_TYPE_FLAG_INT) {
    out += (td.traits.numeric.scalar.signedness > 0) ? "int" : "uint";
    out += std::to_string(td.traits.numeric.scalar.width);
    return out;
  }

  if ((td.type_flags & SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) ==
      SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
    out += td.type_name ? td.type_name : "";
    return out;
  }

  return td.type_name ? td.type_name : "[Type Uknown]";
}

void parse_type_desc_recursive(const SpvReflectTypeDescription& td,
                               int level = 0) {
  std::string td_string = "";
  td_string += type_desc_to_string(td);

  std::string level_tab = "";
  for (int i = 0; i < level; i++) {
    level_tab += "\t";
  }

  spdlog::trace("{} {} {} {}", level_tab, td_string,
                td.struct_member_name ? td.struct_member_name : "",
                td.type_name ? td.type_name : "");

  for (uint32_t i = 0; i < td.member_count; i++) {
    parse_type_desc_recursive(td.members[i], level + 1);
  }
}

bool parse_spirv(const void* spirv_code,
                 size_t spirv_nbytes,
                 VkDescriptorSetLayoutBinding* bindings,
                 uint32_t* n_bindings,
                 VkPushConstantRange* pc_ranges,
                 uint32_t* n_pc_ranges) {
  // spdlog::set_level(spdlog::level::trace);

  SpvReflectShaderModule module;
  SpvReflectResult result =
      spvReflectCreateShaderModule(spirv_nbytes, spirv_code, &module);
  assert(result == SPV_REFLECT_RESULT_SUCCESS);

  if (result != SPV_REFLECT_RESULT_SUCCESS) {
    return false;
  }

  {
    uint32_t var_count = 0;
    result = spvReflectEnumerateInterfaceVariables(&module, &var_count, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    SpvReflectInterfaceVariable** interface_variables =
        (SpvReflectInterfaceVariable**)malloc(
            var_count * sizeof(SpvReflectInterfaceVariable*));
    result = spvReflectEnumerateInterfaceVariables(&module, &var_count,
                                                   interface_variables);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    std::string out_interface_string = "out ";
    std::string in_interface_string = "in ";
    for (uint32_t iv_count = 0; iv_count < var_count; iv_count++) {
      const SpvReflectInterfaceVariable& iv = *interface_variables[iv_count];

      const SpvReflectTypeDescription& iv_type_desc = *iv.type_description;

      std::string iv_string = type_desc_to_string(iv_type_desc) + " ";
      iv_string += iv.name || !strlen(iv.name) ? iv.name : "_name_";

      switch (iv.storage_class) {
        case (SpvStorageClassOutput): {
          out_interface_string += iv_string;
          out_interface_string += (iv_count < var_count - 1) ? "," : "";
        } break;

        case (SpvStorageClassInput): {
          in_interface_string += iv_string;
          in_interface_string += (iv_count < var_count - 1) ? "," : "";
        } break;

        default: {
          spdlog::error("Uknown storage type!");
        } break;
      }
    }

    spdlog::trace("{}", in_interface_string);
    spdlog::trace("{}", out_interface_string);

    free(interface_variables);
  }

  {
    assert(n_bindings != nullptr && "Must pass non null n_bindings!");
    result = spvReflectEnumerateDescriptorBindings(&module, n_bindings, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    if (bindings != nullptr) {
      SpvReflectDescriptorBinding** ds_bindings =
          (SpvReflectDescriptorBinding**)malloc(
              *n_bindings * sizeof(SpvReflectDescriptorBinding*));
      result = spvReflectEnumerateDescriptorBindings(&module, n_bindings,
                                                     ds_bindings);
      assert(result == SPV_REFLECT_RESULT_SUCCESS);

      for (uint32_t i = 0; i < *n_bindings; i++) {
        const SpvReflectDescriptorBinding& ds_binding = *ds_bindings[i];
        VkDescriptorType ds_type =
            static_cast<VkDescriptorType>(ds_binding.descriptor_type);

        bindings[i].binding = ds_binding.binding;
        bindings[i].descriptorCount = ds_binding.count;
        bindings[i].descriptorType = ds_type;

        spdlog::trace("ds({}) binding({}) type({})", ds_binding.set,
                      ds_binding.binding, k_ds_type_to_string[ds_type]);
        const SpvReflectTypeDescription& ds_type_desc =
            *ds_binding.type_description;

        if (ds_type_desc.type_name) {
          spdlog::trace("{}", ds_type_desc.type_name);
        }
        switch (ds_type) {
          case (VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER): {
            for (uint32_t member_count = 0;
                 member_count < ds_type_desc.member_count; member_count++) {
              const SpvReflectTypeDescription& member =
                  ds_type_desc.members[member_count];
              parse_type_desc_recursive(member, 1);
            }
          } break;

          case (VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER): {
            for (uint32_t member_count = 0;
                 member_count < ds_type_desc.member_count; member_count++) {
              const SpvReflectTypeDescription& member =
                  ds_type_desc.members[member_count];
              parse_type_desc_recursive(member, 1);
            }
          } break;

          default: {
            spdlog::error("Uknown descriptor type");
          } break;
        }
      }

      free(ds_bindings);
    }
  }

  {
    result = spvReflectEnumeratePushConstantBlocks(&module, n_pc_ranges, NULL);
    assert(result == SPV_REFLECT_RESULT_SUCCESS);

    if (pc_ranges != nullptr) {
      SpvReflectBlockVariable** pc_blocks = (SpvReflectBlockVariable**)malloc(
          *n_pc_ranges * sizeof(SpvReflectBlockVariable*));
      result = spvReflectEnumeratePushConstantBlocks(&module, n_pc_ranges,
                                                     pc_blocks);
      assert(result == SPV_REFLECT_RESULT_SUCCESS);

      for (uint32_t i = 0; i < *n_pc_ranges; i++) {
        const SpvReflectBlockVariable& bv = *pc_blocks[i];

        pc_ranges[i].size = bv.size;
        pc_ranges[i].offset = bv.offset;

        spdlog::trace(bv.type_description->type_name);
        for (uint32_t i = 0; i < bv.member_count; i++) {
          SpvReflectBlockVariable bm = bv.members[i];
          parse_type_desc_recursive(*bm.type_description);
        }
      }

      free(pc_blocks);
    }
  }

  spvReflectDestroyShaderModule(&module);
  return true;
}

}  // namespace tsk
