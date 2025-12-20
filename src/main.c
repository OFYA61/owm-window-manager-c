#include <drm.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <sys/mman.h>
#include <xf86drmMode.h>
#include <drm_mode.h>
#include <poll.h>

#include "display.h"
#include "dumbfb.h"

void page_flip_handler(
  int fd_card,
  unsigned int frame,
  unsigned int sec,
  unsigned int usec,
  void *data
) {
  int *waiting  = data;
  *waiting = 0;
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
  struct DumbFB fb[2];
  size_t front = 0;
  size_t back = 1;
  if (DumbFB_create(
    display.fd_card,
    display.displayMode.hdisplay,
    display.displayMode.vdisplay,
    &fb[0]
  )) {
    fprintf(stderr, "Failed to create dumb frame buffer 0\n");
    Display_close(&display);
    return 1;
  }
  if (DumbFB_create(
    display.fd_card,
    display.displayMode.hdisplay,
    display.displayMode.vdisplay,
    &fb[1]
  )) {
    fprintf(stderr, "Failed to create dumb frame buffer 0\n");
    Display_close(&display);
    return 1;
  }

  if (drmModeSetCrtc(
    display.fd_card,
    display.crtc_id,
    fb[front].fb_id,
    0,
    0,
    &display.connector_id,
    1,
    &display.displayMode
  )) {
    perror("drmModeSetCrtc");
    DumbFB_destroy(display.fd_card, &fb[0]);
    DumbFB_destroy(display.fd_card, &fb[1]);
    Display_close(&display);
    return 1;
  }

  drmEventContext ev = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler
  };

  while (1) {
    uint32_t color = frame_count & 1 ? 0x00FF0000 : 0x000000FF;
    uint32_t *pixel = fb[back].map;
    for (uint32_t y = 0; y < display.displayMode.vdisplay; ++y) {
      for (uint32_t x = 0; x < display.displayMode.hdisplay; ++x) {
        pixel[x] = color;
      }
      pixel += fb[back].pitch / 4;
    }

    int waiting_for_flip = 1;
    drmModePageFlip(
      display.fd_card,
      display.crtc_id,
      fb[back].fb_id,
      DRM_MODE_PAGE_FLIP_EVENT,
      &waiting_for_flip
    );

    while (waiting_for_flip) {
      fd_set fds;
      FD_ZERO(&fds);
      FD_SET(display.fd_card, &fds);
      select(display.fd_card + 1, &fds, NULL, NULL, NULL);
      drmHandleEvent(display.fd_card, &ev);
    }

    int tmp = front;
    front = back;
    back = tmp;
    frame_count++;
  }

  DumbFB_destroy(display.fd_card, &fb[0]);
  DumbFB_destroy(display.fd_card, &fb[1]);
  Display_close(&display);

  return 0;
}
