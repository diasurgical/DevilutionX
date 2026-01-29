#pragma once

#include_next <errno.h>

#define ESHUTDOWN (__ELASTERROR + 1)
