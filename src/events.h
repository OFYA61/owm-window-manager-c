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
} OfyaEventPollFds;

extern OfyaEventPollFds EVENT_POLL_FDS;

/// Setups up global `EVENT_POLL_FDS`.
/// Requires the input devices `KEYBOARDS` and the `RENDER_DISPLAY` to be set before calling
void OfyaEventPollFds_setup();
void OfyaEventPollFds_poll();
