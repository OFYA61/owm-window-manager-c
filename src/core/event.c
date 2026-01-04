#include "event.h"

#include <stdint.h>
#include <string.h>

#include "input.h"

#define BITSIZE(bits) ((bits + 7) / 8)

// Stores if the key is pressed or not, we need to keep track of this in order to differentiate between
// an initial press and a repeat press
uint8_t KEY_STATUS[BITSIZE(OWM_KEY_MAX)] = { 0 };

#define IS_KEY_PRESSED(idx) (KEY_STATUS[(idx)/8] & (1 << ((idx) % 8)))
#define CLEAR_KEY_PRESS(idx) (KEY_STATUS[(idx)/8] &= ~(1 << ((idx) % 8)))
#define MARK_KEY_PRESSED(idx) (KEY_STATUS[(idx)/8] |= (1 << ((idx) % 8)))

OWM_KeyEventType getKeyEventType(uint16_t key_code, bool pressed) {
  if (!pressed) {
    CLEAR_KEY_PRESS(key_code);
    return OWM_EVENT_KEY_EVENT_RELEASE;
  }
  if (IS_KEY_PRESSED(key_code)) {
    return OWM_EVENT_KEY_EVENT_PRESS_REPEATE;
  } else {
    MARK_KEY_PRESSED(key_code);
    return OWM_EVENT_KEY_EVENT_PRESS;
  }
}

void (*owm_keyboard_key_press_callback)(OWM_KeyCode key_code, OWM_KeyEventType event_type) = NULL;
void (*owm_mouse_key_press_callback)(OWM_KeyCode key_code, OWM_KeyEventType event_type) = NULL;
void (*owm_mouse_move_callback)(int rel_x, int rel_y) = NULL;

void OWM_setKeyboardKeyPressCallback(void (*callback)(OWM_KeyCode key_code, OWM_KeyEventType event_type)) {
  owm_keyboard_key_press_callback = callback;
}

void OWM_submitKeyboardKeyPressCallback(uint16_t key_code, bool pressed) {
  owm_keyboard_key_press_callback(key_code, getKeyEventType(key_code, pressed));
}

void OWM_setMouseKeyPressCallback(void (*callback)(OWM_KeyCode key_code, OWM_KeyEventType event_type)) {
  owm_mouse_key_press_callback = callback;
}

void OWM_submitMouseKeyPressCallback(uint16_t key_code, bool pressed) {
  owm_mouse_key_press_callback(key_code, getKeyEventType(key_code, pressed));
}

void OWM_setMouseMoveCallback(void (*callback)(int rel_x, int rel_y)) {
  owm_mouse_move_callback = callback;
}

void OWM_submitMouseMoveCallback(int rel_x, int rel_y) {
  owm_mouse_move_callback(rel_x, rel_y);
}
