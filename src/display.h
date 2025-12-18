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
};

struct SystemGraphicsCards{
  char **cards;
  size_t count;
};

struct DiscoveryDisplayMode {
  uint16_t h;
  uint16_t v;
  uint32_t hz;
};

struct DiscoveryDisplay {
  uint32_t connector_id;
  uint32_t encoder_id;
  uint32_t crtc_id;
  drmModeModeInfo* displayModes;
  uint32_t displayModesCount;
  char cardPath[64];
};

struct DiscoveryDisplays {
  struct DiscoveryDisplay* results;
  size_t count;
};

/// Pick a display
struct Display Display_pick();
// Close/cleanup the display
void Display_close(struct Display *display);
