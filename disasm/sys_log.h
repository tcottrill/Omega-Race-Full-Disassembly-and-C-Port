#pragma once
// Shim for the AAE z80 core: route AAE logging macros to this rig's logger.
#include "log.h"

#define LOG_INFO  wrlog
#define LOG_ERROR wrlog
#define LOG_DEBUG wrlog
