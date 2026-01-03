#include "owm.h"

#include "backend/backend.h"
#include "window.h"

OWM_Context backend = { 0 };

int OWM_init(OWM_Context_type context_type) {
  if (OWM_initBackend(context_type, &backend)) {
    return 1;
  }
  return 0;
}

void OWM_shutdown() {
  OWM_cleanupWindows();
  OWM_shutdownBackend(&backend);
}

inline OWM_Context* OWM_getContext() {
  return &backend;
}
