#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/poll.h>

typedef enum {
  OWM_EVENT_KEY_EVENT_PRESS,
  OWM_EVENT_KEY_EVENT_RELEASE,
  OWM_EVENT_KEY_EVENT_PRESS_REPEATE
} OWM_KeyEventType;

/// Setups up global `OWM_EVENT_POLL_FDS`.
/// Requires the input devices `OWM_KEYBOARDS` and the `OWM_RENDER_DISPLAY` to be set before calling
int OWM_setupEvents();
/// Poll for events
void OWM_pollEvents();
/// Cleanup input device objects
void OWM_cleanupEvents();

/// Set callback function for keyboard key press events
void OWM_setKeyboardKeyPressCallback(void (*callback)(uint16_t key_code, OWM_KeyEventType event_type));
/// Set callback function for mouse key press events
void OWM_setMouseKeyPressCallback(void (*callback)(uint16_t key_code, OWM_KeyEventType event_type));
/// Set callback function for mouse movement events
void OWM_setMouseMoveCallback(void (*callback)(int rel_x, int rel_y));
