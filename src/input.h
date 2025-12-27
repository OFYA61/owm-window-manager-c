#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int *kbd_fds;
  size_t count;
} owmKeyboards;

extern owmKeyboards OWM_KEYBOARDS;

int owmKeyboards_setup();
void owmKeyboards_close();
void owmKeyboard_handle_poll_event(int kbd_fd);

void owmKeyboards_set_key_press_callback(void (*callback)(uint16_t key_code, bool pressed));
