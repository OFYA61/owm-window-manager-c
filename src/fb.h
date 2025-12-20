#pragma once

#include "dumbfb.h"

enum FBState {
  FB_FREE,
  FB_QUEUED,
  FB_DISPLAYED
};

struct FrameBuffer {
  struct DumbFB buffer;
  enum FBState state;
};

int FrameBuffer_create(int fd_card, uint32_t width, uint32_t height, struct FrameBuffer *out);
void FrameBuffer_destroy(int fd_card, struct FrameBuffer *fb);
