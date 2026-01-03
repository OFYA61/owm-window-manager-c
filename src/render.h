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
} OWM_DRMDumbFrameBuffer;

typedef enum {
  OWM_FB_DISPLAYED,
  OWM_FB_QUEUED,
  OWM_FB_FREE
} OWM_FBState ;

typedef struct {
  OWM_DRMDumbFrameBuffer buffer;
  OWM_FBState state;
} OWM_DRMFrameBuffer ;

typedef struct {
  uint64_t last_timestamp;
  uint64_t frame_time;
  OWM_DRMFrameBuffer frame_buffers[FB_COUNT];
  int displayed_buffer_idx;
  int queued_buffer_idx;
  int next_buffer_idx;
} OWM_DRMRenderContext;

typedef struct {
  int buffer_to_swap;
} OWM_DRMFlipEvent;

/// Initializes the render context
int OWM_drmInitRenderContext();
/// Cleans up objects related to the render context
void OWM_drmCloseRenderContext();

/// Submit swap request
int OWM_drmFlipRenderContext();
/// Checks if there is a free buffer to be drawn on
bool OWM_drmIsNextBufferFree();
/// Returns a `owmFrameBuffer` that is ready to be drawn upon.
/// If no frame buffer is free, returns `NULL`.
OWM_DRMFrameBuffer* OWM_drmGetFreeBuffer();

uint32_t OWM_drmGetBufferWidth();
uint32_t OWM_drmGetBufferHeight();

int OWM_drmGetCardFileDescritor();

/// Page flip handler.
void OWM_drmFlipRenderContextHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
