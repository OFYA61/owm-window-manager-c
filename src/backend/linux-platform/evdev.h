#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct {
  int *fds;
  size_t count;
} OWM_EvDevKeyboards;

typedef struct {
  int *fds;
  size_t count;
} OWM_EvDevMice;

int OWM_setupEvDev();
void OWM_closeEvDev();

const OWM_EvDevKeyboards* OWM_getEvDevKeyboards();
const OWM_EvDevMice* OWM_getEvDevMice();
