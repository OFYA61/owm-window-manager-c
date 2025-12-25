#include <drm.h>
#include <dirent.h>
#include <fcntl.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <sys/mman.h>
#include <xf86drmMode.h>
#include <drm_mode.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>

#include "display.h"
#include "input.h"
#include "render.h"

// int tty_fd;
//
// void vt_release(int sig) {
//     ioctl(tty_fd, KDSETMODE, KD_TEXT);
//     ioctl(tty_fd, VT_RELDISP, 1);
// }
//
// void vt_acquire(int sig) {
//     ioctl(tty_fd, VT_RELDISP, VT_ACKACQ);
//     ioctl(tty_fd, KDSETMODE, KD_GRAPHICS);
// }

void PlaneProperties_init() {
  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  PrimaryPlaneProperties* plane_props = &RENDER_DISPLAY.display->plane_primary_properties;
  OfyaDisplay *render_display = RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &render_display->display_modes[RENDER_DISPLAY.selected_mode_idx];

  // TODO: Proper multi-output routing
  // NVIDIA + USB-C mirrors connectors onto a single CRTC.
  // This needs driver-specific handling.
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

void cleanup() {
}

int main() {
  int kbd_fd = open_keyboard_device();
  // tty_fd = open("/dev/tty0", O_RDWR | O_CLOEXEC);
  // if (tty_fd < 0) {
  //   perror("open /dev/tty0");
  //   close(kbd_fd);
  //   return 1;
  // }
  // struct vt_mode vtmode = {
  //   .mode = VT_PROCESS,
  //   .relsig = SIGUSR1,
  //   .acqsig = SIGUSR2
  // };
  // ioctl(tty_fd, VT_SETMODE, &vtmode);
  // signal(SIGUSR1, vt_release);
  // signal(SIGUSR2, vt_acquire);
  // atexit(cleanup);
  // signal(SIGINT, exit);
  // signal(SIGTERM, exit);

  if (kbd_fd < 0) {
    perror("Failed to find keybaod\n");
    return 1;
  }

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

  struct pollfd pfds[2] = {
    { .fd = display->fd_card, .events = POLLIN },
    { .fd = kbd_fd, .events = POLLIN}
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
    int ret = poll(pfds, 2, -1); // -1 waits forever
    if (ret > 0) {
      if (pfds[0].revents & POLLIN) {
        drmHandleEvent(display->fd_card, &ev);
      }

      if (pfds[1].revents & POLLIN) {
        if (handle_keybaord(kbd_fd)) {
          printf("Got some event from the keyboard handler\n");
          break;
        }
      }
    }
    usleep(1000);

    frame_count++;
  }

  // ioctl(tty_fd, KDSETMODE, KD_TEXT);
  //
  // struct vt_mode vtmode2 = {
  //   .mode = VT_AUTO
  // };
  // ioctl(tty_fd, VT_SETMODE, &vtmode2);
  // close(tty_fd);
  release_keyboard(kbd_fd);
  close(kbd_fd);

  OfyaRenderContext_close();
  OfyaDisplays_close();

  return 0;
}
