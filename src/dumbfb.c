#include "dumbfb.h"
#include "drm.h"

#include <drm_mode.h>
#include <stdio.h>
#include <sys/mman.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

int DumbFB_create(int fd_card, uint32_t width, uint32_t height, struct DumbFB *out) {
  struct drm_mode_create_dumb create = { 0 };
  create.width = width;
  create.height = height;
  create.bpp = 32;

  if (drmIoctl(fd_card, DRM_IOCTL_MODE_CREATE_DUMB, &create) < 0) {
    perror("drmIoctl");
    return 1;
  }

  if (drmModeAddFB(
    fd_card,
    width,
    height,
    24,
    32,
    create.pitch,
    create.handle,
    &out->fb_id
  )) {
    perror("drmModeAddFB");
    return 1;
  }

  struct drm_mode_map_dumb map = { 0 };
  map.handle = create.handle;

  if (drmIoctl(fd_card, DRM_IOCTL_MODE_MAP_DUMB, &map) < 0) {
    perror("drmIoctl");
    return 1;
  }

  out->map = mmap(NULL, create.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd_card, map.offset);

  out->handle = create.handle;
  out->pitch = create.pitch;
  out->size = create.size;

  return 0;
}

void DumbFB_destroy(int fd_card, struct DumbFB* fb) {
  struct drm_mode_destroy_dumb destroy = { 0 };
  destroy.handle = fb->handle;
  drmIoctl(fd_card, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
}
