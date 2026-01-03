#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
  OWM_EVENT_KEY_EVENT_PRESS,
  OWM_EVENT_KEY_EVENT_RELEASE,
  OWM_EVENT_KEY_EVENT_PRESS_REPEATE
} OWM_KeyEventType;

void OWM_setKeyboardKeyPressCallback(void (*callback)(uint16_t key_code, OWM_KeyEventType event_type));
void OWM_submitKeyboardKeyPressCallback(uint16_t key_code, bool pressed);
void OWM_setMouseKeyPressCallback(void (*callback)(uint16_t key_code, OWM_KeyEventType event_type));
void OWM_submitMouseKeyPressCallback(uint16_t key_code, bool pressed);
void OWM_setMouseMoveCallback(void (*callback)(int rel_x, int rel_y));
void OWM_submitMouseMoveCallback(int rel_x, int rel_y);
