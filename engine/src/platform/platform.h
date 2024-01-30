#pragma once

#include "defines.h"

b8 platform_initialize(void);

void platform_shutdown(void);

f64 platform_get_time(void);

b8 platform_init_ANSI_escape(void);