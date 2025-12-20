#include "fb.h"

#include "dumbfb.h"

int FrameBuffer_create(struct Display *display, struct FrameBuffer *out) {
  if (DumbFB_create(display, &out->buffer)) {
    return 1;
  }
  out->state = FB_FREE;
  return 0;
}

int FrameBuffer_createList(struct Display *display, struct FrameBuffer *out, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    if (FrameBuffer_create(display, &out[i])) {
      for (size_t j = 0; j < i; ++j) {
        FrameBuffer_destroy(display, &out[j]);
      }
      return 1;
    }
  }
  return 0;
}

void FrameBuffer_destroy(struct Display *display, struct FrameBuffer *fb) {
  DumbFB_destroy(display, &fb->buffer);
}

void FrameBuffer_destroyList(struct Display *display, struct FrameBuffer *fb, size_t count) {
  for (size_t i = 0; i < count; ++i) {
    FrameBuffer_destroy(display, &fb[i]);
  }
}
