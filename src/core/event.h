#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "input.h"

typedef enum {
  OWM_EVENT_KEY_EVENT_PRESS,
  OWM_EVENT_KEY_EVENT_RELEASE,
  OWM_EVENT_KEY_EVENT_PRESS_REPEATE
} OWM_KeyEventType;

void OWM_setKeyboardKeyPressCallback(void (*callback)(OWM_KeyCode key_code, OWM_KeyEventType event_type));
void OWM_submitKeyboardKeyPressCallback(uint16_t key_code, bool pressed);
void OWM_setMouseKeyPressCallback(void (*callback)(OWM_KeyCode key_code, OWM_KeyEventType event_type));
void OWM_setMouseSetPositionCallback(void (*callback)(int x, int y));
void OWM_submitMouseKeyPressCallback(uint16_t key_code, bool pressed);
void OWM_setMouseMoveCallback(void (*callback)(int rel_x, int rel_y));
void OWM_submitMouseMoveCallback(int rel_x, int rel_y);
void OWM_submitMouseSetPositionCallback(int x, int y);

