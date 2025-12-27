#pragma once

#include <stddef.h>
#include <stdint.h>
#include <xf86drmMode.h>

typedef struct {
  uint32_t connector_crtc_id;

  uint32_t crtc_activate;
  uint32_t crtc_mode_id;

  uint32_t plane_fb_id;

  uint32_t plane_crtc_id;
  uint32_t plane_crtc_x;
  uint32_t plane_crtc_y;
  uint32_t plane_crtc_w;
  uint32_t plane_crtc_h;

  uint32_t plane_src_x;
  uint32_t plane_src_y;
  uint32_t plane_src_w;
  uint32_t plane_src_h;
} owmPrimaryPlaneProperties;

typedef struct {
  drmModeModeInfo* display_modes;
  size_t count_display_modes;

  uint32_t fd_card;

  uint32_t connector_id;

  uint32_t encoder_id;

  uint32_t crtc_id;
  uint32_t crtc_index;

  uint32_t plane_primary;
  owmPrimaryPlaneProperties plane_primary_properties;

  uint32_t plane_cursor;
  uint32_t plane_overlay;
} owmDisplay;

typedef struct {
  owmDisplay* displays;
  size_t count;
} owmDisplays;

typedef struct {
  owmDisplay* display;
  uint32_t property_blob_id;
  size_t selected_mode_idx;
} owmRenderDisplay;

extern owmDisplays OWM_DISPLAYS;
extern owmRenderDisplay OWM_RENDER_DISPLAY;

/// Scan the displays and return them in the global `OWM_DISPLAYS` array
int owmDisplays_scan();
/// Goes over the displays in the the global `OWM_DISPLAYS` array and closes them
void owmDisplays_close();

/// Queries the user to select a display from the global `OWM_DISPLAYS` array and sets it up in the global `OWM_RENDER_DISPLAY` variable
int owmRenderDisplay_pick();
