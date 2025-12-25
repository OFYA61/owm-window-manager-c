#include <drm.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <sys/mman.h>
#include <xf86drmMode.h>
#include <drm_mode.h>

#include "display.h"
#include "render.h"

void PlaneProperties_init() {
  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  PrimaryPlaneProperties* plane_props = &RENDER_DISPLAY.display->plane_primary_properties;
  OfyaDisplay *render_display = RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &render_display->display_modes[RENDER_DISPLAY.selected_mode_idx];

  drmModeAtomicAddProperty(atomicReq, render_display->connector_id, plane_props->connector_crtc_id, render_display->crtc_id);

  // CRTC
  drmModeAtomicAddProperty(atomicReq, render_display->crtc_id, plane_props->crtc_activate, 1);
  drmModeAtomicAddProperty(atomicReq, render_display->crtc_id, plane_props->crtc_mode_id, RENDER_DISPLAY.property_blob_id);

  // Plane
  drmModeAtomicAddProperty(atomicReq, render_display->plane_primary, plane_props->plane_fb_id, RENDER_CONTEXT.frameBuffers[RENDER_CONTEXT.displayedBufferIdx].buffer.fb_id);
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
}

int commitAtomicRenderRequest(uint32_t fb_id, OfyaFlipEvent *flipEvent) {
  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  PrimaryPlaneProperties* plane_props = &RENDER_DISPLAY.display->plane_primary_properties;
  OfyaDisplay *display = RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &display->display_modes[RENDER_DISPLAY.selected_mode_idx];

  // Plane
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, plane_props->plane_fb_id, fb_id);
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

  int commitResult = drmModeAtomicCommit(display->fd_card, atomicReq, DRM_MODE_ATOMIC_NONBLOCK | DRM_MODE_PAGE_FLIP_EVENT, flipEvent);
  if (commitResult != 0) {
    perror("drmModeAtomicCommit");
  }

  drmModeAtomicFree(atomicReq);
  return commitResult;
}


int main() {
  if (OfyaDisplays_scan()) {
    perror("OfyaDisplay_scan");
    return 1;
  }

  if (OfyaRenderDisplay_pick()) {
    OfyaDisplays_close();
    return 1;
  }

  if (OfyaRenderContext_init()) {
    OfyaDisplays_close();
    return 1;
  }

  OfyaDisplay *display = RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &display->display_modes[RENDER_DISPLAY.selected_mode_idx];

  uint32_t frame_count = 0;

  drmEventContext ev = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler
  };

  struct pollfd pfd = {
    .fd = display->fd_card,
    .events = POLLIN
  };

  while (1) {
    int renderFrameBufferIdx = OfyaRenderContext_find_free_buffer();
    if (renderFrameBufferIdx < 0) {
      // This should not happen, but don't crash yet
      fprintf(stderr, "Could not find free buffer to render");
      continue;
    }
    RENDER_CONTEXT.renderFrameBufferIdx = renderFrameBufferIdx;

    uint32_t color = frame_count & 1 ? 0x00FF0000 : 0x000000FF;
    uint32_t *pixel = RENDER_CONTEXT.frameBuffers[RENDER_CONTEXT.renderFrameBufferIdx].buffer.map;
    for (uint32_t y = 0; y < mode->vdisplay; ++y) {
      for (uint32_t x = 0; x < mode->hdisplay; ++x) {
        pixel[x] = color;
      }
      pixel += RENDER_CONTEXT.frameBuffers[RENDER_CONTEXT.renderFrameBufferIdx].buffer.pitch / 4; // Divide by 4, since pixel jumps by 4 bytes
    }

    if (RENDER_CONTEXT.queuedBuffer == -1) {
      static OfyaFlipEvent flipEvent;
      flipEvent.bufferIndex = RENDER_CONTEXT.renderFrameBufferIdx;

      if (commitAtomicRenderRequest(RENDER_CONTEXT.frameBuffers[RENDER_CONTEXT.renderFrameBufferIdx].buffer.fb_id, &flipEvent) != 0) {
        perror("drmModeAtomicCommit");
      }

      RENDER_CONTEXT.frameBuffers[RENDER_CONTEXT.renderFrameBufferIdx].state = FB_QUEUED;
      RENDER_CONTEXT.queuedBuffer = RENDER_CONTEXT.renderFrameBufferIdx;
    }

    // Poll for events
    int ret = poll(&pfd, 1, -1); // -1 waits forever
    if (ret > 0 && pfd.revents & POLLIN) {
      drmHandleEvent(display->fd_card, &ev);
    }
    usleep(1000);

    frame_count++;
  }

  OfyaRenderContext_close();
  OfyaDisplays_close();

  return 0;
}
