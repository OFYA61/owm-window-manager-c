#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int *kbd_fds;
  size_t count;
} OfyaKeyboards;

extern OfyaKeyboards KEYBOARDS;

int OfyaKeyboards_setup();
void OfyaKeyboards_close();
void OfyaKeyboard_handle_poll_event(int kbd_fd);

void OfyaKeyboards_set_key_press_callback(void (*callback)(uint16_t key_code, bool pressed));
