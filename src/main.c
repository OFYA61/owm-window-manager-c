#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "backend/backend.h"
#include "core/cursor.h"
#include "core/event.h"
#include "core/input.h"
#include "owm.h"
#include "core/window.h"

bool running = true;

static void keyboardKeyPressCallback(OWM_KeyCode key_code, OWM_KeyEventType event_type) {
  if (event_type == OWM_EVENT_KEY_EVENT_PRESS && key_code == OWM_KEY_ESC) {
    running = false;
  }
  OWM_processWindowKeyEvent(key_code, event_type);
}

static void mouseKeyPressCallback(OWM_KeyCode key_code, OWM_KeyEventType event_type) {
  OWM_processWindowMouseButtonEvent(key_code, event_type);
}

static void mouseMoveCallback(int rel_x, int rel_y) {
  OWM_updateCursorPosition(rel_x, rel_y);
  OWM_processWindowMouseEvent(rel_x, rel_y);
}

static void mouseSetPositionCallback(int x, int y) {
  OWM_setCursorPosition(x, y);
}

int main() {
  srand(time(NULL)); // Just to get different colors on dummy windows on each run

  if (OWM_init(OWM_BACKEND_TYPE_WAYLAND)) {
    fprintf(stderr, "Failed to initialize owm\n");
    return 1;
  }

  OWM_Backend* backend = OWM_getContext();
  uint32_t display_width = backend->getDisplayWidth();
  uint32_t display_height = backend->getDisplayHeight();
  OWM_updateCursorConfines(0, display_width, 0, display_height);

  OWM_setKeyboardKeyPressCallback(keyboardKeyPressCallback);
  OWM_setMouseKeyPressCallback(mouseKeyPressCallback);
  OWM_setMouseMoveCallback(mouseMoveCallback);
  OWM_setMouseSetPositionCallback(mouseSetPositionCallback);

  while (running) {
    backend->dispatch();

    OWM_FrameBuffer *frame_buffer;
    if((frame_buffer = backend->aquireFreeFrameBuffer()) != NULL) {
      // Render
      // Clear screen
      // TODO: extract into separete function on BE to clear a given part of the screen
      uint32_t clear_color = 0x00000000;
      uint32_t *pixel = frame_buffer->pixels;
      for (uint32_t y = 0; y < frame_buffer->height; ++y) {
        for (uint32_t x = 0; x < frame_buffer->width; ++x) {
          pixel[x] = clear_color;
        }
        pixel += frame_buffer->stride;
      }

      OWM_renderWindows(frame_buffer);
      OWM_renderCursor(frame_buffer);

      if (backend->swapBuffers() != 0) {
        fprintf(stderr, "Failed to submit swap frame buffer request\n");
      }
    }
  }

  OWM_shutdown();

  return 0;
}
