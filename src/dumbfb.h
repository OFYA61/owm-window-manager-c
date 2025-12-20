#pragma once

#include <stddef.h>
#include <stdint.h>

struct DumbFB {
  uint32_t fb_id;
  uint32_t handle;
  // Number of bytes in a row
  uint32_t pitch; 
  size_t size;
  void *map;
};

int DumbFB_create(int fd_card, uint32_t width, uint32_t height, struct DumbFB *out);
void DumbFB_destroy(int fd_card, struct DumbFB* fb);
