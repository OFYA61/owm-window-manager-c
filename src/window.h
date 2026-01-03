#pragma once

#include <stddef.h>
#include <stdint.h>

#include "events.h"
#include "render.h"

void OWM_renderWindows(OWM_DRMFrameBuffer* frameBuffer);
void OWM_processWindowMouseEvent(uint32_t new_mouse_x, uint32_t new_mouse_y, int32_t mouse_delta_x, int32_t mouse_delta_y);
void OWM_processWindowMouseButtonEvent(uint32_t mouse_x, uint32_t mouse_y, uint16_t key_code, OWM_KeyEventType event_type);
void OWM_processKeyEvent(uint16_t key_code, OWM_KeyEventType event_type);
void OWM_cleanupWindows();
