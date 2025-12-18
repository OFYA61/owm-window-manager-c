#include "display.h"
#include <drm.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <unistd.h>
#include <xf86drm.h>
#include <sys/mman.h>
#include <xf86drmMode.h>
#include <drm_mode.h>

int main() {
  struct Display display = Display_pick();

  printf("Chosen display stats: %dx%d @ %dHz\n", display.displayMode.hdisplay, display.displayMode.vdisplay, display.displayMode.vrefresh);

  struct drm_mode_create_dumb create = { 0 };
  create.width = display.displayMode.hdisplay;
  create.height = display.displayMode.vdisplay;
  create.bpp = 32;

  if (drmIoctl(display.fd_card, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
    perror("drmIoctl");
    Display_close(&display);
    return 1;
  }

  uint32_t fb_id;
  if (drmModeAddFB(
    display.fd_card,
    display.displayMode.hdisplay,
    display.displayMode.vdisplay,
    24,
    32,
    create.pitch,
    create.handle,
    &fb_id)
  ) {
    perror("drmModeAddFB");
    Display_close(&display);
    return 1;
  }

  struct drm_mode_map_dumb map = { 0 };
  map.handle = create.handle;

  if (drmIoctl(display.fd_card, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
    perror("drmIoctl");
    Display_close(&display);
    return 1;
  }

  void *fb_ptr = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, display.fd_card, map.offset);
  uint32_t *pixel = fb_ptr;
  for (uint32_t y = 0; y < display.displayMode.vdisplay; ++y) {
    for (uint32_t x = 0; x < display.displayMode.hdisplay; ++x) {
      pixel[x] = 0x000000FF;
    }
    pixel += create.pitch / 4;
  }

  drmModeCrtc *old_crtc = drmModeGetCrtc(display.fd_card, display.crtc_id);
  // drmModeSetCrtc(display.fd_card, display.crtc_id, 0, 0, 0, NULL, 0, NULL);
  if (drmModeSetCrtc(display.fd_card, display.crtc_id, fb_id, 0, 0, &display.connector_id, 1, &display.displayMode)) {
    perror("drmModeSetCrtc");
    Display_close(&display);
    return 1;
  }

  sleep(5);

  drmModeSetCrtc(
    display.fd_card,
    old_crtc->crtc_id,
    old_crtc->buffer_id,
    old_crtc->x,
    old_crtc->y,
    &display.connector_id,
    1,
    &old_crtc->mode
  );
  munmap(fb_ptr, create.size);
  drmModeRmFB(display.fd_card, fb_id);

  struct drm_mode_destroy_dumb destory = { 0 };
  destory.handle = create.handle;
  drmIoctl(display.fd_card, DRM_IOCTL_MODE_DESTROY_DUMB, &destory);
  drmModeFreeCrtc(old_crtc);

  Display_close(&display);

  return 0;
}
