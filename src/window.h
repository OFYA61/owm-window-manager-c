#pragma once

#include <stddef.h>
#include <stdint.h>

#include "render.h"

void owmWindows_render(owmFrameBuffer* frameBuffer);
void owmWindows_process_mouse_move_event(uint32_t new_mouse_x, uint32_t new_mouse_y, int32_t mouse_delta_x, int32_t mouse_delta_y);
void owmWindows_process_mouse_key_event(uint32_t mouse_x, uint32_t mouse_y, uint16_t key_code, bool pressed);
void owmWindows_process_key_event(uint16_t key_code, bool pressed);
void owmWindows_cleanup();
