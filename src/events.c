#include "events.h"

#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <unistd.h>
#include <xf86drm.h>

#include "input.h"
#include "render.h"

owmEventPollFds OWM_EVENT_POLL_FDS = { 0 };

void owmEvents_setup() {
  const owmKeyboards *keyboards = owmKeyboards_get();
  const owmMice *mice = owmMice_get();
  size_t size = 1 +                 // 1 for render display
                keyboards->count +  // Keyboard input devices
                mice->count;        // Mouse input devices
  OWM_EVENT_POLL_FDS.pollfds = malloc(sizeof(struct pollfd) * size);
  OWM_EVENT_POLL_FDS.count = size;

  // Register render display
  size_t index = 0;
  OWM_EVENT_POLL_FDS.pollfds[index].fd = owmRenderDisplay_get_fd_card();
  OWM_EVENT_POLL_FDS.pollfds[index].events = POLLIN;
  OWM_EVENT_POLL_FDS.display_idx = index;
  index++;

  // Register keyboards
  size_t kbds_to_process = keyboards->count;
  OWM_EVENT_POLL_FDS.input_kbd_start_idx = index;
  while (kbds_to_process > 0) {
    OWM_EVENT_POLL_FDS.pollfds[index].fd = keyboards->fds[kbds_to_process - 1];
    OWM_EVENT_POLL_FDS.pollfds[index].events = POLLIN;
    index++;
    --kbds_to_process;
  }
  OWM_EVENT_POLL_FDS.input_kbd_end_idx = index - 1;

  // Register mice
  size_t mice_to_process = mice->count;
  OWM_EVENT_POLL_FDS.input_mice_start_idx = index;
  while(mice_to_process > 0) {
    OWM_EVENT_POLL_FDS.pollfds[index].fd = mice->fds[mice_to_process - 1];
    OWM_EVENT_POLL_FDS.pollfds[index].events = POLLIN;
    index++;
    --mice_to_process;
  }
  OWM_EVENT_POLL_FDS.input_mice_end_idx = index - 1;
}

void (*owmEvents_keyboard_key_press_callback)(uint16_t key_code, bool pressed) = NULL;
void (*owmEvents_mouse_key_press_callback)(uint16_t key_code, bool presssed) = NULL;
void (*owmEvents_mouse_move_callback)(int rel_x, int rel_y) = NULL;

void owmEvents_set_keyboard_key_press_callback(void (*callback)(uint16_t key_code, bool pressed)) {
  owmEvents_keyboard_key_press_callback = callback;
}

void owmEvents_set_mouse_key_press_callback(void (*callback)(uint16_t key_code, bool pressed)) {
  owmEvents_mouse_key_press_callback = callback;
}

void owmEvents_set_mouse_move_callback(void (*callback)(int rel_x, int rel_y)) {
  owmEvents_mouse_move_callback = callback;
}

// TODO: make custom code to handle key events, since the OS sends reapeadted key down events if the key is being held down
void owmEvents_poll() {
  int timeout = owmRenderContext_is_next_frame_buffer_free() ? 10 : -1;
  int num_events = poll(OWM_EVENT_POLL_FDS.pollfds, OWM_EVENT_POLL_FDS.count, timeout);
  if (num_events == 0) {
    return;
  }

  struct pollfd *pfds = OWM_EVENT_POLL_FDS.pollfds;

  // TODO: check for `revents` bits `POLLERR` `POLLHUP` `POLLNVAL` for input hot-unplug or DRM fd errors

  for (size_t kbd_poll_fd_idx = OWM_EVENT_POLL_FDS.input_kbd_start_idx; kbd_poll_fd_idx <= OWM_EVENT_POLL_FDS.input_kbd_end_idx; kbd_poll_fd_idx++) {
    if (pfds[kbd_poll_fd_idx].revents & POLLIN) {
      struct input_event ev;
      while (read(pfds[kbd_poll_fd_idx].fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_KEY) {
          if (owmEvents_keyboard_key_press_callback != NULL) {
            owmEvents_keyboard_key_press_callback(ev.code, ev.value ? true : false);
          }
        }
      }
    }
  }

  for (size_t mice_poll_fd_idx = OWM_EVENT_POLL_FDS.input_mice_start_idx; mice_poll_fd_idx <= OWM_EVENT_POLL_FDS.input_mice_end_idx; mice_poll_fd_idx++) {
    if (pfds[mice_poll_fd_idx].revents & POLLIN) {
      struct input_event ev;
      static int rel_x = 0;
      static int rel_y = 0;
      while (read(pfds[mice_poll_fd_idx].fd, &ev, sizeof(ev)) == sizeof(ev)) {
        if (ev.type == EV_REL) {
          if (ev.code == REL_X) {
            rel_x += ev.value;
          } else if (ev.code == REL_Y) {
            rel_y += ev.value;
          } else if (ev.code == REL_WHEEL) {
            // TODO: handle scroll wheel
          }
        } else if (ev.type == EV_KEY) {
          if (owmEvents_mouse_key_press_callback != NULL) {
            owmEvents_mouse_key_press_callback(ev.code, ev.value ? true : false);
          }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) { // The mouse "packet" is complete. Dispatch total movement
          if (owmEvents_mouse_move_callback != NULL) {
            owmEvents_mouse_move_callback(rel_x, rel_y);
          }
          rel_x = 0;
          rel_y = 0;
        }
      }
    }
  }

  if (pfds[OWM_EVENT_POLL_FDS.display_idx].revents & POLLIN) {
    drmEventContext ev = {
      .version = DRM_EVENT_CONTEXT_VERSION,
      .page_flip_handler = owmRenderContext_page_flip_handler
    };
    drmHandleEvent(pfds[OWM_EVENT_POLL_FDS.display_idx].fd, &ev);
  }
}

void owmEvents_cleanup() {
  free(OWM_EVENT_POLL_FDS.pollfds);
}
