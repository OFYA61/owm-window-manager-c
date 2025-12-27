#include "events.h"

#include "display.h"
#include "input.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/poll.h>
#include <xf86drm.h>

owmEventPollFds OWM_EVENT_POLL_FDS = {NULL, 0, 0, 0, 0};

void owmEventPollFds_setup() {
  size_t size = 1 +              // 1 for render display
                OWM_KEYBOARDS.count; // Keyboard input devices
  OWM_EVENT_POLL_FDS.pollfds = malloc(sizeof(struct pollfd) * size);
  OWM_EVENT_POLL_FDS.count = size;

  // Register render display
  size_t index = 0;
  OWM_EVENT_POLL_FDS.pollfds[index].fd = OWM_RENDER_DISPLAY.display->fd_card;
  OWM_EVENT_POLL_FDS.pollfds[index].events = POLLIN;
  OWM_EVENT_POLL_FDS.display_idx = index;
  index++;

  size_t kbds_to_process = OWM_KEYBOARDS.count;
  OWM_EVENT_POLL_FDS.input_kbd_start_idx = index;
  while (kbds_to_process > 0) {
    OWM_EVENT_POLL_FDS.pollfds[index].fd = OWM_KEYBOARDS.kbd_fds[kbds_to_process - 1];
    OWM_EVENT_POLL_FDS.pollfds[index].events = POLLIN;
    index++;
    --kbds_to_process;
  }
  OWM_EVENT_POLL_FDS.input_kbd_end_idx = index - 1;
}

void owmEventPollFds_poll() {
  int ret = poll(OWM_EVENT_POLL_FDS.pollfds, OWM_EVENT_POLL_FDS.count, -1);
  if (ret == 0) {
    return;
  }
  if (ret < 0) {
    perror("OfyaEventPollFds_poll: poll");
    return;
  }

  struct pollfd *pfds = OWM_EVENT_POLL_FDS.pollfds;

  // TODO: check for `revents` bits `POLLERR` `POLLHUP` `POLLNVAL` for input hot-unplug or DRM fd errors

  if (pfds[OWM_EVENT_POLL_FDS.display_idx].revents & POLLIN) {
    drmEventContext ev = {
      .version = DRM_EVENT_CONTEXT_VERSION,
      .page_flip_handler = owmRenderContext_page_flip_handler
    };
    drmHandleEvent(pfds[OWM_EVENT_POLL_FDS.display_idx].fd, &ev);
  }

  for (size_t kbd_poll_fd_idx = OWM_EVENT_POLL_FDS.input_kbd_start_idx; kbd_poll_fd_idx <= OWM_EVENT_POLL_FDS.input_kbd_end_idx; kbd_poll_fd_idx++) {
    if (pfds[kbd_poll_fd_idx].revents & POLLIN) {
      owmKeyboard_handle_poll_event(pfds[kbd_poll_fd_idx].fd);
    }
  }
}
