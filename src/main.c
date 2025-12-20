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

#define FB_COUNT 3

static struct FlipContext flipContext;

struct FlipContext {
  int pending;
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

  printf("Displayed buffer %d, frame time %lu us\n", newDisplayed, ctx->frameTime);

  uint64_t now = (uint64_t) sec * 1000000 + usec;
  if (ctx->lastTimestamp != 0) {
    ctx->frameTime = now - ctx->lastTimestamp;
  }

  ctx->frameBuffers[ctx->displayedBuffer].state = FB_FREE; // Old displayed buffer becomes DB_FREE
  ctx->frameBuffers[newDisplayed].state = FB_DISPLAYED; // New disaplyed becomes FB_DISPLAYED

  ctx->displayedBuffer = newDisplayed;
  ctx->queuedBuffer = -1;
  ctx->lastTimestamp = now;
  ctx->pending = 0;
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

  flipContext.pending = 0;
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

  if (drmModeSetCrtc(
    display.fd_card,
    display.crtc_id,
    flipContext.frameBuffers[flipContext.displayedBuffer].buffer.fb_id,
    0,
    0,
    &display.connector_id,
    1,
    &display.displayMode
  )) {
    perror("drmModeSetCrtc");
    FrameBuffer_destroyList(&display, flipContext.frameBuffers, FB_COUNT);
    Display_close(&display);
    return 1;
  }

  drmEventContext ev = {
    .version = DRM_EVENT_CONTEXT_VERSION,
    .page_flip_handler = page_flip_handler
  };

  struct pollfd pfd = {
    .fd = display.fd_card,
    .events = POLLIN
  };

  while (1) {
    
    flipContext.renderFrameBufferIdx = find_free_buffer(flipContext.frameBuffers);
    if (flipContext.renderFrameBufferIdx < 0) {
      // This should not happen, but don't crash yet
      fprintf(stderr, "Could not find free buffer to render");
      continue;
    }

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
      flipContext.pending = 1;
      drmModePageFlip(
        display.fd_card,
        display.crtc_id,
        flipContext.frameBuffers[flipContext.renderFrameBufferIdx].buffer.fb_id,
        DRM_MODE_PAGE_FLIP_EVENT,
        &flipEvent
      );

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
