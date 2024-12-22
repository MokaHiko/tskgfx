/**
 * @file renderer.h
 * @brief This file contains the definition for the tgfx renderer.
 *
 * This library is responsible for managing application configuration, 
 * rendering, and other functionalities within the TUSK framework.
 * 
 * @author Moka
 * @date 2024-11-03
 */

#ifndef RENDERER_H_
#define RENDERER_H_

#include <tskgfx/tskgfx.h>

namespace tsk {

/*@brief Enum class representing flags for dirty states in the rendering context.*/
/*This enum is used to track changes in various rendering settings. Each flag corresponds*/
/*to a specific aspect of the rendering context that, when modified, may require updates */
/*to the rendering pipeline. For instance, a change in the viewport settings or the swapchain */
/*might necessitate a rebuild or reconfiguration.*/
/*The flags can be combined using bitwise operations to track multiple changes at once.*/
enum class RenderContextDirtyFlags : uint32_t {
  k_none = 0,                // Clean.
  k_swapchain = 1 << 0,      // Swapchain needs to be rebuilt
  k_msaa = 1 << 1,           // MSAA settings changed
  k_viewport = 1 << 2,       // Viewport settings changed
  k_scissor = 1 << 3,        // Scissor settings changed
  k_shader = 1 << 4,         // Shader settings changed
  k_render_target = 1 << 5,  // Render target changed
  k_depth_stencil = 1 << 6,  // Depth or stencil settings changed
  k_texture = 1 << 7,        // Texture state changed
  k_material = 1 << 8,       // Material properties changed
  k_lighting = 1 << 9,       // Lighting settings changed
  k_clear_values = 1 << 10,  // Clear values changed
  k_depth_range = 1 << 11,   // Depth range (near/far plane) changed

  k_all = 0xFFFFFFFF  // All flags set
};

inline RenderContextDirtyFlags operator~(RenderContextDirtyFlags a) {
  return (RenderContextDirtyFlags) ~(int)a;
}
inline RenderContextDirtyFlags operator|(RenderContextDirtyFlags a,
                                         RenderContextDirtyFlags b) {
  return (RenderContextDirtyFlags)((int)a | (int)b);
}
inline RenderContextDirtyFlags operator&(RenderContextDirtyFlags a,
                                         RenderContextDirtyFlags b) {
  return (RenderContextDirtyFlags)((int)a & (int)b);
}
inline RenderContextDirtyFlags operator^(RenderContextDirtyFlags a,
                                         RenderContextDirtyFlags b) {
  return (RenderContextDirtyFlags)((int)a ^ (int)b);
}
inline RenderContextDirtyFlags& operator|=(RenderContextDirtyFlags& a,
                                           RenderContextDirtyFlags b) {
  return (RenderContextDirtyFlags&)((int&)a |= (int)b);
}
inline RenderContextDirtyFlags& operator&=(RenderContextDirtyFlags& a,
                                           RenderContextDirtyFlags b) {
  return (RenderContextDirtyFlags&)((int&)a &= (int)b);
}
inline RenderContextDirtyFlags& operator^=(RenderContextDirtyFlags& a,
                                           RenderContextDirtyFlags b) {
  return (RenderContextDirtyFlags&)((int&)a ^= (int)b);
}

class TUSK_NO_VTABLE RenderContextI {
 public:
  virtual ~RenderContextI() = 0;

  virtual bool init(const AppConfig& config) = 0;
  virtual void shutdown() = 0;
  virtual void frame() = 0;

  virtual void create_texture_2d(TextureHandle th, const TextureInfo& info) = 0;
  virtual void update_texture_2d(TextureHandle th,
                                 uint32_t offset,
                                 uint32_t size,
                                 void* data);
  virtual void destroy(TextureHandle th) = 0;

  virtual void create_shader(ShaderHandle sh, const char* path) = 0;
  virtual void destroy(ShaderHandle sh) = 0;

  virtual void create_program(ProgramHandle ph, ShaderHandle csh) = 0;
  virtual void create_program(ProgramHandle ph,
                              ShaderHandle vsh,
                              ShaderHandle fsh) = 0;
  virtual void destroy(ProgramHandle ph) = 0;

  virtual void create_descriptor(DescriptorHandle dh,
                                 DescriptorType type,
                                 uint16_t rh,
                                 const char* name) = 0;

  virtual void create_uniform_buffer(BufferHandle bh,
                                     uint32_t size,
                                     void* data) = 0;
  virtual void create_vertex_buffer(BufferHandle bh,
                                    VertexLayoutHandle vlh,
                                    uint32_t size,
                                    void* data) = 0;
  virtual void create_index_buffer(BufferHandle bh,
                                   uint32_t size,
                                   void* data) = 0;
  virtual void update_buffer(BufferHandle bh,
                             uint32_t offset,
                             uint32_t size,
                             void* data) = 0;
  virtual void destroy(BufferHandle bh) = 0;

  virtual void submit(Frame* frame) = 0;
};

inline RenderContextI::~RenderContextI() = default;

RenderContextI* create_render_context();

}  // namespace tsk

#endif
