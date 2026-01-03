#pragma once

#include "backend/backend.h"

/// Initialize window manager
int OWM_init(OWM_Context_type context_type);
/// Cleanup objects created for the window manager
void OWM_shutdown();

OWM_Context* OWM_getActiveBackend();
