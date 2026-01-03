#pragma once

#include <stdint.h>

typedef struct {
  uint32_t *pixels;
  uint32_t widht;
  uint32_t height;
  uint32_t stride;
} OWM_FrameBuffer;

typedef struct {
  // Initializes the backend
  int (*init)();
  // Cleans up allocated memory and opened file descriptors by the backend
  void (*shutdown)();

  // Gets an available frame buffer to render on, if not returns `NULL`
  OWM_FrameBuffer* (*aquireFrameBuffer)();
  // Swaps frame buffers
  void* (*swap)();
} OWM_Backend;
