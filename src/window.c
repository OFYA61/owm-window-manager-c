#include "window.h"
#include "render.h"

#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define OWM_FOCUSED_WINDOW_BORDER_COLOR 0x0000FFFF
#define OWM_WINDOW_BORDER_COLOR 0x00FF00FF
#define OWM_BORDER_SIZE 4

typedef struct {
  int32_t pos_x;
  int32_t pos_y;
  uint32_t width;
  uint32_t height;
  
  bool focused;
  bool dragging;

  uint32_t color;
} owmWindow;

typedef struct {
  owmWindow* windows;
  size_t count;
  size_t capacity;
} owmWindows;

owmWindows OWM_WINDOWS = { NULL, 0, 0};

void owmWindows_create_window() {
  if (OWM_WINDOWS.windows == NULL) {
    OWM_WINDOWS.windows = malloc(sizeof(owmWindow) * 4);
    OWM_WINDOWS.capacity = 4;
  }

  if (OWM_WINDOWS.count >= OWM_WINDOWS.capacity) {
    size_t newCapacity = OWM_WINDOWS.capacity * 2;
    owmWindow* tmpWindows = realloc(OWM_WINDOWS.windows, sizeof(owmWindow) * newCapacity);
    if (tmpWindows == NULL) {
      fprintf(stderr, "Failed to reallocated windows\n");
    }
    OWM_WINDOWS.windows = tmpWindows;
    OWM_WINDOWS.capacity = newCapacity;
  }

  // Shift all existing windows to the right
  if (OWM_WINDOWS.count > 0) {
    OWM_WINDOWS.windows[0].focused = false; // Unfocus the currently focused window
    for (size_t window_idx = OWM_WINDOWS.count; window_idx > 0; --window_idx) {
      OWM_WINDOWS.windows[window_idx] = OWM_WINDOWS.windows[window_idx - 1];
    }
  }

  uint32_t rand_color = (uint32_t) rand();
  rand_color = rand_color & 0x00FFFFFF;
  owmWindow newWindow = {
    .pos_x = 100,
    .pos_y = 100,
    .width = 400,
    .height = 300,
    .focused = true,
    .dragging = false,
    .color = rand_color
  };

  OWM_WINDOWS.windows[0] = newWindow;
  OWM_WINDOWS.count++;
}

void owmWindows_close_window() {
  if (OWM_WINDOWS.count == 0) {
    return;
  }

  int focused_window_idx = -1;
  for (size_t window_idx = 0; window_idx < OWM_WINDOWS.count; ++window_idx) {
    if (OWM_WINDOWS.windows[window_idx].focused) {
      focused_window_idx = window_idx;
      break;
    }
  }
  
  if (focused_window_idx == -1) {
    return;
  }

  for (size_t window_idx = focused_window_idx; window_idx < OWM_WINDOWS.count - 1; ++window_idx) {
    OWM_WINDOWS.windows[window_idx] = OWM_WINDOWS.windows[window_idx + 1];
  }
  OWM_WINDOWS.count--;
  if (OWM_WINDOWS.count != 0) {
    OWM_WINDOWS.windows[0].focused = true; // Focus the next window in line
  }
}

void owmWindow_render(size_t window_idx, owmFrameBuffer* frameBuffer) {
  owmWindow* window = &OWM_WINDOWS.windows[window_idx];
  uint32_t *pixel = frameBuffer->buffer.map;

  uint32_t y_start;
  if (window->pos_y > 0) {
    y_start = (uint32_t) window->pos_y;
  } else {
    y_start = 0;
  }
  uint32_t y_end = window->pos_y + window->height;
  if (y_end > owmRenderDisplay_get_height()) {
    y_end = owmRenderDisplay_get_height() - 1;
  }

  uint32_t x_start;
  if (window->pos_x > 0) {
    x_start = (uint32_t) window->pos_x;
  } else {
    x_start = 0;
  }
  uint32_t x_end = window->pos_x + window->width;
  if (x_end > owmRenderDisplay_get_width()) {
    x_end = owmRenderDisplay_get_width() - 1;
  }

  uint32_t border_color = window->focused ? OWM_FOCUSED_WINDOW_BORDER_COLOR : OWM_WINDOW_BORDER_COLOR;
  pixel += (frameBuffer->buffer.pitch / 4) * y_start;
  for (uint32_t y = y_start; y <= y_end; ++y) {
    for (uint32_t x = x_start; x <= x_end; ++x) {
      bool x_check = (x - window->pos_x < OWM_BORDER_SIZE) || (x - window->pos_x > window->width - OWM_BORDER_SIZE);
      bool y_check = (y - window->pos_y < OWM_BORDER_SIZE) || (y - window->pos_y > window->height - OWM_BORDER_SIZE);
      bool within_border = x_check || y_check;
      if (within_border) {
        pixel[x] = border_color;
      } else {
        pixel[x] = window->color;
      }
    }
    pixel += frameBuffer->buffer.pitch / 4;
  }
}

void owmWindows_render(owmFrameBuffer* frameBuffer) {
  if (OWM_WINDOWS.count <= 0) {
    return;
  }

  size_t window_idx = OWM_WINDOWS.count;
  do {
    window_idx--;
    owmWindow_render(window_idx, frameBuffer);
  } while(window_idx > 0);
}

void owmWindows_process_mouse_move_event(uint32_t new_mouse_x, uint32_t new_mouse_y, int32_t mouse_delta_x, int32_t mouse_delta_y) {
  if (OWM_WINDOWS.windows[0].dragging) {
    OWM_WINDOWS.windows->pos_x += mouse_delta_x;
    OWM_WINDOWS.windows->pos_y += mouse_delta_y;
  }
}

void owmWindows_process_mouse_key_event(uint32_t mouse_x, uint32_t mouse_y, uint16_t key_code, bool pressed) {
  if (key_code != BTN_LEFT) { // For now only handle left mouse button events
    return;
  }

  if (pressed) {
    int clicked_window_idx = -1;
    for (size_t window_idx = 0; window_idx < OWM_WINDOWS.count; ++window_idx) {
      owmWindow* window = &OWM_WINDOWS.windows[window_idx];
      if (
        window->pos_x <= (int32_t) mouse_x &&
        (int32_t) mouse_x <= window->pos_x + (int32_t) window->width &&
        window->pos_y <= (int32_t) mouse_y &&
        (int32_t) mouse_y <= window->pos_y + (int32_t) window->height
      ) {
        clicked_window_idx = window_idx;
        break;
      }
    }

    OWM_WINDOWS.windows[0].focused = 0;

    if (clicked_window_idx == -1) {
      return;
    }

    // Focus window and bring it to the front
    owmWindow clicked_window = OWM_WINDOWS.windows[clicked_window_idx];
    clicked_window.focused = true;
    clicked_window.dragging = true;
    for (size_t window_idx = (size_t) clicked_window_idx; window_idx > 0; --window_idx) {
      OWM_WINDOWS.windows[window_idx] = OWM_WINDOWS.windows[window_idx - 1];
    }
    OWM_WINDOWS.windows[0] = clicked_window;
  } else {
    OWM_WINDOWS.windows[0].dragging = false;
  }
}

void owmWindows_process_key_event(uint16_t key_code, bool pressed) {
  if (key_code == KEY_W && pressed) {
    owmWindows_create_window();
  } else if (key_code == KEY_Q && pressed) {
    owmWindows_close_window();
  }
}

void owmWindows_cleanup() {
  free(OWM_WINDOWS.windows);
}
