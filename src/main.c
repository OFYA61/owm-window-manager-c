#include <linux/input-event-codes.h>
#include <stdio.h>

#include "owm.h"
#include "events.h"
#include "input.h"
#include "render.h"

bool running = true;

void keyboard_key_press_callback(uint16_t key_code, bool pressed) {
  if (pressed && key_code == KEY_ESC) {
    running = false;
  }
}

void mouse_key_press_callback(uint16_t key_code, bool pressed) {
}

void mouse_move_callback(int rel_x, int rel_y) {
}

int main() {
  if (owm_init()) {
    fprintf(stderr, "Failed to initialize owm\n");
    return 1;
  }

  owmEvents_set_keyboard_key_press_callback(keyboard_key_press_callback);
  owmEvents_set_mouse_key_press_callback(mouse_key_press_callback);
  owmEvents_set_mouse_move_callback(mouse_move_callback);

  uint32_t frame_count = 0;

  while (running) {
    owmEvents_poll();

    if (owmRenderContext_can_swap_frame()) {
      // Render
      owmFrameBuffer *frameBuffer = owmRenderContext_get_free_buffer();
      uint32_t color = frame_count & 1 ? 0x00FF0000 : 0x000000FF;
      uint32_t *pixel = frameBuffer->buffer.map;
      for (uint32_t y = 0; y < owmRenderDisplay_get_height(); ++y) {
        for (uint32_t x = 0; x < owmRenderDisplay_get_width(); ++x) {
          pixel[x] = color;
        }
        pixel += frameBuffer->buffer.pitch / 4; // Divide by 4, since pixel jumps by 4 bytes
      }

      // Submit render
      if (owmRenderContext_swap_frame_buffer() == 0) {
        frame_count++;
      }
    }
  }

  owm_cleanup();

  return 0;
}
