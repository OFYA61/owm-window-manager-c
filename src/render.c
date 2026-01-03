#include "render.h"

#include "display.h"
#include "drm_fourcc.h"
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

typedef struct {
  OWM_DRMDisplay* display;
  uint32_t property_blob_id;
  size_t selected_mode_idx;
} owmRenderDisplay;

owmRenderDisplay OWM_RENDER_DISPLAY = { NULL, 0, 0 };
OWM_DRMRenderContext OWM_RENDER_CONTEXT = { 0 };

int OWM_createDRMDumbFrameBuffer(OWM_DRMDumbFrameBuffer *out) {
  OWM_DRMDisplay* display = OWM_RENDER_DISPLAY.display;
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

void OWM_destroyDRMDumbFrameBuffer(OWM_DRMDumbFrameBuffer* fb) {
  OWM_DRMDisplay* display = OWM_RENDER_DISPLAY.display;
  struct drm_mode_destroy_dumb destroy = { 0 };
  destroy.handle = fb->handle;
  drmIoctl(display->fd_card, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}

void OWM_destroyFrameBuffer(OWM_DRMFrameBuffer *fb) {
  OWM_destroyDRMDumbFrameBuffer(&fb->buffer);
}

void OWM_destroyFrameBufferList(OWM_DRMFrameBuffer *fb, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    OWM_destroyFrameBuffer(&fb[i]);
  }
}

int OWM_createFrameBuffer(OWM_DRMFrameBuffer *out) {
  if (OWM_createDRMDumbFrameBuffer(&out->buffer)) {
    return 1;
  }
  out->state = OWM_FB_FREE;
  return 0;
}

int OWM_creatFrameBufferList(OWM_DRMFrameBuffer *out, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (OWM_createFrameBuffer(&out[i])) {
      for (size_t j = 0; j < i; ++j) {
        OWM_destroyFrameBuffer(&out[j]);
      }
      return 1;
    }
  }
  return 0;
}

/// Queries the user to select a display from the discovered displays array
int OWM_pickDrmDisplay() {
  size_t n_display;
  size_t n_mode;

  const OWM_DRMDisplays *displays = OWM_getDRMDisplays();
  printf("Pick a display from 0-%ld\n", displays->count - 1);
  for (size_t display_idx = 0; display_idx < displays->count; ++display_idx) {
    OWM_DRMDisplay display = displays->displays[display_idx];
    printf(
      "%zd: Conn %d | Enc %d | Crtc %d | Plane Primary %d | Plane Cursor %d | Plane Overlay %d\n",
      display_idx,
      display.connector_id,
      display.encoder_id,
      display.crtc_id,
      display.plane_primary,
      display.plane_cursor,
      display.plane_overlay
    );
  }
  fflush(stdin);
  scanf("%zd", &n_display);
  if (n_display > displays->count) {
    fprintf(stderr, "The chose display ID %zd doesn't exist\n", n_display);
    return 1;
  }
  OWM_DRMDisplay* display = &displays->displays[n_display];

  printf("Pick a mode from 0-%ld\n", display->count_display_modes);
  for (size_t mode_idx = 0; mode_idx < display->count_display_modes; ++mode_idx) {
    drmModeModeInfo mode = display->display_modes[mode_idx];
    printf("\t%zd: %dx%d %dHz\n", mode_idx, mode.hdisplay, mode.vdisplay, mode.vrefresh);
  }
  fflush(stdin);
  scanf("%zd", &n_mode);
  if (n_mode > display->count_display_modes) {
    fprintf(stderr, "The chose display ID %zd doesn't exist\n", n_mode);
    return 1;
  }

  drmModeModeInfo mode = display->display_modes[n_mode];
  uint32_t property_blob_id;
  if (drmModeCreatePropertyBlob(
    display->fd_card,
    &mode,
    sizeof(mode),
    &property_blob_id
  ) != 0 ) {
    perror("drmModeCreatePropertyBlob");
    return 1;
  }

  printf("Selected display stats: %dx%d %dHz\n", mode.hdisplay, mode.vdisplay, mode.vrefresh);

  OWM_RENDER_DISPLAY.display = display;
  OWM_RENDER_DISPLAY.property_blob_id = property_blob_id;
  OWM_RENDER_DISPLAY.selected_mode_idx = n_mode;

  return 0;
}

int OWM_drmInitRenderContext() {
  if (OWM_pickDrmDisplay()) {
    return 1;
  }

  printf("Creating render context for the chosen display\n");

  OWM_RENDER_CONTEXT.last_timestamp = 0;
  OWM_RENDER_CONTEXT.displayed_buffer_idx = 0;
  OWM_RENDER_CONTEXT.queued_buffer_idx = 1;
  OWM_RENDER_CONTEXT.next_buffer_idx = 2;

  if (OWM_creatFrameBufferList(OWM_RENDER_CONTEXT.frame_buffers, FB_COUNT)) {
    fprintf(stderr, "Failed to create render context for the chosen display: Failed to create frame buffers.\n");
    return 1;
  }

  OWM_RENDER_CONTEXT.frame_buffers[OWM_RENDER_CONTEXT.displayed_buffer_idx].state = OWM_FB_DISPLAYED;

  // Initial atomic request to setup rendering
  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  OWM_DRMPrimaryPlaneProperties* plane_props = &OWM_RENDER_DISPLAY.display->plane_primary_properties;
  OWM_DRMDisplay *render_display = OWM_RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &render_display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx];

  // TODO: Proper multi-output routing
  // NVIDIA + USB-C mirrors connectors onto a single CRTC.
  // This needs driver-specific handling.
  drmModeAtomicAddProperty(atomicReq, render_display->connector_id, plane_props->connector_crtc_id, render_display->crtc_id);

  // CRTC
  drmModeAtomicAddProperty(atomicReq, render_display->crtc_id, plane_props->crtc_activate, 1);
  drmModeAtomicAddProperty(atomicReq, render_display->crtc_id, plane_props->crtc_mode_id, OWM_RENDER_DISPLAY.property_blob_id);

  // Plane
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_fb_id, OWM_RENDER_CONTEXT.frame_buffers[OWM_RENDER_CONTEXT.displayed_buffer_idx].buffer.fb_id);
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

  printf("Created render context for the chosen display\n");
  return 0;
}

void OWM_drmCloseRenderContext() {
  OWM_destroyFrameBufferList(OWM_RENDER_CONTEXT.frame_buffers, FB_COUNT);
}

OWM_DRMFrameBuffer* OWM_drmGetFreeBuffer() {
  if (OWM_RENDER_CONTEXT.frame_buffers[OWM_RENDER_CONTEXT.next_buffer_idx].state != OWM_FB_FREE) {
    return NULL;
  }

  OWM_DRMFrameBuffer *frame_buffer = &OWM_RENDER_CONTEXT.frame_buffers[OWM_RENDER_CONTEXT.next_buffer_idx];
  frame_buffer->state = OWM_FB_QUEUED;
  OWM_RENDER_CONTEXT.next_buffer_idx++;
  if (OWM_RENDER_CONTEXT.next_buffer_idx >= FB_COUNT) {
    OWM_RENDER_CONTEXT.next_buffer_idx = 0;
  }
  return frame_buffer;
}

int OWM_drmFlipRenderContext() {
  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  OWM_DRMPrimaryPlaneProperties* plane_props = &OWM_RENDER_DISPLAY.display->plane_primary_properties;
  OWM_DRMDisplay *display = OWM_RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx];

  // Plane
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_fb_id, OWM_RENDER_CONTEXT.frame_buffers[OWM_RENDER_CONTEXT.queued_buffer_idx].buffer.fb_id);
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

  static OWM_DRMFlipEvent flipEvent;
  flipEvent.buffer_to_swap = OWM_RENDER_CONTEXT.queued_buffer_idx;
  int commitResult = drmModeAtomicCommit(display->fd_card, atomicReq, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, &flipEvent);
  if (commitResult != 0) {
    perror("drmModeAtomicCommit: failed to commit frame buffer swap request");
    return 1;
  }

  OWM_RENDER_CONTEXT.queued_buffer_idx++;
  if (OWM_RENDER_CONTEXT.queued_buffer_idx >= FB_COUNT) {
    OWM_RENDER_CONTEXT.queued_buffer_idx = 0;
  }

  drmModeAtomicFree(atomicReq);
  return 0;
}

inline bool OWM_drmIsNextBufferFree() {
  return OWM_RENDER_CONTEXT.frame_buffers[OWM_RENDER_CONTEXT.next_buffer_idx].state == OWM_FB_FREE;
}

inline uint32_t OWM_drmGetBufferWidth() {
  return OWM_RENDER_DISPLAY.display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx].hdisplay;
}

inline uint32_t OWM_drmGetBufferHeight() {
  return OWM_RENDER_DISPLAY.display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx].vdisplay;
}

inline int OWM_drmGetCardFileDescritor() {
  return OWM_RENDER_DISPLAY.display->fd_card;
}

void OWM_drmFlipRenderContextHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data) {
  OWM_DRMFlipEvent *ev = data;
  int newDisplayedBufferIdx = ev->buffer_to_swap;

  // printf("Displayed buffer %d, frame time %lu us\n", newDisplayedBufferIdx, OWM_RENDER_CONTEXT.frame_time);

  uint64_t now = (uint64_t) sec * 1000000 + usec;
  if (OWM_RENDER_CONTEXT.last_timestamp != 0) {
    OWM_RENDER_CONTEXT.frame_time = now - OWM_RENDER_CONTEXT.last_timestamp;
  }

  OWM_RENDER_CONTEXT.frame_buffers[OWM_RENDER_CONTEXT.displayed_buffer_idx].state = OWM_FB_FREE; // Old displayed buffer becomes DB_FREE
  OWM_RENDER_CONTEXT.frame_buffers[newDisplayedBufferIdx].state = OWM_FB_DISPLAYED; // New disaplyed becomes FB_DISPLAYED

  OWM_RENDER_CONTEXT.displayed_buffer_idx = newDisplayedBufferIdx;
  OWM_RENDER_CONTEXT.last_timestamp = now;
}
