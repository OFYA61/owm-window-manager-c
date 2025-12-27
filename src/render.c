#include "render.h"

#include "display.h"
#include "drm_fourcc.h"
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

owmRenderContext OWM_RENDER_CONTEXT = { 0 };

int owmDumbFrameBuffer_create(owmDumbFrameBuffer *out) {
  owmDisplay* display = OWM_RENDER_DISPLAY.display;
  drmModeModeInfo mode = OWM_RENDER_DISPLAY.display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx];
  struct drm_mode_create_dumb create = { 0 };
  create.width = mode.hdisplay;
  create.height = mode.vdisplay;
  create.bpp = 32;

  if (drmIoctl(display->fd_card, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
    perror("drmIoctl");
    return 1;
  }

  uint32_t handles[4] = { create.handle, 0, 0, 0 };
  uint32_t pitches[4] = { create.pitch, 0, 0, 0 };
  uint32_t offsets[4] = { 0, 0, 0, 0 };

  if (drmModeAddFB2(
    display->fd_card,
    create.width,
    create.height,
    DRM_FORMAT_XRGB8888,
    handles,
    pitches,
    offsets,
    &out->fb_id,
    0
  )) {
    perror("drmModeAddFB");
    return 1;
  }

  // if (drmModeAddFB(
  //   display->fd_card,
  //   create.width,
  //   create.height,
  //   24,
  //   32,
  //   create.pitch,
  //   create.handle,
  //   &out->fb_id
  // )) {
  //   perror("drmModeAddFB");
  //   return 1;
  // }

  struct drm_mode_map_dumb map = { 0 };
  map.handle = create.handle;

  if (drmIoctl(display->fd_card, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
    perror("drmIoctl");
    return 1;
  }

  out->map = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, display->fd_card, map.offset);

  out->handle = create.handle;
  out->pitch = create.pitch;
  out->size = create.size;

  return 0;
}

void owmDumbFrameBuffer_destroy(owmDumbFrameBuffer* fb) {
  owmDisplay* display = OWM_RENDER_DISPLAY.display;
  struct drm_mode_destroy_dumb destroy = { 0 };
  destroy.handle = fb->handle;
  drmIoctl(display->fd_card, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

void owmFrameBuffer_destroy(owmFrameBuffer *fb) {
  owmDumbFrameBuffer_destroy(&fb->buffer);
}

void owmFrameBuffer_destroyList(owmFrameBuffer *fb, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    owmFrameBuffer_destroy(&fb[i]);
  }
}

int owmFrameBuffer_create(owmFrameBuffer *out) {
  if (owmDumbFrameBuffer_create(&out->buffer)) {
    return 1;
  }
  out->state = FB_FREE;
  return 0;
}

int owmFrameBuffer_createList(owmFrameBuffer *out, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (owmFrameBuffer_create(&out[i])) {
      for (size_t j = 0; j < i; ++j) {
        owmFrameBuffer_destroy(&out[j]);
      }
      return 1;
    }
  }
  return 0;
}

int owmRenderContext_init() {
  printf("Creating render context for the chosen display\n");

  OWM_RENDER_CONTEXT.lastTimestamp = 0;
  OWM_RENDER_CONTEXT.displayedBufferIdx = 0;
  OWM_RENDER_CONTEXT.queuedBuffer = -1;

  if (owmFrameBuffer_createList(OWM_RENDER_CONTEXT.frameBuffers, FB_COUNT)) {
    fprintf(stderr, "Failed to create render context for the chosen display: Failed to create frame buffers.\n");
    return 1;
  }

  OWM_RENDER_CONTEXT.frameBuffers[OWM_RENDER_CONTEXT.displayedBufferIdx].state = FB_DISPLAYED;

  // Initial atomic request to setup rendering
  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  owmPrimaryPlaneProperties* plane_props = &OWM_RENDER_DISPLAY.display->plane_primary_properties;
  owmDisplay *render_display = OWM_RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &render_display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx];

  // TODO: Proper multi-output routing
  // NVIDIA + USB-C mirrors connectors onto a single CRTC.
  // This needs driver-specific handling.
  drmModeAtomicAddProperty(atomicReq, render_display->connector_id, plane_props->connector_crtc_id, render_display->crtc_id);

  // CRTC
  drmModeAtomicAddProperty(atomicReq, render_display->crtc_id, plane_props->crtc_activate, 1);
  drmModeAtomicAddProperty(atomicReq, render_display->crtc_id, plane_props->crtc_mode_id, OWM_RENDER_DISPLAY.property_blob_id);

  // Plane
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_fb_id, OWM_RENDER_CONTEXT.frameBuffers[OWM_RENDER_CONTEXT.displayedBufferIdx].buffer.fb_id);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_crtc_id, render_display->crtc_id);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_crtc_x, 0);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_crtc_y, 0);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_crtc_w, mode->hdisplay);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_crtc_h, mode->vdisplay);

  // SRC are 16.16 fixed-point
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_src_x, 0);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_src_y, 0);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_src_w, mode->hdisplay << 16);
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_src_h, mode->vdisplay << 16);

  if (drmModeAtomicCommit(render_display->fd_card, atomicReq, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL) != 0) {
    perror("drmModeAtomicCommit INIT");
  }
  drmModeAtomicFree(atomicReq);

  return 0;
}

void owmRenderContext_close() {
  owmFrameBuffer_destroyList(OWM_RENDER_CONTEXT.frameBuffers, FB_COUNT);
}

owmFrameBuffer* owmRenderContext_get_free_buffer() {
  for (int buf_idx = 0; buf_idx < FB_COUNT; ++buf_idx) {
    if (OWM_RENDER_CONTEXT.frameBuffers[buf_idx].state == FB_FREE) {
      OWM_RENDER_CONTEXT.renderFrameBufferIdx = buf_idx;
      return &OWM_RENDER_CONTEXT.frameBuffers[buf_idx];
    }
  }
  return NULL;
}

int owmRenderContext_swap_frame_buffer() {
  static owmFlipEvent flipEvent;
  flipEvent.bufferIndex = OWM_RENDER_CONTEXT.renderFrameBufferIdx;

  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  owmPrimaryPlaneProperties* plane_props = &OWM_RENDER_DISPLAY.display->plane_primary_properties;
  owmDisplay *display = OWM_RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx];

  // Plane
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_fb_id, OWM_RENDER_CONTEXT.frameBuffers[OWM_RENDER_CONTEXT.renderFrameBufferIdx].buffer.fb_id);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_crtc_id, display->crtc_id);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_crtc_x, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_crtc_y, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_crtc_w, mode->hdisplay);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_crtc_h, mode->vdisplay);

  // SRC are 16.16 fixed-point
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_src_x, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_src_y, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_src_w, mode->hdisplay << 16);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_src_h, mode->vdisplay << 16);

  int ret = drmModeAtomicCommit(
    display->fd_card,
    atomicReq,
    DRM_MODE_ATOMIC_TEST_ONLY,
    NULL
  );
  if (ret != 0) {
    perror("drmModeAtomicCommit TEST");
    drmModeAtomicFree(atomicReq);
    return ret;
  }

  int commitResult = drmModeAtomicCommit(display->fd_card, atomicReq, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, &flipEvent);
  if (commitResult != 0) {
    perror("drmModeAtomicCommit: failed to commit frame buffer swap request");
    return 1;
  }

  OWM_RENDER_CONTEXT.frameBuffers[OWM_RENDER_CONTEXT.renderFrameBufferIdx].state = FB_QUEUED;
  OWM_RENDER_CONTEXT.queuedBuffer = OWM_RENDER_CONTEXT.renderFrameBufferIdx;

  drmModeAtomicFree(atomicReq);
  return 0;
}

inline bool owmRenderContext_can_swap_frame() {
  return OWM_RENDER_CONTEXT.queuedBuffer == -1;
}

void owmRenderContext_page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
  owmFlipEvent *ev = data;
  int newDisplayedBufferIdx = ev->bufferIndex;

  printf("Displayed buffer %d, frame time %lu us\n", newDisplayedBufferIdx, OWM_RENDER_CONTEXT.frameTime);

  uint64_t now = (uint64_t) sec * 1000000 + usec;
  if (OWM_RENDER_CONTEXT.lastTimestamp != 0) {
    OWM_RENDER_CONTEXT.frameTime = now - OWM_RENDER_CONTEXT.lastTimestamp;
  }

  OWM_RENDER_CONTEXT.frameBuffers[OWM_RENDER_CONTEXT.displayedBufferIdx].state = FB_FREE; // Old displayed buffer becomes DB_FREE
  OWM_RENDER_CONTEXT.frameBuffers[newDisplayedBufferIdx].state = FB_DISPLAYED; // New disaplyed becomes FB_DISPLAYED

  OWM_RENDER_CONTEXT.displayedBufferIdx = newDisplayedBufferIdx;
  OWM_RENDER_CONTEXT.queuedBuffer = -1;
  OWM_RENDER_CONTEXT.lastTimestamp = now;
}
