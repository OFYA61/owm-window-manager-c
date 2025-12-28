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
  FB_DISPLAYED,
  FB_QUEUED,
  FB_FREE
};

typedef struct {
  owmDumbFrameBuffer buffer;
  enum owmFBState state;
} owmFrameBuffer ;

typedef struct {
  uint64_t last_timestamp;
  uint64_t frame_time;
  owmFrameBuffer frame_buffers[FB_COUNT];
  int displayed_buffer_idx;
  int queued_buffer_idx;
  int next_buffer_idx;
} owmRenderContext;

typedef struct {
  int buffer_to_swap;
} owmFlipEvent;

/// Initializes the render context
int owmRenderContext_init();
/// Cleans up objects related to the render context
void owmRenderContext_close();

/// Submit swap request
int owmRenderContext_submit_frame_buffer_swap_request();
/// Checks if there is a free buffer to be drawn on
bool owmRenderContext_is_next_frame_buffer_free();
/// Returns a `owmFrameBuffer` that is ready to be drawn upon.
/// If no frame buffer is free, returns `NULL`.
owmFrameBuffer* owmRenderContext_get_free_buffer();

uint32_t owmRenderDisplay_get_width();
uint32_t owmRenderDisplay_get_height();

int owmRenderDisplay_get_fd_card();

/// Page flip handler.
void owmRenderContext_page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
