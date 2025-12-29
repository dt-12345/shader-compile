#pragma once

#include <heap/seadHeap.h>

#include "nn.hpp"

void* ReadFile(const char* path, sead::Heap* heap);
bool WriteFile(const char* path, const void* data, size_t size);