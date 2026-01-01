#include "owm.h"

#include <stdio.h>

#include "display.h"
#include "events.h"
#include "input.h"
#include "render.h"
#include "window.h"

int owm_init() {
  if (owmInput_setup()) {
    fprintf(stderr, "Failed to find a keyboard\n");
    return 1;
  }

  if (owmDisplays_scan()) {
    perror("owmDisplay_scan");
    return 1;
  }

  if (owmRenderContext_init()) {
    owmDisplays_close();
    return 1;
  }

  owmEvents_setup();

  return 0;
}

void owm_cleanup() {
  owmWindows_cleanup();
  owmEvents_cleanup();
  owmInput_close();
  owmRenderContext_close();
  owmDisplays_close();
}
