#pragma once

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

struct Display {
  drmModeModeInfo displayMode;

  uint32_t fd_card;

  uint32_t connector_id;

  uint32_t encoder_id;

  uint32_t crtc_id;
  uint32_t crtc_index;

  uint32_t plane_primary;
  uint32_t plane_cursor;
  uint32_t plane_overlay;

  uint32_t mode_blob_id;
};

/// Pick a display
struct Display Display_pick();
/// Close/cleanup the display
void Display_close(struct Display *display);
