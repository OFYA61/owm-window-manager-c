#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/poll.h>

typedef struct {
  struct pollfd *pollfds;
  size_t count;
  size_t display_idx;
  size_t input_kbd_start_idx;
  size_t input_kbd_end_idx;
  size_t input_mice_start_idx;
  size_t input_mice_end_idx;
} owmEventPollFds;

typedef enum {
  OWM_EVENT_KEY_EVENT_PRESS,
  OWM_EVENT_KEY_EVENT_RELEASE,
  OWM_EVENT_KEY_EVENT_PRESS_REPEATE
} owmEventKeyEventType;

/// Setups up global `OWM_EVENT_POLL_FDS`.
/// Requires the input devices `OWM_KEYBOARDS` and the `OWM_RENDER_DISPLAY` to be set before calling
void owmEvents_setup();
/// Poll for events
void owmEvents_poll();
/// Cleanup input device objects
void owmEvents_cleanup();

/// Set callback function for keyboard key press events
void owmEvents_set_keyboard_key_press_callback(void (*callback)(uint16_t key_code, owmEventKeyEventType event_type));
/// Set callback function for mouse key press events
void owmEvents_set_mouse_key_press_callback(void (*callback)(uint16_t key_code, owmEventKeyEventType event_type));
/// Set callback function for mouse movement events
void owmEvents_set_mouse_move_callback(void (*callback)(int rel_x, int rel_y));
