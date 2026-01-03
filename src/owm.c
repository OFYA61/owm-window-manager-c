#include "owm.h"

#include "display.h"
#include "events.h"
#include "input.h"
#include "render.h"
#include "window.h"

int OWM_init() {
  if (OWM_setupEvDev()) {
    goto owm_init_failure_input_setup;
  }

  if (OWM_scanDRMDisplays()) {
    goto owm_init_failure_display_scan;
  }

  if (OWM_drmInitRenderContext()) {
    goto owm_init_failure_render_context_init;
  }

  if (OWM_setupEvents()) {
    goto owm_init_failure_events_setup;
  }

  return 0;

owm_init_failure_events_setup:
  OWM_drmCloseRenderContext();
owm_init_failure_render_context_init:
  OWM_closeDRMDisplays();
owm_init_failure_display_scan:
  OWM_closeEvDev();
owm_init_failure_input_setup:
  return 1;
}

void OWM_shutdown() {
  OWM_cleanupWindows();
  OWM_cleanupEvents();
  OWM_closeEvDev();
  OWM_drmCloseRenderContext();
  OWM_closeDRMDisplays();
}
