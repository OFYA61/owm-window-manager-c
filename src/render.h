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
} OfyaDumbFrameBuffer;

enum FBState {
  FB_FREE,
  FB_QUEUED,
  FB_DISPLAYED
};

typedef struct {
  OfyaDumbFrameBuffer buffer;
  enum FBState state;
} OfyaFrameBuffer ;

typedef struct {
  uint64_t lastTimestamp;
  uint64_t frameTime;
  OfyaFrameBuffer frameBuffers[FB_COUNT];
  size_t displayedBufferIdx;
  int queuedBuffer;
} OfyaRenderContext;

typedef struct {
  int bufferIndex;
} OfyaFlipEvent;

extern OfyaRenderContext RENDER_CONTEXT;

/// Initializes the global `RENDER_CONTEXT` object
int OfyaRenderContext_init();
/// Cleans up objects created for the global `RENDER_CONTEXT` object
void OfyaRenderContext_close();

/// Returns the index of the first free buffer that it finds to be drawn upon
int OfyaRenderContext_find_free_buffer();

/// Page flip handler
void page_flip_handler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
