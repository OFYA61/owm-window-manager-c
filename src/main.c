#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "backend/backend.h"
#include "owm.h"
#include "core/window.h"

bool running = true;
uint32_t mouse_pos_x = 0;
uint32_t mouse_pos_y = 0;
uint32_t display_width = 0;
uint32_t display_height = 0;

void keyboardKeyPressCallback(uint16_t key_code, OWM_KeyEventType event_type) {
  if (event_type == OWM_EVENT_KEY_EVENT_PRESS && key_code == KEY_ESC) {
    running = false;
  }
  OWM_processWindowKeyEvent(key_code, event_type);
}

void mouseKeyPressCallback(uint16_t key_code, OWM_KeyEventType event_type) {
  OWM_processWindowMouseButtonEvent(mouse_pos_x, mouse_pos_y, key_code, event_type);
}

void mouseMoveCallback(int rel_x, int rel_y) {
  if (rel_x < 0 && (uint32_t) abs(rel_x) > mouse_pos_x) {
    mouse_pos_x = 0;
    rel_x = 0;
  } else if (mouse_pos_x + rel_x > display_width - 1) {
    rel_x = display_width - 1 - mouse_pos_x;
    mouse_pos_x = display_width - 1;
  } else {
    mouse_pos_x += rel_x;
  }

  if (rel_y < 0 && (uint32_t) abs(rel_y) > mouse_pos_y) {
    mouse_pos_y = 0;
    rel_y = 0;
  } else if (mouse_pos_y + rel_y > display_height - 1) {
    rel_y = display_height - 1 - mouse_pos_y;
    mouse_pos_y = display_height - 1;
  } else {
    mouse_pos_y += rel_y;
  }

  OWM_processWindowMouseEvent(mouse_pos_x, mouse_pos_y, rel_x, rel_y);
}

int main() {
  srand(time(NULL)); // Just to get different colors on dummy windows on each run

  if (OWM_init(OWM_BACKEND_TYPE_LINUX)) {
    fprintf(stderr, "Failed to initialize owm\n");
    return 1;
  }

  OWM_Backend* context = OWM_getContext();
  display_width = context->getDisplayWidth();
  display_height = context->getDisplayHeight();

  OWM_setKeyboardKeyPressCallback(keyboardKeyPressCallback);
  OWM_setMouseKeyPressCallback(mouseKeyPressCallback);
  OWM_setMouseMoveCallback(mouseMoveCallback);

  while (running) {
    context->dispatch();

    OWM_FrameBuffer *frame_buffer;
    if((frame_buffer = context->aquireFreeFrameBuffer()) != NULL) {
      // Render
      // Clear screen
      uint32_t clear_color = 0x00000000;
      uint32_t *pixel = frame_buffer->pixels;
      for (uint32_t y = 0; y < frame_buffer->height; ++y) {
        for (uint32_t x = 0; x < frame_buffer->width; ++x) {
          pixel[x] = clear_color;
        }
        pixel += frame_buffer->stride;
      }

      // Draw windows
      OWM_renderWindows(frame_buffer);

      // Draw cursor
      uint32_t cursor_color = 0x00FFFFFF;
      pixel = frame_buffer->pixels;
      pixel[mouse_pos_y * frame_buffer->stride + mouse_pos_x] = cursor_color;

      if (context->swapBuffers() != 0) {
        fprintf(stderr, "Failed to submit swap frame buffer request\n");
      }
    }
  }

  OWM_shutdown();

  return 0;
}
