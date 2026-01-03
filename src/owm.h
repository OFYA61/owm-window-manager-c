#pragma once

#include "backend/backend.h"

/// Initialize window manager
int OWM_init(OWM_BackendType context_type);
/// Cleanup objects created for the window manager
void OWM_shutdown();

OWM_Backend* OWM_getContext();
