#include "cursor.h"

#include <stdint.h>
#include <sys/param.h>

#define OWM_CURSOR_SIZE 16

int32_t x_pos = 0;
int32_t y_pos = 0;
int32_t x_min = 0;
int32_t x_max = 0;
int32_t y_min = 0;
int32_t y_max = 0;
uint32_t color = 0x00FFFFFF;

void OWM_updateCursorPosition(int delta_x, int delta_y) {
  int32_t new_x_pos = x_pos + delta_x;
  int32_t new_y_pos = y_pos + delta_y;
  if (new_x_pos < x_min) {
    delta_x = x_min - new_x_pos;
    x_pos = x_min;
  } else if (new_x_pos > x_max - 1) {
    delta_x = x_max - 1 - x_pos;
    x_pos = x_max - 1;
  } else {
    x_pos = new_x_pos;
  }

  if (new_y_pos < y_min) {
    delta_y = y_min - new_y_pos;
    y_pos = y_min;
  } else if (new_y_pos > y_max - 1) {
    delta_y = y_max - 1 - y_pos;
    y_pos = y_max - 1;
  } else {
    y_pos = new_y_pos;
  }
}

void OWM_updateCursorConfines(uint32_t new_x_min, uint32_t new_x_max, uint32_t new_y_min, uint32_t new_y_max) {
  x_min = new_x_min;
  x_max = new_x_max;
  y_min = new_y_min;
  y_max = new_y_max;
}

void OWM_renderCursor(OWM_FrameBuffer* frame_buffer) {
  uint32_t *pixel = frame_buffer->pixels;
  pixel += y_pos * frame_buffer->stride;
  uint32_t y_count = 0;
  for (uint32_t y = y_pos; y < MIN((uint32_t) y_pos + OWM_CURSOR_SIZE, frame_buffer->height); ++y) {
    uint32_t x_limit;
    if (y_count >= OWM_CURSOR_SIZE / 2) {
      x_limit = MIN((uint32_t) x_pos + OWM_CURSOR_SIZE / 2, frame_buffer->width); 
    } else {
      x_limit = MIN((uint32_t) x_pos + OWM_CURSOR_SIZE, frame_buffer->width);
    }
    for (uint32_t x = x_pos; x < x_limit; ++x) {
      pixel[x] = color;
    }
    pixel += frame_buffer->stride;
    y_count++;
  }
}

inline int32_t OWM_getCursorX() {
  return  x_pos;
}

inline int32_t OWM_getCursorY() {
  return  y_pos;
}
