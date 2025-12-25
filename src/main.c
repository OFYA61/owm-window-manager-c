#include <drm.h>
#include <dirent.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <unistd.h>
#include <xf86drm.h>
#include <sys/mman.h>
#include <xf86drmMode.h>
#include <drm_mode.h>

#include "display.h"
#include "fb.h"

#define FB_COUNT 3

static struct FlipContext flipContext;

struct FlipContext {
  uint64_t lastTimestamp;
  uint64_t frameTime;
  struct FrameBuffer frameBuffers[FB_COUNT];
  size_t renderFrameBufferIdx;
  size_t displayedBuffer;
  int queuedBuffer;
};

struct FlipEvent {
  int bufferIndex;
};

void page_flip_handler(
  int fd_card,
  unsigned int frame,
  unsigned int sec, // seconds
  unsigned int usec, // microseconds
  void *data
) {
  struct FlipEvent *ev = data;
  int newDisplayed = ev->bufferIndex;

  struct FlipContext *ctx = &flipContext;

  // printf("Displayed buffer %d, frame time %lu us\n", newDisplayed, ctx->frameTime);

  uint64_t now = (uint64_t) sec * 1000000 + usec;
  if (ctx->lastTimestamp != 0) {
    ctx->frameTime = now - ctx->lastTimestamp;
  }

  ctx->frameBuffers[ctx->displayedBuffer].state = FB_FREE; // Old displayed buffer becomes DB_FREE
  ctx->frameBuffers[newDisplayed].state = FB_DISPLAYED; // New disaplyed becomes FB_DISPLAYED

  ctx->displayedBuffer = newDisplayed;
  ctx->queuedBuffer = -1;
  ctx->lastTimestamp = now;
}

uint32_t get_prop_id(int fd, uint32_t obj_id, uint32_t obj_type, const char *name) {
  drmModeObjectProperties *props = drmModeObjectGetProperties(fd, obj_id, obj_type);
  for (uint32_t i = 0; i < props->count_props; ++i) {
    drmModePropertyRes *prop = drmModeGetProperty(fd, props->props[i]);
    if (strcmp(prop->name, name) == 0) {
      uint32_t prop_id = prop->prop_id;
      drmModeFreeProperty(prop);
      drmModeFreeObjectProperties(props);
      return prop_id;
    }
    drmModeFreeProperty(prop);
  }
  drmModeFreeObjectProperties(props);
  fprintf(stderr, "Failed to find property obj_id=%d obj_type=%d name=%s\n", obj_id, obj_type, name);
  return 0;
}

struct PlaneProperties {
  uint32_t connector_crtc_id;

  uint32_t crtc_activate;
  uint32_t crtc_mode_id;

  uint32_t plane_fb_id;
  uint32_t plane_crtc_id;
  uint32_t plane_crtc_x;
  uint32_t plane_crtc_y;
  uint32_t plane_crtc_w;
  uint32_t plane_crtc_h;
  uint32_t plane_src_x;
  uint32_t plane_src_y;
  uint32_t plane_src_w;
  uint32_t plane_src_h;
};

static struct PlaneProperties planeProps;

void PlaneProperties_init(struct Display *display) {
  planeProps.connector_crtc_id = get_prop_id(display->fd_card, display->connector_id, DRM_MODE_OBJECT_CONNECTOR, "CRTC_ID");

  planeProps.crtc_activate = get_prop_id(display->fd_card, display->crtc_id, DRM_MODE_OBJECT_CRTC, "ACTIVE");
  planeProps.crtc_mode_id = get_prop_id(display->fd_card, display->crtc_id, DRM_MODE_OBJECT_CRTC, "MODE_ID");

  planeProps.plane_fb_id = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "FB_ID");
  planeProps.plane_crtc_id = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_ID");
  planeProps.plane_crtc_x = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_X");
  planeProps.plane_crtc_y = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_Y");
  planeProps.plane_crtc_w = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_W");
  planeProps.plane_crtc_h = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "CRTC_H");
  planeProps.plane_src_x = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_X");
  planeProps.plane_src_y = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_Y");
  planeProps.plane_src_w = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_W");
  planeProps.plane_src_h = get_prop_id(display->fd_card, display->plane_primary, DRM_MODE_OBJECT_PLANE, "SRC_H");

  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  drmModeAtomicAddProperty(atomicReq, display->connector_id, planeProps.connector_crtc_id, display->crtc_id);

  // CRTC
  drmModeAtomicAddProperty(atomicReq, display->crtc_id, planeProps.crtc_activate, 1);
  drmModeAtomicAddProperty(atomicReq, display->crtc_id, planeProps.crtc_mode_id, display->mode_blob_id);

  // Plane
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_fb_id, flipContext.frameBuffers[flipContext.displayedBuffer].buffer.fb_id);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_id, display->crtc_id);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_x, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_y, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_w, display->displayMode.hdisplay);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_h, display->displayMode.vdisplay);

  // SRC are 16.16 fixed-point
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_x, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_y, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_w, display->displayMode.hdisplay << 16);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_h, display->displayMode.vdisplay << 16);

  if (drmModeAtomicCommit(display->fd_card, atomicReq, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL) != 0) {
    perror("drmModeAtomicCommit INIT");
  }
  drmModeAtomicFree(atomicReq);
}

int commitAtomicRenderRequest(struct Display *display, uint32_t fb_id, struct FlipEvent *flipEvent) {
  drmModeAtomicReq *atomicReq = drmModeAtomicAlloc();

  // Plane
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_fb_id, fb_id);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_id, display->crtc_id);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_x, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_y, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_w, display->displayMode.hdisplay);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_crtc_h, display->displayMode.vdisplay);

  // SRC are 16.16 fixed-point
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_x, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_y, 0);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_w, display->displayMode.hdisplay << 16);
  drmModeAtomicAddProperty(atomicReq, display->plane_primary, planeProps.plane_src_h, display->displayMode.vdisplay << 16);

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

int find_free_buffer(struct FrameBuffer *frameBuffers) {
  for (int i = 0; i < FB_COUNT; ++i) {
    if (frameBuffers[i].state == FB_FREE) {
      return i;
    }
  }
  return -1;
}

int main() {
  struct Display display = Display_pick();

  flipContext.lastTimestamp = 0;
  flipContext.displayedBuffer = 0;
  flipContext.queuedBuffer = -1;

  printf(
    "Chosen display stats: %dx%d @ %dHz\n",
    display.displayMode.hdisplay,
    display.displayMode.vdisplay,
    display.displayMode.vrefresh
  );

  uint32_t frame_count = 0;

  if (FrameBuffer_createList(
    &display,
    flipContext.frameBuffers,
    FB_COUNT
  )) {
    perror("FrameBuffer_createList");
    Display_close(&display);
    return 1;
  }

  flipContext.frameBuffers[flipContext.displayedBuffer].state = FB_DISPLAYED;

  PlaneProperties_init(&display);

  drmEventContext ev = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler
  };

  struct pollfd pfd = {
    .fd = display.fd_card,
    .events = POLLIN
  };

  while (1) {

    int renderFrameBufferIdx = find_free_buffer(flipContext.frameBuffers);
    if (renderFrameBufferIdx < 0) {
      // This should not happen, but don't crash yet
      fprintf(stderr, "Could not find free buffer to render");
      continue;
    }flipContext.renderFrameBufferIdx = renderFrameBufferIdx;

    uint32_t color = frame_count & 1 ? 0x00FF0000 : 0x000000FF;
    uint32_t *pixel = flipContext.frameBuffers[flipContext.renderFrameBufferIdx].buffer.map;
    for (uint32_t y = 0; y < display.displayMode.vdisplay; ++y) {
      for (uint32_t x = 0; x < display.displayMode.hdisplay; ++x) {
        pixel[x] = color;
      }
      pixel += flipContext.frameBuffers[flipContext.renderFrameBufferIdx].buffer.pitch / 4; // Divide by 4, since pixel jumps by 4 bytes
    }

    if (flipContext.queuedBuffer == -1) {
      static struct FlipEvent flipEvent;
      flipEvent.bufferIndex = flipContext.renderFrameBufferIdx;

      if (commitAtomicRenderRequest(&display, flipContext.frameBuffers[flipContext.renderFrameBufferIdx].buffer.fb_id, &flipEvent) != 0) {
        perror("drmModeAtomicCommit");
      }

      flipContext.frameBuffers[flipContext.renderFrameBufferIdx].state = FB_QUEUED;
      flipContext.queuedBuffer = flipContext.renderFrameBufferIdx;
    }

    // Poll for events
    int ret = poll(&pfd, 1, -1); // -1 waits forever
    if (ret > 0 && pfd.revents & POLLIN) {
      drmHandleEvent(display.fd_card, &ev);
    }
    usleep(1000);

    frame_count++;
  }

  FrameBuffer_destroyList(&display, flipContext.frameBuffers, FB_COUNT);
  Display_close(&display);

  return 0;
}
