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
} PrimaryPlaneProperties;

typedef struct {
  drmModeModeInfo* display_modes;
  size_t count_display_modes;

  uint32_t fd_card;

  uint32_t connector_id;

  uint32_t encoder_id;

  uint32_t crtc_id;
  uint32_t crtc_index;

  uint32_t plane_primary;
  PrimaryPlaneProperties plane_primary_properties;

  uint32_t plane_cursor;
  uint32_t plane_overlay;
} OfyaDisplay;

typedef struct {
  OfyaDisplay* displays;
  size_t count;
} OfyaDisplays;

typedef struct {
  OfyaDisplay* display;
  uint32_t property_blob_id;
  size_t selected_mode_idx;
} OfyaRenderDisplay;

extern OfyaDisplays DISPLAYS;
extern OfyaRenderDisplay RENDER_DISPLAY;

/// Scan the displays and return them in the global `DISPLAYS` array
int OfyaDisplays_scan();
/// Goes over the displays in the the global `DISPLAYS` array and closes them
void OfyaDisplays_close();

/// Queries the user to select a display from the global `DISPLAYS` array and sets it up in the global `RENDER_DISPLAY` variable
int OfyaRenderDisplay_pick();
