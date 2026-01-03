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
} OWM_DRMPrimaryPlaneProperties;

typedef struct {
  drmModeModeInfo* display_modes;
  size_t count_display_modes;

  uint32_t fd_card;

  uint32_t connector_id;

  uint32_t encoder_id;

  uint32_t crtc_id;
  uint32_t crtc_index;

  uint32_t plane_primary;
  OWM_DRMPrimaryPlaneProperties plane_primary_properties;

  uint32_t plane_cursor;
  uint32_t plane_overlay;
} OWM_DRMDisplay;

typedef struct {
  OWM_DRMDisplay* displays;
  size_t count;
} OWM_DRMDisplays;

/// Discovers the available displays from the DRM
int OWM_scanDRMDisplays();
/// Closes open file descriptors and cleans up allocated memory
void OWM_closeDRMDisplays();

/// Returns the list of DRM displays
const OWM_DRMDisplays* OWM_getDRMDisplays();
