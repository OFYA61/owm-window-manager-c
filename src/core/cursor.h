#pragma once

#include "backend/backend.h"

void OWM_setCursorPosition(int new_x, int new_y);
void OWM_updateCursorPosition(int delta_x, int delta_y);
void OWM_updateCursorConfines(uint32_t x_min, uint32_t x_max, uint32_t y_min, uint32_t y_max);
void OWM_renderCursor(OWM_FrameBuffer* frame_buffer);

int32_t OWM_getCursorX();
int32_t OWM_getCursorY();
