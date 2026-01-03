#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

int OWM_setupEvDev();
void OWM_closeEvDev();
void OWM_pollEvDevEvents();
