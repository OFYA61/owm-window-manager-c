#pragma once

#include <stddef.h>
#include <stdint.h>

#include "backend/backend.h"
#include "event.h"

void OWM_renderWindows(OWM_FrameBuffer* frameBuffer);
void OWM_processWindowMouseEvent(int32_t mouse_delta_x, int32_t mouse_delta_y);
void OWM_processWindowMouseButtonEvent(uint16_t key_code, OWM_KeyEventType event_type);
void OWM_processWindowKeyEvent(uint16_t key_code, OWM_KeyEventType event_type);
void OWM_cleanupWindows();
