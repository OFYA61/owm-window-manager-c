#pragma once

typedef struct {
  // Initializes the backend
  int (*init)();
  // Cleans up allocated memory and opened file descriptors by the backend
  void (*shutdown)();

  // Gets an available frame buffer to render on, if not returns `NULL`
  void* (*aquireFrameBuffer)();
  // Swaps frame buffers
  void* (*swap)();
} OWM_Backend;
