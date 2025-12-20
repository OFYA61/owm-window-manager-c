#pragma once

#include "display.h"
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

int FrameBuffer_create(struct Display *display, struct FrameBuffer *out);
int FrameBuffer_createList(struct Display *display, struct FrameBuffer *out, size_t count);
void FrameBuffer_destroy(struct Display *display, struct FrameBuffer *fb);
void FrameBuffer_destroyList(struct Display *display, struct FrameBuffer *fb, size_t count);
