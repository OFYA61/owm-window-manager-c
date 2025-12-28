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

const owmKeyboards* owmKeyboards_get();
const owmMice* owmMice_get();
