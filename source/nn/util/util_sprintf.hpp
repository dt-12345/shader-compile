#pragma once

#include "types.h"

#include <cstdarg>

namespace nn::util {

    u32 SNPrintf(char* buffer, size_t bufferSize, const char* fmt, ...);
    u32 VSNPrintf(char* buffer, size_t bufferSize, const char* fmt, va_list args);

}