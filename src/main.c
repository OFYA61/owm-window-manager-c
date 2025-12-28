#include <linux/input-event-codes.h>
#include <stdio.h>
#include <stdlib.h>

#include "owm.h"
#include "events.h"
#include "render.h"

bool running = true;
uint32_t mouse_pos_x = 0;
uint32_t mouse_pos_y = 0;

void keyboard_key_press_callback(uint16_t key_code, bool pressed) {
  if (pressed && key_code == KEY_ESC) {
    running = false;
  }
}

void mouse_key_press_callback(uint16_t key_code, bool pressed) {
}

void mouse_move_callback(int rel_x, int rel_y) {
  if (rel_x < 0 && (uint32_t) abs(rel_x) > mouse_pos_x) {
    mouse_pos_x = 0;
  } else if (mouse_pos_x + rel_x > owmRenderDisplay_get_width()) {
    mouse_pos_x = owmRenderDisplay_get_width();
  } else {
    mouse_pos_x += rel_x;
  }

  if (rel_y < 0 && (uint32_t) abs(rel_y) > mouse_pos_y) {
    mouse_pos_y = 0;
  } else if (mouse_pos_y + rel_y > owmRenderDisplay_get_height()) {
    mouse_pos_y = owmRenderDisplay_get_height();
  } else {
    mouse_pos_y += rel_y;
  }
}

int main() {
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
      uint32_t color = 0x00000000;
      uint32_t cursor_color = 0x00FFFFFF;
      uint32_t *pixel = frameBuffer->buffer.map;
      for (uint32_t y = 0; y < owmRenderDisplay_get_height(); ++y) {
        for (uint32_t x = 0; x < owmRenderDisplay_get_width(); ++x) {
          if (x == mouse_pos_x && y == mouse_pos_y) {
            pixel[x] = cursor_color;
          } else {
            pixel[x] = color;
          }
        }
        pixel += frameBuffer->buffer.pitch / 4; // Divide by 4, since pixel jumps by 4 bytes
      }

      if (owmRenderContext_submit_frame_buffer_swap_request() != 0) {
        fprintf(stderr, "Failed to submit swap frame buffer request\n");
      }
    }
  }

  owm_cleanup();

  return 0;
}
