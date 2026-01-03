#pragma once

#include <stdint.h>

typedef enum {
  OWM_BACKEND_TYPE_LINUX
} OWM_Context_type;

typedef struct {
  uint32_t *pixels;
  uint32_t width;
  uint32_t height;
  uint32_t stride;
} OWM_FrameBuffer;

typedef struct {
  // Gets an available frame buffer to render on, if not returns `NULL`
  OWM_FrameBuffer* (*aquireFreeFrameBuffer)();
  // Swaps frame buffers, returns non 0 value when error happens
  int (*swapBuffers)();

  uint32_t (*getDisplayWidth)();
  uint32_t (*getDisplayHeight)();

  OWM_Context_type type;
} OWM_Context;

int OWM_initBackend(OWM_Context_type context_type, OWM_Context *out_backend);
void OWM_shutdownBackend(OWM_Context *backend);
