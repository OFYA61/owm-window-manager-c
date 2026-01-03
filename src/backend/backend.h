#pragma once

#include <stdint.h>

typedef enum {
  OWM_BACKEND_TYPE_LINUX
} OWM_BackendType;

typedef struct {
  uint32_t *pixels;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
} OWM_FrameBuffer;

typedef struct {
  OWM_BackendType type;

  // Gets an available frame buffer to render on, if not returns `NULL`
  OWM_FrameBuffer* (*aquireFreeFrameBuffer)();
  // Swaps frame buffers, returns non 0 value when error happens
  int (*swapBuffers)();
  uint32_t (*getDisplayWidth)();
  uint32_t (*getDisplayHeight)();

  // Must be called every frame to check for events
  void (*dispatch)();

} OWM_Backend;

int OWM_initBackend(OWM_BackendType type, OWM_Backend *out_backend);
void OWM_shutdownBackend(OWM_Backend *backend);
