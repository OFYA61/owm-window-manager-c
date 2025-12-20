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
#include "fb.h"

struct FrameStats {
  int pending;
  uint64_t last_us;
};

void page_flip_handler(
  int fd_card,
  unsigned int frame,
  unsigned int sec, // seconds
  unsigned int usec, // microseconds
  void *data
) {
  struct FrameStats *stats = data;

  uint64_t now = (uint64_t) sec * 1000000 + usec;
  if (stats->last_us != 0) {
    printf("Frame time: %lu us\n", now - stats->last_us);
  }

  stats->last_us = now;
  stats->pending = 0;
}

int main() {
  struct Display display = Display_pick();

  printf(
    "Chosen display stats: %dx%d @ %dHz\n",
    display.displayMode.hdisplay,
    display.displayMode.vdisplay,
    display.displayMode.vrefresh
  );

  uint32_t frame_count = 0;
  struct FrameBuffer frameBuffers[3];
  size_t front = 0;
  size_t back = 1;

  if (FrameBuffer_createList(
    &display,
    frameBuffers,
    3
  )) {
    perror("FrameBuffer_createList");
    Display_close(&display);
    return 1;
  }

  if (drmModeSetCrtc(
    display.fd_card,
    display.crtc_id,
    frameBuffers[front].buffer.fb_id,
    0,
    0,
    &display.connector_id,
    1,
    &display.displayMode
  )) {
    perror("drmModeSetCrtc");
    FrameBuffer_destroyList(&display, frameBuffers, 3);
    Display_close(&display);
    return 1;
  }

  drmEventContext ev = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler
  };

  struct FrameStats frameStats = { 
    .pending = 1,
    .last_us = 0
  };

  struct pollfd pfd = {
    .fd = display.fd_card,
    .events = POLLIN
  };

  while (1) {
    uint32_t color = frame_count & 1 ? 0x00FF0000 : 0x000000FF;
    uint32_t *pixel = frameBuffers[back].buffer.map;
    for (uint32_t y = 0; y < display.displayMode.vdisplay; ++y) {
      for (uint32_t x = 0; x < display.displayMode.hdisplay; ++x) {
        pixel[x] = color;
      }
      pixel += frameBuffers[back].buffer.pitch / 4; // Divide by 4, since pixel jumps by 4 bits
    }

    frameStats.pending = 1;

    drmModePageFlip(
      display.fd_card,
      display.crtc_id,
      frameBuffers[back].buffer.fb_id,
      DRM_MODE_PAGE_FLIP_EVENT,
      &frameStats
    );

    while (frameStats.pending) {
      int ret = poll(&pfd, 1, -1); // -1 waits forever
      if (ret < 0) {
        perror("poll");
        break;
      }
      if (pfd.revents & POLLIN) {
        drmHandleEvent(display.fd_card, &ev);
      }
    }

    int tmp = front;
    front = back;
    back = tmp;
    frame_count++;
  }

  FrameBuffer_destroyList(&display, frameBuffers, 3);
  Display_close(&display);

  return 0;
}
