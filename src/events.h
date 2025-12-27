#pragma once

#include <stddef.h>
#include <sys/poll.h>

extern struct pollfd *POLL_FDS;

typedef struct {
  struct pollfd *pollfds;
  size_t count;
  size_t display_idx;
  size_t input_kbd_start_idx;
  size_t input_kbd_end_idx;
} owmEventPollFds;

extern owmEventPollFds OWM_EVENT_POLL_FDS;

/// Setups up global `OWM_EVENT_POLL_FDS`.
/// Requires the input devices `OWM_KEYBOARDS` and the `OWM_RENDER_DISPLAY` to be set before calling
void owmEventPollFds_setup();
void owmEventPollFds_poll();
