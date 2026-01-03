#pragma once

#include <stdint.h>

typedef struct {
  uint32_t *pixels;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
} OWM_FrameBuffer;

typedef struct {
  // Initializes the backend
  int (*init)();
  // Cleans up allocated memory and opened file descriptors by the backend
  void (*shutdown)();

  // Gets an available frame buffer to render on, if not returns `NULL`
  OWM_FrameBuffer* (*aquireFreeFrameBuffer)();
  // Swaps frame buffers
  void* (*swap)();

  uint32_t (*getDisplayWidth)();
  uint32_t (*getDisplayHeight)();
} OWM_Backend;
