#include "render.h"

#include "display.h"
#include "drm_fourcc.h"
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

owmRenderContext OWM_RENDER_CONTEXT = { 0 };

int OfyaDumbFrameBuffer_create(owmDumbFrameBuffer *out) {
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

void OfyaDumbFrameBuffer_destroy(owmDumbFrameBuffer* fb) {
  owmDisplay* display = OWM_RENDER_DISPLAY.display;
  struct drm_mode_destroy_dumb destroy = { 0 };
  destroy.handle = fb->handle;
  drmIoctl(display->fd_card, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

void OfyaFrameBuffer_destroy(owmFrameBuffer *fb) {
  OfyaDumbFrameBuffer_destroy(&fb->buffer);
}

void OfyaFrameBuffer_destroyList(owmFrameBuffer *fb, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    OfyaFrameBuffer_destroy(&fb[i]);
  }
}

int OfyaFrameBuffer_create(owmFrameBuffer *out) {
  if (OfyaDumbFrameBuffer_create(&out->buffer)) {
    return 1;
  }
  out->state = FB_FREE;
  return 0;
}

int OfyaFrameBuffer_createList(owmFrameBuffer *out, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (OfyaFrameBuffer_create(&out[i])) {
      for (size_t j = 0; j < i; ++j) {
        OfyaFrameBuffer_destroy(&out[j]);
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

  if (OfyaFrameBuffer_createList(OWM_RENDER_CONTEXT.frameBuffers, FB_COUNT)) {
    fprintf(stderr, "Failed to create render context for the chosen display: Failed to create frame buffers.\n");
    return 1;
  }

  OWM_RENDER_CONTEXT.frameBuffers[OWM_RENDER_CONTEXT.displayedBufferIdx].state = FB_DISPLAYED;

  return 0;
}

void owmRenderContext_close() {
  OfyaFrameBuffer_destroyList(OWM_RENDER_CONTEXT.frameBuffers, FB_COUNT);
}

int owmRenderContext_find_free_buffer() {
  for (int buf_idx = 0; buf_idx < FB_COUNT; ++buf_idx) {
    if (OWM_RENDER_CONTEXT.frameBuffers[buf_idx].state == FB_FREE) {
      return buf_idx;
    }
  }
  return -1;
}

void owm_page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
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
