#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "owm.h"
#include "events.h"
#include "render.h"
#include "window.h"

bool running = true;
uint32_t mouse_pos_x = 0;
uint32_t mouse_pos_y = 0;

void keyboard_key_press_callback(uint16_t key_code, owmEventKeyEventType event_type) {
  if (event_type == OWM_EVENT_KEY_EVENT_PRESS && key_code == KEY_ESC) {
    running = false;
  }
  owmWindows_process_key_event(key_code, event_type);
}

void mouse_key_press_callback(uint16_t key_code, owmEventKeyEventType event_type) {
  owmWindows_process_mouse_key_event(mouse_pos_x, mouse_pos_y, key_code, event_type);
}

void mouse_move_callback(int rel_x, int rel_y) {
  if (rel_x < 0 && (uint32_t) abs(rel_x) > mouse_pos_x) {
    mouse_pos_x = 0;
    rel_x = 0;
  } else if (mouse_pos_x + rel_x > owmRenderDisplay_get_width() - 1) {
    rel_x = owmRenderDisplay_get_width() - 1 - mouse_pos_x;
    mouse_pos_x = owmRenderDisplay_get_width() - 1;
  } else {
    mouse_pos_x += rel_x;
  }

  if (rel_y < 0 && (uint32_t) abs(rel_y) > mouse_pos_y) {
    mouse_pos_y = 0;
    rel_y = 0;
  } else if (mouse_pos_y + rel_y > owmRenderDisplay_get_height() - 1) {
    rel_y = owmRenderDisplay_get_height() - 1 - mouse_pos_y;
    mouse_pos_y = owmRenderDisplay_get_height() - 1;
  } else {
    mouse_pos_y += rel_y;
  }

  owmWindows_process_mouse_move_event(mouse_pos_x, mouse_pos_y, rel_x, rel_y);
}

int main() {
  srand(time(NULL)); // Just to get different colors on dummy windows on each run

  if (owm_init()) {
    fprintf(stderr, "Failed to initialize owm\n");
    return 1;
  }

  owmEvents_set_keyboard_key_press_callback(keyboard_key_press_callback);
  owmEvents_set_mouse_key_press_callback(mouse_key_press_callback);
  owmEvents_set_mouse_move_callback(mouse_move_callback);

  while (running) {
    owmEvents_poll();

    if (owmRenderContext_is_next_frame_buffer_free()) {
      // Render
      owmFrameBuffer *frameBuffer = owmRenderContext_get_free_buffer();
      uint32_t clear_color = 0x00000000;
      uint32_t *pixel = frameBuffer->buffer.map;
      // Clear screen
      for (uint32_t y = 0; y < owmRenderDisplay_get_height(); ++y) {
        for (uint32_t x = 0; x < owmRenderDisplay_get_width(); ++x) {
          pixel[x] = clear_color;
        }
        pixel += frameBuffer->buffer.pitch / 4; // Divide by 4, since pixel jumps by 4 bytes
      }

      // Draw windows
      owmWindows_render(frameBuffer);

      // Draw cursor
      uint32_t cursor_color = 0x00FFFFFF;
      pixel = frameBuffer->buffer.map;
      pixel[mouse_pos_y * frameBuffer->buffer.pitch / 4 + mouse_pos_x] = cursor_color;

      if (owmRenderContext_submit_frame_buffer_swap_request() != 0) {
        fprintf(stderr, "Failed to submit swap frame buffer request\n");
      }
    }
  }

  owm_cleanup();

  return 0;
}
