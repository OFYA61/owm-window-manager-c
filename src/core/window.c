#include "window.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "backend/backend.h"
#include "core/cursor.h"
#include "event.h"
#include "input.h"

#define OWM_FOCUSED_WINDOW_BORDER_COLOR 0x0000FFFF
#define OWM_BORDER_SIZE 2
#define OWM_RESIZE_BORDER_SIZE 10
#define OWM_WINDOW_MIN_WIDTH 120
#define OWM_WINDOW_MIN_HEIGHT 120

typedef enum {
  OWM_WINDOW_MOUSE_ACTION_NONE,
  OWM_WINDOW_MOUSE_ACTION_DRAG,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_BORDER,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_RIGHT_BORDER,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_RIGHT_BORDER,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_RIGHT_BORDER,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_BORDER,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_LEFT_BORDER,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_LEFT_BORDER,
  OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_LEFT_BORDER,
} owmWindowMouseAction ;

typedef enum {
  OWM_WINDOW_BORDER_NONE,
  OWM_WINDOW_BORDER_TOP,
  OWM_WINDOW_BORDER_TOP_RIGHT,
  OWM_WINDOW_BORDER_RIGHT,
  OWM_WINDOW_BORDER_BOTTOM_RIGHT,
  OWM_WINDOW_BORDER_BOTTOM,
  OWM_WINDOW_BORDER_BOTTOM_LEFT,
  OWM_WINDOW_BORDER_LEFT,
  OWM_WINDOW_BORDER_TOP_LEFT
} owmWindowBorder;

typedef struct {
  int32_t pos_x;
  int32_t pos_y;
  uint32_t width;
  uint32_t height;

  bool focused;
  owmWindowMouseAction mouse_action;

  uint32_t color;
} owmWindow;

typedef struct {
  owmWindow* windows;
  size_t count;
  size_t capacity;
} owmWindows;

owmWindows OWM_WINDOWS = { NULL, 0, 0};

void createWindow() {
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
    .mouse_action = OWM_WINDOW_MOUSE_ACTION_NONE,
    .color = rand_color
  };

  OWM_WINDOWS.windows[0] = newWindow;
  OWM_WINDOWS.count++;
}

void closeWindow() {
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

owmWindowBorder getBorderSideWithBorderWidth(owmWindow* window, uint32_t x, uint32_t y, uint32_t border_width) {
  bool is_left = x - window->pos_x < border_width;
  bool is_right = x - window->pos_x > window->width - border_width;
  bool is_top = y - window->pos_y < border_width;
  bool is_bottom = y - window->pos_y > window->height - border_width;
  if (is_top) {
    if (is_left) {
      return OWM_WINDOW_BORDER_TOP_LEFT;
    }
    if (is_right) {
      return OWM_WINDOW_BORDER_TOP_RIGHT;
    }
    return OWM_WINDOW_BORDER_TOP;
  }
  if (is_bottom) {
    if (is_left) {
      return OWM_WINDOW_BORDER_BOTTOM_LEFT;
    }
    if (is_right) {
      return OWM_WINDOW_BORDER_BOTTOM_RIGHT;
    }
    return OWM_WINDOW_BORDER_BOTTOM;
  }
  if (is_left) {
    return OWM_WINDOW_BORDER_LEFT;
  }
  if (is_right) {
    return OWM_WINDOW_BORDER_RIGHT;
  }
  return OWM_WINDOW_BORDER_NONE;
}

owmWindowBorder getVisualBorderSide(owmWindow* window, uint32_t x, uint32_t y){
  return getBorderSideWithBorderWidth(window, x, y, OWM_BORDER_SIZE);
}

bool isPointOnVisualBorder(owmWindow* window, uint32_t x, uint32_t y) {
  return getVisualBorderSide(window, x, y) != OWM_WINDOW_BORDER_NONE;
}

owmWindowBorder getResizeBorderSide(owmWindow* window, uint32_t x, uint32_t y) {
  return getBorderSideWithBorderWidth(window, x, y, OWM_RESIZE_BORDER_SIZE);
}

void renderWindow(size_t window_idx, OWM_FrameBuffer* frame_buffer) {
  owmWindow* window = &OWM_WINDOWS.windows[window_idx];
  uint32_t *pixel = frame_buffer->pixels;

  uint32_t y_start;
  if (window->pos_y > 0) {
    y_start = (uint32_t) window->pos_y;
  } else {
    y_start = 0;
  }
  uint32_t y_end = window->pos_y + window->height;
  if (y_end >= frame_buffer->height) {
    y_end = frame_buffer->height - 1;
  }

  uint32_t x_start;
  if (window->pos_x > 0) {
    x_start = (uint32_t) window->pos_x;
  } else {
    x_start = 0;
  }
  uint32_t x_end = window->pos_x + window->width;
  if (x_end >= frame_buffer->width) {
    x_end = frame_buffer->width - 1;
  }

  pixel += (frame_buffer->stride) * y_start;
  for (uint32_t y = y_start; y <= y_end; ++y) {
    for (uint32_t x = x_start; x <= x_end; ++x) {
      if (isPointOnVisualBorder(window, x, y)) {
        if (window->focused) {
          pixel[x] = OWM_FOCUSED_WINDOW_BORDER_COLOR;
        }
        continue;
      }
      pixel[x] = window->color;
    }
    pixel += frame_buffer->stride;
  }
}

void OWM_renderWindows(OWM_FrameBuffer* frameBuffer) {
  if (OWM_WINDOWS.count <= 0) {
    return;
  }

  size_t window_idx = OWM_WINDOWS.count;
  do {
    window_idx--;
    renderWindow(window_idx, frameBuffer);
  } while(window_idx > 0);
}

void OWM_processWindowMouseEvent(int32_t mouse_delta_x, int32_t mouse_delta_y) {
  if (OWM_WINDOWS.count <= 0) {
    return;
  }
  owmWindow* window = &OWM_WINDOWS.windows[0];
  owmWindowMouseAction mouse_action = window->mouse_action;
  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_DRAG) {
    window->pos_x += mouse_delta_x;
    window->pos_y += mouse_delta_y;
    return;
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_BORDER) {
    if (mouse_delta_y == 0) {
      return;
    }
    uint32_t new_height = window->height - mouse_delta_y;
    if (new_height < OWM_WINDOW_MIN_HEIGHT) {
      return;
    }
    window->height = new_height;
    window->pos_y += mouse_delta_y;
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_RIGHT_BORDER) {
    if (mouse_delta_y != 0) {
      uint32_t new_height = window->height - mouse_delta_y;
      if (new_height < OWM_WINDOW_MIN_HEIGHT) {
        return;
      }
      window->height = new_height;
      window->pos_y += mouse_delta_y;
    }
    if (mouse_delta_x != 0) {
      uint32_t new_width = window->width + mouse_delta_x;
      if (new_width < OWM_WINDOW_MIN_WIDTH) {
        return;
      }
      window->width = new_width;
    }
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_RIGHT_BORDER) {
    if (mouse_delta_x == 0) {
      return;
    }
    uint32_t new_width = window->width + mouse_delta_x;
    if (new_width < OWM_WINDOW_MIN_WIDTH) {
      return;
    }
    window->width = new_width;
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_RIGHT_BORDER) {
    if (mouse_delta_x != 0) {
      uint32_t new_width = window->width + mouse_delta_x;
      if (new_width < OWM_WINDOW_MIN_WIDTH) {
        return;
      }
      window->width = new_width;
    }
    if (mouse_delta_y != 0) {
      uint32_t new_height = window->height + mouse_delta_y;
      if (new_height < OWM_WINDOW_MIN_HEIGHT) {
        return;
      }
      window->height = new_height;
    }
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_BORDER) {
    if (mouse_delta_y == 0) {
      return;
    }
    uint32_t new_height = window->height + mouse_delta_y;
    if (new_height < OWM_WINDOW_MIN_HEIGHT) {
      return;
    }
    window->height = new_height;
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_LEFT_BORDER) {
    if (mouse_delta_y != 0) {
      uint32_t new_height = window->height + mouse_delta_y;
      if (new_height < OWM_WINDOW_MIN_HEIGHT) {
        return;
      }
      window->height = new_height;
    }
    if (mouse_delta_x != 0) {
      uint32_t new_width = window->width - mouse_delta_x;
      if (new_width < OWM_WINDOW_MIN_WIDTH) {
        return;
      }
      window->width= new_width;
      window->pos_x += mouse_delta_x;
    }
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_LEFT_BORDER) {
    if (mouse_delta_x == 0) {
      return;
    }
    uint32_t new_width = window->width - mouse_delta_x;
    if (new_width < OWM_WINDOW_MIN_WIDTH) {
      return;
    }
    window->width= new_width;
    window->pos_x += mouse_delta_x;
  }

  if (mouse_action == OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_LEFT_BORDER) {
    if (mouse_delta_x != 0) {
      uint32_t new_width = window->width - mouse_delta_x;
      if (new_width < OWM_WINDOW_MIN_WIDTH) {
        return;
      }
      window->width= new_width;
      window->pos_x += mouse_delta_x;
    }
    if (mouse_delta_y != 0) {
      uint32_t new_height = window->height - mouse_delta_y;
      if (new_height < OWM_WINDOW_MIN_HEIGHT) {
        return;
      }
      window->height = new_height;
      window->pos_y += mouse_delta_y;
    }
  }
}

void OWM_processWindowMouseButtonEvent(uint16_t key_code, OWM_KeyEventType event_type) {
  int32_t mouse_x = OWM_getCursorX();
  int32_t mouse_y = OWM_getCursorY();
  if (key_code != OWM_BTN_LEFT) { // For now only handle left mouse button events
    return;
  }

  if (event_type == OWM_EVENT_KEY_EVENT_PRESS) {
    if (OWM_WINDOWS.count <= 0) {
      return;
    }

    int clicked_window_idx = -1;
    for (size_t window_idx = 0; window_idx < OWM_WINDOWS.count; ++window_idx) {
      owmWindow* window = &OWM_WINDOWS.windows[window_idx];
      if (
        window->pos_x <= mouse_x &&
        mouse_x <= window->pos_x + (int32_t) window->width &&
        window->pos_y <= mouse_y &&
         mouse_y <= window->pos_y + (int32_t) window->height
      ) {
        clicked_window_idx = window_idx;
        break;
      }
    }

    OWM_WINDOWS.windows[0].focused = 0;

    if (clicked_window_idx == -1) {
      return;
    }

    // Focus window
    owmWindow clicked_window = OWM_WINDOWS.windows[clicked_window_idx];
    clicked_window.focused = true;
    owmWindowBorder border_side = getResizeBorderSide(&clicked_window, mouse_x, mouse_y);
    if (border_side != OWM_WINDOW_BORDER_NONE) {
      switch(border_side) {
        case OWM_WINDOW_BORDER_TOP:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_BORDER;
          break;
        case OWM_WINDOW_BORDER_TOP_RIGHT:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_RIGHT_BORDER;
          break;
        case OWM_WINDOW_BORDER_RIGHT:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_RIGHT_BORDER;
          break;
        case OWM_WINDOW_BORDER_BOTTOM_RIGHT:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_RIGHT_BORDER;
          break;
        case OWM_WINDOW_BORDER_BOTTOM:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_BORDER;
          break;
        case OWM_WINDOW_BORDER_BOTTOM_LEFT:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_LEFT_BORDER;
          break;
        case OWM_WINDOW_BORDER_LEFT:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_LEFT_BORDER;
          break;
        case OWM_WINDOW_BORDER_TOP_LEFT:
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_LEFT_BORDER;
          break;
        default:
          fprintf(stderr, "Got unexpected border side %d\n", border_side);
        break;
      }
    } else {
      clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_DRAG;
    }

    // Bring it to the front
    for (size_t window_idx = (size_t) clicked_window_idx; window_idx > 0; --window_idx) {
      OWM_WINDOWS.windows[window_idx] = OWM_WINDOWS.windows[window_idx - 1];
    }
    OWM_WINDOWS.windows[0] = clicked_window;
  } else if (event_type == OWM_EVENT_KEY_EVENT_RELEASE) {
    if (OWM_WINDOWS.count <= 0) {
      return;
    }
    OWM_WINDOWS.windows[0].mouse_action = OWM_WINDOW_MOUSE_ACTION_NONE;
  }
}

void OWM_processWindowKeyEvent(uint16_t key_code, OWM_KeyEventType event_type) {
  if (key_code == OWM_KEY_W && event_type == OWM_EVENT_KEY_EVENT_PRESS) {
    createWindow();
  } else if (key_code == OWM_KEY_Q && event_type == OWM_EVENT_KEY_EVENT_PRESS) {
    closeWindow();
  }
}

void OWM_cleanupWindows() {
  free(OWM_WINDOWS.windows);
}
