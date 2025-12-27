#pragma once

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
} owmRenderContext;

typedef struct {
  int bufferIndex;
} owmFlipEvent;

extern owmRenderContext OWM_RENDER_CONTEXT;

/// Initializes the global `OWM_RENDER_CONTEXT` object
int owmRenderContext_init();
/// Cleans up objects created for the global `RENDER_CONTEXT` object
void owmRenderContext_close();

/// Returns the index of the first free buffer that it finds to be drawn upon
int owmRenderContext_find_free_buffer();

/// Page flip handler
void owm_page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
