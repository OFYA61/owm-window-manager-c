#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int *fds;
  size_t count;
} owmKeyboards;

typedef struct {
  int *fds;
  size_t count;
} owmMice;

int owmInput_setup();
void owmInput_close();
void owmKeyboard_handle_poll_event(int kbd_fd);
void owmMice_handle_poll_event(int mouse_fd);

const owmKeyboards* owmKeyboards_get();
const owmMice* owmMice_get();

void owmKeyboards_set_key_press_callback(void (*callback)(uint16_t key_code, bool pressed));
