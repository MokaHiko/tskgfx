/** @file tskgfx.h
 * @brief This file contains the declarations for the tskgfx library.
 *
 * This library is responsible for managing application configuration,
 * rendering, and other functionalities within the TUSK framework.
 *
 * @author Moka
 * @date 2024-11-03
 */

#ifndef TGFX_H_
#define TGFX_H_

#include <tsk/tsk.h>

namespace tsk {

constexpr uint8_t k_frame_overlap = 2;
constexpr uint32_t k_max_draws = 256;

/* @enum Format*/
/* @brief Represents texture and pixel formats used in the renderer.*/
/**/
/* This enumeration defines various texture and pixel formats, specifying the */
/* layout and packing of pixel data for efficient rendering.*/
/**/
/* @note Each format in this enum corresponds one-to-one with Vulkan's
 * `VkFormat`, */
/* allowing for direct mapping and compatibility when working with Vulkan.*/
enum class Format {
  k_undefined = 0,
  k_r4g4_unorm_pack8 = 1,
  k_r4g4b4a4_unorm_pack16 = 2,
  k_b4g4r4a4_unorm_pack16 = 3,
  k_r5g6b5_unorm_pack16 = 4,
  k_b5g6r5_unorm_pack16 = 5,
  k_r5g5b5a1_unorm_pack16 = 6,
  k_b5g5r5a1_unorm_pack16 = 7,
  k_a1r5g5b5_unorm_pack16 = 8,
  k_r8_unorm = 9,
  k_r8_snorm = 10,
  k_r8_uscaled = 11,
  k_r8_sscaled = 12,
  k_r8_uint = 13,
  k_r8_sint = 14,
  k_r8_srgb = 15,
  k_r8g8_unorm = 16,
  k_r8g8_snorm = 17,
  k_r8g8_uscaled = 18,
  k_r8g8_sscaled = 19,
  k_r8g8_uint = 20,
  k_r8g8_sint = 21,
  k_r8g8_srgb = 22,
  k_r8g8b8_unorm = 23,
  k_r8g8b8_snorm = 24,
  k_r8g8b8_uscaled = 25,
  k_r8g8b8_sscaled = 26,
  k_r8g8b8_uint = 27,
  k_r8g8b8_sint = 28,
  k_r8g8b8_srgb = 29,
  k_b8g8r8_unorm = 30,
  k_b8g8r8_snorm = 31,
  k_b8g8r8_uscaled = 32,
  k_b8g8r8_sscaled = 33,
  k_b8g8r8_uint = 34,
  k_b8g8r8_sint = 35,
  k_b8g8r8_srgb = 36,
  k_r8g8b8a8_unorm = 37,
  k_r8g8b8a8_snorm = 38,
  k_r8g8b8a8_uscaled = 39,
  k_r8g8b8a8_sscaled = 40,
  k_r8g8b8a8_uint = 41,
  k_r8g8b8a8_sint = 42,
  k_r8g8b8a8_srgb = 43,
  k_b8g8r8a8_unorm = 44,
  k_b8g8r8a8_snorm = 45,
  k_b8g8r8a8_uscaled = 46,
  k_b8g8r8a8_sscaled = 47,
  k_b8g8r8a8_uint = 48,
  k_b8g8r8a8_sint = 49,
  k_b8g8r8a8_srgb = 50,
  k_a8b8g8r8_unorm_pack32 = 51,
  k_a8b8g8r8_snorm_pack32 = 52,
  k_a8b8g8r8_uscaled_pack32 = 53,
  k_a8b8g8r8_sscaled_pack32 = 54,
  k_a8b8g8r8_uint_pack32 = 55,
  k_a8b8g8r8_sint_pack32 = 56,
  k_a8b8g8r8_srgb_pack32 = 57,
  k_a2r10g10b10_unorm_pack32 = 58,
  k_a2r10g10b10_snorm_pack32 = 59,
  k_a2r10g10b10_uscaled_pack32 = 60,
  k_a2r10g10b10_sscaled_pack32 = 61,
  k_a2r10g10b10_uint_pack32 = 62,
  k_a2r10g10b10_sint_pack32 = 63,
  k_a2b10g10r10_unorm_pack32 = 64,
  k_a2b10g10r10_snorm_pack32 = 65,
  k_a2b10g10r10_uscaled_pack32 = 66,
  k_a2b10g10r10_sscaled_pack32 = 67,
  k_a2b10g10r10_uint_pack32 = 68,
  k_a2b10g10r10_sint_pack32 = 69,
  k_r16_unorm = 70,
  k_r16_snorm = 71,
  k_r16_uscaled = 72,
  k_r16_sscaled = 73,
  k_r16_uint = 74,
  k_r16_sint = 75,
  k_r16_sfloat = 76,
  k_r16g16_unorm = 77,
  k_r16g16_snorm = 78,
  k_r16g16_uscaled = 79,
  k_r16g16_sscaled = 80,
  k_r16g16_uint = 81,
  k_r16g16_sint = 82,
  k_r16g16_sfloat = 83,
  k_r16g16b16_unorm = 84,
  k_r16g16b16_snorm = 85,
  k_r16g16b16_uscaled = 86,
  k_r16g16b16_sscaled = 87,
  k_r16g16b16_uint = 88,
  k_r16g16b16_sint = 89,
  k_r16g16b16_sfloat = 90,
  k_r16g16b16a16_unorm = 91,
  k_r16g16b16a16_snorm = 92,
  k_r16g16b16a16_uscaled = 93,
  k_r16g16b16a16_sscaled = 94,
  k_r16g16b16a16_uint = 95,
  k_r16g16b16a16_sint = 96,
  k_r16g16b16a16_sfloat = 97,
  k_r32_uint = 98,
  k_r32_sint = 99,
  k_r32_sfloat = 100,
  k_r32g32_uint = 101,
  k_r32g32_sint = 102,
  k_r32g32_sfloat = 103,
  k_r32g32b32_uint = 104,
  k_r32g32b32_sint = 105,
  k_r32g32b32_sfloat = 106,
  k_r32g32b32a32_uint = 107,
  k_r32g32b32a32_sint = 108,
  k_r32g32b32a32_sfloat = 109,
  k_r64_uint = 110,
  k_r64_sint = 111,
  k_r64_sfloat = 112,
  k_r64g64_uint = 113,
  k_r64g64_sint = 114,
  k_r64g64_sfloat = 115,
  k_r64g64b64_uint = 116,
  k_r64g64b64_sint = 117,
  k_r64g64b64_sfloat = 118,
  k_r64g64b64a64_uint = 119,
  k_r64g64b64a64_sint = 120,
  k_r64g64b64a64_sfloat = 121,
  k_b10g11r11_ufloat_pack32 = 122,
  k_e5b9g9r9_ufloat_pack32 = 123,
  k_d16_unorm = 124,
  k_x8_d24_unorm_pack32 = 125,
  k_d32_sfloat = 126,
  k_s8_uint = 127,
  k_d16_unorm_s8_uint = 128,
  k_d24_unorm_s8_uint = 129,
  k_d32_sfloat_s8_uint = 130,
  k_bc1_rgb_unorm_block = 131,
  k_bc1_rgb_srgb_block = 132,
  k_bc1_rgba_unorm_block = 133,
  k_bc1_rgba_srgb_block = 134,
  k_bc2_unorm_block = 135,
  k_bc2_srgb_block = 136,
  k_bc3_unorm_block = 137,
  k_bc3_srgb_block = 138,
  k_bc4_unorm_block = 139,
  k_bc4_snorm_block = 140,
  k_bc5_unorm_block = 141,
  k_bc5_snorm_block = 142,
  k_bc6h_ufloat_block = 143,
  k_bc6h_sfloat_block = 144,
  k_bc7_unorm_block = 145,
  k_bc7_srgb_block = 146,
  k_etc2_r8g8b8_unorm_block = 147,
  k_etc2_r8g8b8_srgb_block = 148,
  k_etc2_r8g8b8a1_unorm_block = 149,
  k_etc2_r8g8b8a1_srgb_block = 150,
  k_etc2_r8g8b8a8_unorm_block = 151,
  k_etc2_r8g8b8a8_srgb_block = 152,
  k_eac_r11_unorm_block = 153,
  k_eac_r11_snorm_block = 154,
  k_eac_r11g11_unorm_block = 155,
  k_eac_r11g11_snorm_block = 156,
  k_astc_4x4_unorm_block = 157,
  k_astc_4x4_srgb_block = 158,
  k_astc_5x4_unorm_block = 159,
  k_astc_5x4_srgb_block = 160,
  k_astc_5x5_unorm_block = 161,
  k_astc_5x5_srgb_block = 162,
  k_astc_6x5_unorm_block = 163,
  k_astc_6x5_srgb_block = 164,
  k_astc_6x6_unorm_block = 165,
  k_astc_6x6_srgb_block = 166,
  k_astc_8x5_unorm_block = 167,
  k_astc_8x5_srgb_block = 168,
  k_astc_8x6_unorm_block = 169,
  k_astc_8x6_srgb_block = 170,
  k_astc_8x8_unorm_block = 171,
  k_astc_8x8_srgb_block = 172,
  k_astc_10x5_unorm_block = 173,
  k_astc_10x5_srgb_block = 174,
  k_astc_10x6_unorm_block = 175,
  k_astc_10x6_srgb_block = 176,
  k_astc_10x8_unorm_block = 177,
  k_astc_10x8_srgb_block = 178,
  k_astc_10x10_unorm_block = 179,
  k_astc_10x10_srgb_block = 180,
  k_astc_12x10_unorm_block = 181,
  k_astc_12x10_srgb_block = 182,
  k_astc_12x12_unorm_block = 183,
  k_astc_12x12_srgb_block = 184,
  k_g8b8g8r8_422_unorm = 1000156000,
  k_b8g8r8g8_422_unorm = 1000156001,
  k_g8_b8_r8_3plane_420_unorm = 1000156002,
  k_g8_b8r8_2plane_420_unorm = 1000156003,
  k_g8_b8_r8_3plane_422_unorm = 1000156004,
  k_g8_b8r8_2plane_422_unorm = 1000156005,
  k_g8_b8_r8_3plane_444_unorm = 1000156006,
  k_r10x6_unorm_pack16 = 1000156007,
  k_r10x6g10x6_unorm_2pack16 = 1000156008,
  k_r10x6g10x6b10x6a10x6_unorm_4pack16 = 1000156009,
  k_g10x6b10x6g10x6r10x6_422_unorm_4pack16 = 1000156010,
  k_b10x6g10x6r10x6g10x6_422_unorm_4pack16 = 1000156011,
  k_g10x6_b10x6_r10x6_3plane_420_unorm_3pack16 = 1000156012,
  k_g10x6_b10x6r10x6_2plane_420_unorm_3pack16 = 1000156013,
  k_g10x6_b10x6_r10x6_3plane_422_unorm_3pack16 = 1000156014,
  k_g10x6_b10x6r10x6_2plane_422_unorm_3pack16 = 1000156015,
  k_g10x6_b10x6_r10x6_3plane_444_unorm_3pack16 = 1000156016,
  k_r12x4_unorm_pack16 = 1000156017,
  k_r12x4g12x4_unorm_2pack16 = 1000156018,
  k_r12x4g12x4b12x4a12x4_unorm_4pack16 = 1000156019,
  k_g12x4b12x4g12x4r12x4_422_unorm_4pack16 = 1000156020,
  k_b12x4g12x4r12x4g12x4_422_unorm_4pack16 = 1000156021,
  k_g12x4_b12x4_r12x4_3plane_420_unorm_3pack16 = 1000156022,
  k_g12x4_b12x4r12x4_2plane_420_unorm_3pack16 = 1000156023,
  k_g12x4_b12x4_r12x4_3plane_422_unorm_3pack16 = 1000156024,
  k_g12x4_b12x4r12x4_2plane_422_unorm_3pack16 = 1000156025,
  k_g12x4_b12x4_r12x4_3plane_444_unorm_3pack16 = 1000156026,
  k_g16b16g16r16_422_unorm = 1000156027,
  k_b16g16r16g16_422_unorm = 1000156028,
  k_g16_b16_r16_3plane_420_unorm = 1000156029,
  k_g16_b16r16_2plane_420_unorm = 1000156030,
  k_g16_b16_r16_3plane_422_unorm = 1000156031,
  k_g16_b16r16_2plane_422_unorm = 1000156032,
  k_g16_b16_r16_3plane_444_unorm = 1000156033,
  k_g8_b8r8_2plane_444_unorm = 1000330000,
  k_g10x6_b10x6r10x6_2plane_444_unorm_3pack16 = 1000330001,
  k_g12x4_b12x4r12x4_2plane_444_unorm_3pack16 = 1000330002,
  k_g16_b16r16_2plane_444_unorm = 1000330003,
  k_a4r4g4b4_unorm_pack16 = 1000340000,
  k_a4b4g4r4_unorm_pack16 = 1000340001,
  k_astc_4x4_sfloat_block = 1000066000,
  k_astc_5x4_sfloat_block = 1000066001,
  k_astc_5x5_sfloat_block = 1000066002,
  k_astc_6x5_sfloat_block = 1000066003,
  k_astc_6x6_sfloat_block = 1000066004,
  k_astc_8x5_sfloat_block = 1000066005,
  k_astc_8x6_sfloat_block = 1000066006,
  k_astc_8x8_sfloat_block = 1000066007,
  k_astc_10x5_sfloat_block = 1000066008,
  k_astc_10x6_sfloat_block = 1000066009,
  k_astc_10x8_sfloat_block = 1000066010,
  k_astc_10x10_sfloat_block = 1000066011,
  k_astc_12x10_sfloat_block = 1000066012,
  k_astc_12x12_sfloat_block = 1000066013,
  k_pvrtc1_2bpp_unorm_block_img = 1000054000,
  k_pvrtc1_4bpp_unorm_block_img = 1000054001,
  k_pvrtc2_2bpp_unorm_block_img = 1000054002,
  k_pvrtc2_4bpp_unorm_block_img = 1000054003,
  k_pvrtc1_2bpp_srgb_block_img = 1000054004,
  k_pvrtc1_4bpp_srgb_block_img = 1000054005,
  k_pvrtc2_2bpp_srgb_block_img = 1000054006,
  k_pvrtc2_4bpp_srgb_block_img = 1000054007,
  k_r16g16_sfixed5_nv = 1000464000,
  k_a1b5g5r5_unorm_pack16_khr = 1000470000,
  k_a8_unorm_khr = 1000470001,

  k_max_enum = 0x7fffffff
};

/* @enum Descriptor Type*/
/* @brief Represents descriptor types used in programs. */
/**/
/**/
/* @note Each format in this enum corresponds one-to-one with Vulkan's
 * `VkDescriptorType`, */
/* allowing for direct mapping and compatibility when working with Vulkan.*/
enum class DescriptorType : uint32_t {
  k_sampler = 0,
  k_combined_image_sampler = 1,
  k_sampled_image = 2,
  k_storage_image = 3,
  k_uniform_texel_buffer = 4,
  k_storage_texel_buffer = 5,
  k_uniform_buffer = 6,
  k_storage_buffer = 7,
  k_uniform_buffer_dynamic = 8,
  k_storage_buffer_dynamic = 9,
  k_input_attachment = 10,

  k_max_enum = 0x7fffffff
};

/* @brief Holds metadata about a texture, including its format, size, and layout
 * properties.*/
/**/
/* This structure encapsulates information required to describe a texture's
 * layout, */
/* memory requirements, and format. It provides essential details for managing
 * textures */
/* within the rendering engine.*/
struct TUSK_API TextureInfo {
  Format format;  //!< Texture format.
  /*uint32_t storage_size;		//!< total amount of bytes required to
   * store texture.*/
  uint16_t width;       //!< texture width.
  uint16_t height;      //!< texture height.
  uint16_t depth;       //!< texture depth.
  uint16_t num_layers;  //!< number of layers in texture array.
  uint8_t num_mips;     //!< number of MIP maps.
  /*uint8_t bits_per_pixel;		//!< format bits per pixel.*/
  bool cube_map;  //!< texture is cubemap.
};

/// @brief Structure to hold application configuration settings.
///
/// This structure contains parameters that define the configuration for the
/// tgfx application.
///
/// @struct AppConfig
/// @var AppConfig::app_name
/// Application name, which is a string of maximum length 256 characters.
///
/// @var AppConfig::nwh
/// Native window handle, a pointer to the window created by the application.
///
/// @var AppConfig::ndt
/// Native display type, a pointer used for platform-specific display
/// information.
///
/// @var AppConfig::width
/// Width of the application window in pixels.
///
/// @var AppConfig::height
/// Height of the application window in pixels.
struct TUSK_API AppConfig {
  char app_name[256];
  void *nwh;
  void *ndt;
  int width;
  int height;
};

/// @brief Initializes the tgfx library.
///
/// @param[in] app_config Initialization parameters.
/// @returns `true` if initialization was successful.
[[nodiscard]] TUSK_API bool init(const AppConfig &app_config);

/// @brief Advance to next frame.
///
/// When using multithreaded renderer, this call just swaps internal buffers,
/// kicks render thread, and returns. In singlethreaded renderer this call does
/// frame rendering.
TUSK_API void frame();

/// @brief Shuts down the tgfx library.
///
/// @attention This function releases all resources allocated by the tgfx
/// library.
TUSK_API void shutdown();

static const uint16_t k_invalid_handle = UINT16_MAX;

#define TUSK_HANDLE(name)                       \
  struct name {                                 \
    uint16_t idx = tsk::k_invalid_handle;       \
    inline operator uint16_t() const {          \
      return idx;                               \
    }                                           \
  };                                            \
  inline bool is_valid(name handle) {           \
    return tsk::k_invalid_handle != handle.idx; \
  }

#define TUSK_INVALID_HANDLE {tsk::k_invalid_handle}

TUSK_HANDLE(ProgramHandle);
TUSK_HANDLE(DescriptorHandle);
TUSK_HANDLE(ShaderHandle);
TUSK_HANDLE(BufferHandle);
TUSK_HANDLE(TextureHandle);
TUSK_HANDLE(VertexLayoutHandle);
TUSK_HANDLE(FrameBufferHandle);

/// @brief Creates and caches a shader program.
///
/// @param[in] path Shader spriv.
/// @returns program Reference to shader that was created.
TUSK_API ShaderHandle create_shader(const char *path);

TUSK_API void destroy(ShaderHandle sh);

/// @brief Creates a compute shader.
///
/// @param[in] path Handle to compute shader.
/// @returns program Reference to program that was created.
TUSK_API ProgramHandle create_program(ShaderHandle csh);

/// @brief Creates vertex shader.
///
/// @param[in] path Handle to vertex shader.
/// @param[in] path Handle to fragment shader.
/// @returns program Reference to program was created.
///
/// @note This function will create a program if at least one vsh/fsh path is
/// passed.
TUSK_API ProgramHandle create_program(ShaderHandle vsh, ShaderHandle fsh);

TUSK_API void destroy(ProgramHandle ph);

/// @brief Creates descriptor.
///
/// @param[in] path Unique name of descriptor.
/// @param[in] path Descriptor type.
/// @param[in] path Handle to resource.
/// @returns program Reference to descriptor created.
TUSK_API DescriptorHandle create_descriptor(const char *name,
                                            DescriptorType type,
                                            uint16_t rh);

// TODO: Make buffer *data const if not owning.
/// @brief Creates descriptor.
///
/// @param[in] size in bytes of the buffer.
/// @param[in] data that will be copied to the buffer.
/// @returns buffer Reference to the buffer created.
///
/// @note If data ownership not passed, it must exist for atleast one frame
/// (call to tgfx::frame).
TUSK_API BufferHandle create_uniform_buffer(uint32_t size, void *data);

TUSK_API BufferHandle create_vertex_buffer(VertexLayoutHandle vlh,
                                           uint32_t size,
                                           void *data);

TUSK_API BufferHandle create_index_buffer(uint32_t size, void *data);

/// @brief Updates a buffers data.
///
/// @param[in] buffer_handle Buffer handle.
/// @param[in] offset in the buffer to copy the data.
/// @param[in] data Pointer to the new buffer data.
TUSK_API void update(BufferHandle bh,
                     uint32_t offset,
                     uint32_t size,
                     void *data);

/*/// @brief Updates a buffers data and handles the lifetime of data.*/
/*///*/
/*/// @param[in] buffer_handle Buffer handle. */
/*/// @param[in] offset in the buffer to copy the data.*/
/*/// @param[in] data Pointer to the new buffer data.*/
/*TUSK_API void update(BufferHandle bh, uint32_t offset, uint32_t size, void*
 * data, bool manage = false);*/

/// @brief Destroys a buffer.
///
/// @param[in] buffer_handle Buffer handle.
TUSK_API void destroy(BufferHandle bh);

/// @brief Creates a texture given info.
///
/// @param[in] info The parameters that define the texture.
/// @returns texure Reference to texture that was created.
TUSK_API TextureHandle create_texture_2d(const TextureInfo &info);

TUSK_API void update(TextureHandle th,
                     uint32_t offset,
                     uint32_t size,
                     void *data);

// @brief Releases the resources a texture.
//
/// @param[in] handle Handle to texture that will be invalidated.
TUSK_API void destroy(TextureHandle th);

/// @brief Binds view-projection matrix to draw call.
///
/// @param[in] Ptr to view-projection matrix.
///
/// @note The matrix pass must be a float[16] or equivalent.
TUSK_API void set_view_proj(const void *mtx);

TUSK_API void set_camera_pos(const void *camera_pos);

/// @brief Binds transform matrix to draw call.
///
/// @param[in] Ptr to transform matrix.
///
/// @note The matrix pass must be a float[16] or equivalent.
TUSK_API void set_transform(const void *mtx);

TUSK_API void set_vertex_buffer(BufferHandle vbh);

TUSK_API void set_index_buffer(BufferHandle ibh);

TUSK_API void set_descriptor(DescriptorHandle dh);

TUSK_API void submit(ProgramHandle ph);

// ~ TODO: Internal ~

/* @brief Holds metadata about a descritpor. */
/**/
struct DescriptorInfo {
  DescriptorType type = DescriptorType::k_max_enum;
  char name[256];
  uint16_t resource_handle_index;  //!< handle index to the resource this
                                   //!< descriptor is bound to.

  inline const bool valid() const {
    return type != DescriptorType::k_max_enum ||
           resource_handle_index == k_invalid_handle;
  }
};

struct Rect2D {
  float x, y;
  float width, height;
};

/* @brief Holds rendering information for a draw call, including transformation
 * matrices */
/* and buffer handles.*/
/**/
/* This structure encapsulates details required to execute a draw call, such as
 */
/* view and transformation matrices, vertex, index, and instance buffer
 * handles.*/
/* It provides essential data for managing draw operations within the rendering
 * engine.*/
struct RenderDraw {
  RenderDraw() { clear(); }

  float viewproj_mtx[16];
  float transform_matrix[16];

  float camera_pos[3];

  BufferHandle vbh;
  BufferHandle ibh;
  BufferHandle instbh;

  ProgramHandle ph;

  uint32_t dh_count;
  DescriptorHandle dhs[16];

  Rect2D viewport = {0.0f, 0.0f};

  void clear() {
    viewproj_mtx[0] = 1.0f, viewproj_mtx[1] = 0.0f, viewproj_mtx[2] = 0.0f,
    viewproj_mtx[3] = 0.0f;  // 1st column
    viewproj_mtx[4] = 0.0f, viewproj_mtx[5] = 1.0f, viewproj_mtx[6] = 0.0f,
    viewproj_mtx[7] = 0.0f;  // 2nd column
    viewproj_mtx[8] = 0.0f, viewproj_mtx[9] = 0.0f, viewproj_mtx[10] = 1.0f,
    viewproj_mtx[11] = 0.0f;  // 3rd column
    viewproj_mtx[12] = 0.0f, viewproj_mtx[13] = 0.0f, viewproj_mtx[14] = 0.0f,
    viewproj_mtx[15] = 1.0f;  // 4th column

    transform_matrix[0] = 1.0f, transform_matrix[1] = 0.0f,
    transform_matrix[2] = 0.0f, transform_matrix[3] = 0.0f;  // 1st column
    transform_matrix[4] = 0.0f, transform_matrix[5] = 1.0f,
    transform_matrix[6] = 0.0f, transform_matrix[7] = 0.0f;  // 2nd column
    transform_matrix[8] = 0.0f, transform_matrix[9] = 0.0f,
    transform_matrix[10] = 1.0f, transform_matrix[11] = 0.0f;  // 3rd column
    transform_matrix[12] = 0.0f, transform_matrix[13] = 0.0f,
    transform_matrix[14] = 0.0f, transform_matrix[15] = 1.0f;  // 4th column

    camera_pos[0] = 0;
    camera_pos[1] = 0;
    camera_pos[2] = 0;

    vbh = TUSK_INVALID_HANDLE;
    ibh = TUSK_INVALID_HANDLE;
    instbh = TUSK_INVALID_HANDLE;

    ph = TUSK_INVALID_HANDLE;

    dh_count = 0;
    // TODO: Change to memset in impl.
    for (auto &dh : dhs) {
      dh = TUSK_INVALID_HANDLE;
    }
  };
};

struct Frame {
  uint32_t draw_count = 0;
  RenderDraw draws[k_max_draws] = {};
};

struct FrameBuffer {
  constexpr static int k_max_color_attachments = 2;

  Rect2D render_area;
  uint32_t layer_count;
  uint32_t view_mask;
  uint32_t color_attachment_count;
  tsk::TextureHandle color_attachments[k_max_color_attachments];
  tsk::TextureHandle depth_attachment;
  tsk::TextureHandle stencil_attachment;
};

}  // namespace tsk

#endif
