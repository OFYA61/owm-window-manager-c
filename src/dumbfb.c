#include "dumbfb.h"

#include <drm_fourcc.h>
#include <drm.h>
#include <drm_mode.h>
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int DumbFB_create(struct Display *display, struct DumbFB *out) {
  struct drm_mode_create_dumb create = { 0 };
  create.width = display->displayMode.hdisplay;
  create.height = display->displayMode.vdisplay;
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

void DumbFB_destroy(struct Display *display, struct DumbFB* fb) {
  struct drm_mode_destroy_dumb destroy = { 0 };
  destroy.handle = fb->handle;
  drmIoctl(display->fd_card, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}
