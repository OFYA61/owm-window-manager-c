#include "backend.h"

#include <stdio.h>

#include "linux-platform/display.h"
#include "linux-platform/evdev.h"
#include "linux-platform/render.h"
#include "linux-platform/events.h"

int OWM_initBackend(OWM_Context_type context_type, OWM_Context *out_backend) {
  if (context_type == OWM_BACKEND_TYPE_LINUX) {
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

    out_backend->type = context_type;
    out_backend->aquireFreeFrameBuffer = OWM_drmAquireFreeFrameBuffer;
    out_backend->getDisplayWidth = OWM_drmGetDisplayWidth;
    out_backend->getDisplayHeight = OWM_drmGetDisplayHeight;
    out_backend->swapBuffers = OWM_drmFlipRenderContext;

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
  fprintf(stderr, "Got unexpected backend type\n");
  return 1;
}

void OWM_shutdownBackend(OWM_Context *backend) {
  if (backend->type == OWM_BACKEND_TYPE_LINUX) {
    OWM_cleanupEvents();
    OWM_closeEvDev();
    OWM_drmCloseRenderContext();
    OWM_closeDRMDisplays();
  }
}
