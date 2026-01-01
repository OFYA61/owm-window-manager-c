#include "window.h"
#include "events.h"
#include "render.h"

#include <linux/input-event-codes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    .mouse_action = OWM_WINDOW_MOUSE_ACTION_NONE,
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

owmWindowBorder owmWindow_get_border_side_with_border_width(owmWindow* window, uint32_t x, uint32_t y, uint32_t border_width) {
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

owmWindowBorder owmWindow_get_border_side(owmWindow* window, uint32_t x, uint32_t y){
  return owmWindow_get_border_side_with_border_width(window, x, y, OWM_BORDER_SIZE);
}

bool owmWindow_within_border(owmWindow* window, uint32_t x, uint32_t y) {
  return owmWindow_get_border_side(window, x, y) != OWM_WINDOW_BORDER_NONE;
}

owmWindowBorder owmWindow_get_resize_border_size(owmWindow* window, uint32_t x, uint32_t y) {
  return owmWindow_get_border_side_with_border_width(window, x, y, OWM_RESIZE_BORDER_SIZE);
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

  pixel += (frameBuffer->buffer.pitch / 4) * y_start;
  for (uint32_t y = y_start; y <= y_end; ++y) {
    for (uint32_t x = x_start; x <= x_end; ++x) {
      if (owmWindow_within_border(window, x, y)) {
        if (window->focused) {
          pixel[x] = OWM_FOCUSED_WINDOW_BORDER_COLOR;
        }
        continue;
      }
      pixel[x] = window->color;
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

void owmWindows_process_mouse_key_event(uint32_t mouse_x, uint32_t mouse_y, uint16_t key_code, owmEventKeyEventType event_type) {
  if (key_code != BTN_LEFT) { // For now only handle left mouse button events
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

    // Focus window
    owmWindow clicked_window = OWM_WINDOWS.windows[clicked_window_idx];
    clicked_window.focused = true;
    owmWindowBorder border_side = owmWindow_get_resize_border_size(&clicked_window, mouse_x, mouse_y);
    if (border_side != OWM_WINDOW_BORDER_NONE) {
      switch(border_side) {
        case OWM_WINDOW_BORDER_TOP:
          printf("TOP\n");
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_BORDER;
          break;
        case OWM_WINDOW_BORDER_TOP_RIGHT:
          printf("TOP RIGHT\n");
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_TOP_RIGHT_BORDER;
          break;
        case OWM_WINDOW_BORDER_RIGHT:
          printf("RIGHT\n");
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_RIGHT_BORDER;
          break;
        case OWM_WINDOW_BORDER_BOTTOM_RIGHT:
          printf("BOTTOM RIGHT\n");
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_RIGHT_BORDER;
          break;
        case OWM_WINDOW_BORDER_BOTTOM:
          printf("BOTTOM\n");
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_BORDER;
          break;
        case OWM_WINDOW_BORDER_BOTTOM_LEFT:
          printf("BOTTOM LEFT\n");
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_BOTTOM_LEFT_BORDER;
          break;
        case OWM_WINDOW_BORDER_LEFT:
          printf("LEFT\n");
          clicked_window.mouse_action = OWM_WINDOW_MOUSE_ACTION_RESIZE_LEFT_BORDER;
          break;
        case OWM_WINDOW_BORDER_TOP_LEFT:
          printf("TOP LEFT\n");
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

void owmWindows_process_key_event(uint16_t key_code, owmEventKeyEventType event_type) {
  if (key_code == KEY_W && event_type == OWM_EVENT_KEY_EVENT_PRESS) {
    owmWindows_create_window();
  } else if (key_code == KEY_Q && event_type == OWM_EVENT_KEY_EVENT_PRESS) {
    owmWindows_close_window();
  }
}

void owmWindows_cleanup() {
  free(OWM_WINDOWS.windows);
}
