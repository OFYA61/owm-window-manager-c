#include "owm.h"

#include "backend/backend.h"
#include "core/window.h"

OWM_Backend backend = { 0 };

int OWM_init(OWM_BackendType context_type) {
  if (OWM_initBackend(context_type, &backend)) {
    return 1;
  }
  return 0;
}

void OWM_shutdown() {
  OWM_cleanupWindows();
  OWM_shutdownBackend(&backend);
}

inline OWM_Backend* OWM_getContext() {
  return &backend;
}
