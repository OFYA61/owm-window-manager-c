#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define FB_COUNT 3

typedef struct {
  uint32_t fb_id;
  uint32_t handle;
  /// Number of bytes in a row
  uint32_t pitch; 
  size_t size;
  void *map;
} owmDumbFrameBuffer;

enum owmFBState {
  FB_FREE,
  FB_QUEUED,
  FB_DISPLAYED
};

typedef struct {
  owmDumbFrameBuffer buffer;
  enum owmFBState state;
} owmFrameBuffer ;

typedef struct {
  uint64_t lastTimestamp;
  uint64_t frameTime;
  owmFrameBuffer frameBuffers[FB_COUNT];
  size_t displayedBufferIdx;
  int queuedBuffer;
  int renderFrameBufferIdx;
} owmRenderContext;

typedef struct {
  int bufferIndex;
} owmFlipEvent;

/// Initializes the render context
int owmRenderContext_init();
/// Cleans up objects related to the render context
void owmRenderContext_close();

/// Returns a `owmFrameBuffer` that is ready to be drawn upon.
/// If no frame buffer is free, returns `NULL`.
owmFrameBuffer* owmRenderContext_get_free_buffer();
/// Submit swap request
int owmRenderContext_swap_frame_buffer();
/// Checks if a swap request can be submitted
bool owmRenderContext_can_swap_frame();

uint32_t owmRenderDisplay_get_width();
uint32_t owmRenderDisplay_get_height();

int owmRenderDisplay_get_fd_card();

/// Page flip handler.
void owmRenderContext_page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
