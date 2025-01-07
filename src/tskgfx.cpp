#include "tskgfx/tskgfx.h"

#include <spdlog/spdlog.h>

#include <cassert>
#include <cstring>

#include "tskgfx/renderer.h"

#ifdef TUSK_DEBUG
#define TUSK_GFX_ASSERT(x, msg) \
  if (!(x)) {                   \
    spdlog::error(msg);         \
    assert(x);                  \
  }
#else

#define TUSK_GFX_ASSERT(x, msg)

#endif

namespace tsk {

static Frame s_frame = {};
static RenderContextI* s_ctx;

bool init(const AppConfig& app_config) {
  s_ctx = create_render_context();

  return s_ctx->init(app_config);
}

void frame() {
  s_ctx->submit(&s_frame);
  s_ctx->frame();
}

void shutdown() {
  s_ctx->shutdown();
  delete s_ctx;
}

static TextureHandle th;
TextureHandle create_texture_2d(const TextureInfo& info) {
  th.idx++;
  s_ctx->create_texture_2d(th, info);
  return th;
}

void update(TextureHandle th, uint32_t offset, uint32_t size, void* data) {
  s_ctx->update_texture_2d(th, offset, size, data);
}

void destroy(TextureHandle th) {
  s_ctx->destroy(th);
}

static ShaderHandle sh;
ShaderHandle create_shader(const char* path) {
  sh.idx++;

  s_ctx->create_shader(sh, path);

  return sh;
}

void destroy(ShaderHandle sh) {
  s_ctx->destroy(sh);
}

static ProgramHandle ph;
ProgramHandle create_program(ShaderHandle csh) {
  ph.idx++;

  s_ctx->create_program(ph, csh);

  return ph;
}

ProgramHandle create_program(ShaderHandle vsh, ShaderHandle fsh) {
  ph.idx++;

  s_ctx->create_program(ph, vsh, fsh);

  return ph;
}

TUSK_API void destroy(ProgramHandle ph) {
  s_ctx->destroy(ph);
}

static DescriptorHandle dh;
DescriptorHandle create_descriptor(const char* name,
                                   DescriptorType type,
                                   uint16_t rh) {
  dh.idx++;
  s_ctx->create_descriptor(dh, type, rh, name);
  return dh;
}

TUSK_API void destroy(DescriptorHandle dh) {}

static BufferHandle bh;
BufferHandle create_uniform_buffer(uint32_t size, void* data) {
  bh.idx++;

  s_ctx->create_uniform_buffer(bh, size, data);
  s_ctx->update_buffer(bh, 0, size, data);

  return bh;
};

BufferHandle create_vertex_buffer(VertexLayoutHandle vlh,
                                  uint32_t size,
                                  void* data) {
  bh.idx++;

  s_ctx->create_vertex_buffer(bh, vlh, size, data);
  s_ctx->update_buffer(bh, 0, size, data);

  return bh;
}

BufferHandle create_index_buffer(uint32_t size, void* data) {
  bh.idx++;

  s_ctx->create_index_buffer(bh, size, data);
  s_ctx->update_buffer(bh, 0, size, data);

  return bh;
}

void update(BufferHandle bh, uint32_t offset, uint32_t size, void* data) {
  TUSK_GFX_ASSERT(bh.idx != k_invalid_handle,
                  "Cannot updated invalid buffer handle!");
  TUSK_GFX_ASSERT(data != nullptr && size > 0,
                  "Data must be non null and non zero size!");

  s_ctx->update_buffer(bh, offset, size, data);
}

void destroy(BufferHandle bh) {
  TUSK_GFX_ASSERT(bh.idx != k_invalid_handle,
                  "Cannot destroy invalid buffer handle!");

  s_ctx->destroy(bh);
}

void set_view_proj(const void* mtx) {
  memcpy(
      s_frame.draws[s_frame.draw_count].viewproj_mtx, mtx, sizeof(float) * 16);
}

TUSK_API void set_camera_pos(const void* camera_pos) {
  memcpy(s_frame.draws[s_frame.draw_count].camera_pos,
         camera_pos,
         sizeof(float) * 3);
}

void set_transform(const void* mtx) {
  memcpy(s_frame.draws[s_frame.draw_count].transform_matrix,
         mtx,
         sizeof(float) * 16);
}

void set_vertex_buffer(BufferHandle vbh) {
  TUSK_GFX_ASSERT(s_frame.draws[s_frame.draw_count].vbh.idx == k_invalid_handle,
                  "Vertex buffer already set for this draw call!");

  TUSK_GFX_ASSERT(vbh != k_invalid_handle,
                  "Attemping to set invalid vertex buffer!");

  s_frame.draws[s_frame.draw_count].vbh = vbh;
}

void set_index_buffer(BufferHandle ibh) {
  TUSK_GFX_ASSERT(s_frame.draws[s_frame.draw_count].ibh.idx == k_invalid_handle,
                  "Index buffer already set for this draw call!");

  TUSK_GFX_ASSERT(ibh != k_invalid_handle,
                  "Attemping to set invalid index buffer!");

  s_frame.draws[s_frame.draw_count].ibh = ibh;
}

void set_descriptor(DescriptorHandle dh) {
  TUSK_GFX_ASSERT(dh != k_invalid_handle,
                  "Attemping to bind invalid descriptor!");

  RenderDraw& draw = s_frame.draws[s_frame.draw_count];
  draw.dhs[draw.dh_count++] = dh;
}

void submit(ProgramHandle ph) {
  if (ph.idx == tsk::k_invalid_handle) {
    spdlog::error("Calling submit with invalid (ProgramHandle).");
    return;
  };

  TUSK_GFX_ASSERT(s_frame.draw_count < k_max_draws - 1,
                  "Exceeded max draws this frame!");

  s_frame.draws[s_frame.draw_count].ph = ph;
  ++s_frame.draw_count;
}

}  // namespace tsk
