#pragma once

#include "../backend.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// Initializes the render context
int OWM_drmInitRenderContext();
/// Cleans up objects related to the render context
void OWM_drmShutdownRenderContext();

/// Submit swap request
int OWM_drmFlipRenderContext();
/// Checks if there is a free buffer to be drawn on
bool OWM_drmIsNextBufferFree();
/// Returns a `owmFrameBuffer` that is ready to be drawn upon.
/// If no frame buffer is free, returns `NULL`.
OWM_FrameBuffer* OWM_drmAquireFreeFrameBuffer();

uint32_t OWM_drmGetDisplayWidth();
uint32_t OWM_drmGetDisplayHeight();

int OWM_drmGetCardFileDescritor();

/// Page flip handler.
void OWM_drmFlipRenderContextHandler(int fd, unsigned int frame, unsigned int sec, unsigned int usec, void *data);
