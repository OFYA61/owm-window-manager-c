#include "owm.h"

#include "display.h"
#include "events.h"
#include "input.h"
#include "render.h"
#include "window.h"

int owm_init() {
  if (owmInput_setup()) {
    goto owm_init_failure_input_setup;
  }

  if (owmDisplays_scan()) {
    goto owm_init_failure_display_scan;
  }

  if (owmRenderContext_init()) {
    goto owm_init_failure_render_context_init;
  }

  if (owmEvents_setup()) {
    goto owm_init_failure_events_setup;
  }

  return 0;

owm_init_failure_events_setup:
  owmRenderContext_close();
owm_init_failure_render_context_init:
  owmDisplays_close();
owm_init_failure_display_scan:
  owmInput_close();
owm_init_failure_input_setup:
  return 1;
}

void owm_cleanup() {
  owmWindows_cleanup();
  owmEvents_cleanup();
  owmInput_close();
  owmRenderContext_close();
  owmDisplays_close();
}
