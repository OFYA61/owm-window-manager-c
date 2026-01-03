#include "backend.h"

#include <stdio.h>

#include "linux-platform/display.h"
#include "linux-platform/evdev.h"
#include "linux-platform/render.h"

int OWM_initBackend(OWM_BackendType type, OWM_Backend *out_backend) {
  if (type == OWM_BACKEND_TYPE_LINUX) {
    if (OWM_scanDRMDisplays()) {
      goto owm_init_failure_display_scan;
    }

    if (OWM_drmInitRenderContext()) {
      goto owm_init_failure_render_context_init;
    }

    if (OWM_setupEvDev()) {
      goto owm_init_failure_input_setup;
    }

    out_backend->type = type;
    out_backend->aquireFreeFrameBuffer = OWM_drmAquireFreeFrameBuffer;
    out_backend->getDisplayWidth = OWM_drmGetDisplayWidth;
    out_backend->getDisplayHeight = OWM_drmGetDisplayHeight;
    out_backend->swapBuffers = OWM_drmFlipRenderContext;
    out_backend->dispatch = OWM_pollEvDevEvents;

    return 0;

  owm_init_failure_input_setup:
    OWM_drmShutdownRenderContext();
  owm_init_failure_render_context_init:
    OWM_closeDRMDisplays();
  owm_init_failure_display_scan:
    return 1;
  }
  fprintf(stderr, "Got unexpected backend type\n");
  return 1;
}

void OWM_shutdownBackend(OWM_Backend *backend) {
  if (backend->type == OWM_BACKEND_TYPE_LINUX) {
    OWM_closeEvDev();
    OWM_drmShutdownRenderContext();
    OWM_closeDRMDisplays();
  }
}
