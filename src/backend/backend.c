#include "backend.h"

#include <stdio.h>

#include "linux/linux_backend.h"
#include "wayland/wayland_backend.h"

int OWM_initBackend(OWM_BackendType type, OWM_Backend *out_backend) {
  if (type == OWM_BACKEND_TYPE_LINUX) {
    return OWM_initLinuxBackend(out_backend);
  } else if (type == OWM_BACKEND_TYPE_WAYLAND) {
    return OWM_initWaylandBackend(out_backend);
  }
  fprintf(stderr, "Got unexpected backend type\n");
  return 1;
}

void OWM_shutdownBackend(OWM_Backend *backend) {
  if (backend->type == OWM_BACKEND_TYPE_LINUX) {
    OWM_shutdownLinuxBackend();
  } else if (backend->type == OWM_BACKEND_TYPE_WAYLAND) {
    OWM_shutdownWaylandBackend();
  }
}
