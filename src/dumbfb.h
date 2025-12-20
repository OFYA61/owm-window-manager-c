#pragma once

#include <stddef.h>
#include <stdint.h>

#include "display.h"

struct DumbFB {
  uint32_t fb_id;
  uint32_t handle;
  // Number of bytes in a row
  uint32_t pitch; 
  size_t size;
  void *map;
};

int DumbFB_create(struct Display *display, struct DumbFB *out);
void DumbFB_destroy(struct Display *disppay, struct DumbFB* fb);
