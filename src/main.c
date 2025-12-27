#include <linux/input-event-codes.h>
#include <stdio.h>

#include "display.h"
#include "events.h"
#include "input.h"
#include "render.h"

bool running = true;

void key_pressed_callback(uint16_t key_code, bool pressed) {
  if (pressed && key_code == KEY_ESC) {
    running = false;
  }
}

int main() {
  if (owmKeyboards_setup()) {
    fprintf(stderr, "Failed to find a keyboard\n");
    return 1;
  }
  owmKeyboards_set_key_press_callback(key_pressed_callback);

  if (owmDisplays_scan()) {
    perror("owmDisplay_scan");
    return 1;
  }

  if (owmRenderDisplay_pick()) {
    owmDisplays_close();
    return 1;
  }

  owmEventPollFds_setup();

  if (owmRenderContext_init()) {
    owmDisplays_close();
    return 1;
  }
  owmEventPollFds_setup();

  owmDisplay *display = OWM_RENDER_DISPLAY.display;
  drmModeModeInfo* mode = &display->display_modes[OWM_RENDER_DISPLAY.selected_mode_idx];

  uint32_t frame_count = 0;

  while (running) {
    owmEventPollFds_poll();

    if (owmRenderContext_can_swap_frame()) {
      // Render
      owmFrameBuffer *frameBuffer = owmRenderContext_get_free_buffer();
      uint32_t color = frame_count & 1 ? 0x00FF0000 : 0x000000FF;
      uint32_t *pixel = frameBuffer->buffer.map;
      for (uint32_t y = 0; y < mode->vdisplay; ++y) {
        for (uint32_t x = 0; x < mode->hdisplay; ++x) {
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

  owmKeyboards_close();
  owmRenderContext_close();
  owmDisplays_close();

  return 0;
}
