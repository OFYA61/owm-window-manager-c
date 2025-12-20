#include "fb.h"

#include "dumbfb.h"

int FrameBuffer_create(int fd_card, uint32_t width, uint32_t height, struct FrameBuffer *out) {
  if (DumbFB_create(fd_card, width, height, &out->buffer)) {
    return 1;
  }
  out->state = FB_FREE;
  return 0;
}

void FrameBuffer_destroy(int fd_card, struct FrameBuffer *fb) {
  DumbFB_destroy(fd_card, &fb->buffer);
}
