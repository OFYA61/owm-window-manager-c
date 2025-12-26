#include "events.h"

#include "display.h"
#include "input.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <xf86drm.h>

OfyaEventPollFds EVENT_POLL_FDS = {NULL, 0, 0, 0, 0};

void OfyaEventPollFds_setup() {
  size_t size = 1 +              // 1 for render display
                KEYBOARDS.count; // Keyboard input devices
  EVENT_POLL_FDS.pollfds = malloc(sizeof(struct pollfd) * size);
  EVENT_POLL_FDS.count = size;

  // Register render display
  size_t index = 0;
  EVENT_POLL_FDS.pollfds[index].fd = RENDER_DISPLAY.display->fd_card;
  EVENT_POLL_FDS.pollfds[index].events = POLLIN;
  EVENT_POLL_FDS.display_idx = index;
  index++;

  size_t kbds_to_process = KEYBOARDS.count;
  EVENT_POLL_FDS.input_kbd_start_idx = index;
  while (kbds_to_process > 0) {
    EVENT_POLL_FDS.pollfds[index].fd = KEYBOARDS.kbd_fds[kbds_to_process - 1];
    EVENT_POLL_FDS.pollfds[index].events = POLLIN;
    index++;
    --kbds_to_process;
  }
  EVENT_POLL_FDS.input_kbd_end_idx = index - 1;
}

void OfyaEventPollFds_poll() {
  int ret = poll(EVENT_POLL_FDS.pollfds, EVENT_POLL_FDS.count, -1);
  if (ret == 0) {
    return;
  }
  if (ret < 0) {
    perror("OfyaEventPollFds_poll: poll");
    return;
  }

  struct pollfd *pfds = EVENT_POLL_FDS.pollfds;

  if (pfds[EVENT_POLL_FDS.display_idx].revents & POLLIN) {
    drmEventContext ev = {
      .version = DRM_EVENT_CONTEXT_VERSION,
      .page_flip_handler = page_flip_handler
    };
    drmHandleEvent(pfds[EVENT_POLL_FDS.display_idx].fd, &ev);
  }

  for (size_t kbd_poll_fd_idx = EVENT_POLL_FDS.input_kbd_start_idx; kbd_poll_fd_idx <= EVENT_POLL_FDS.input_kbd_end_idx; kbd_poll_fd_idx++) {
    if (pfds[kbd_poll_fd_idx].revents & POLLIN) {
      OfyaKeyboard_handle_poll_event(pfds[kbd_poll_fd_idx].fd);
    }
  }
}
