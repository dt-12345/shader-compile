#pragma once

#include <nvnTool/nvnTool_GlslcInterface.h>
#include <heap/seadHeap.h>

void GlslcInitialize();
const GLSLCcompileObject Compile(const char* const* shaderSources, const NVNshaderStage* shaderStages, int shaderCount);

inline sead::Heap* g_Heap = nullptr;

extern "C" __attribute__((visibility("default"))) void* glslc_Alloc(size_t size);
extern "C" __attribute__((visibility("default"))) void glslc_Free(void* address);
extern "C" __attribute__((visibility("default"))) void* glslc_Realloc(void* address, size_t size);

// for glslc_SetAllocators (but also why tf do they make us do this then proceed to not use the fucking allocators we set)
void* Alloc(size_t size, size_t align, void* userData = nullptr);
void Free(void* address, void* userData = nullptr);
void* Realloc(void* address, size_t size, void* userData = nullptr);