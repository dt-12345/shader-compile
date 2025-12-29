#include "compile.hpp"

#include "lib.hpp"
#include "loggers.hpp"

void* Alloc(size_t size, size_t align, void* userData) {
    EXL_ASSERT(g_Heap != nullptr);
    return g_Heap->tryAlloc(size, align);
}

void Free(void* address, void* userData) {
    EXL_ASSERT(g_Heap != nullptr);
    g_Heap->free(address);
}

void* Realloc(void* address, size_t size, void* userData) {
    EXL_ASSERT(g_Heap != nullptr);
    return g_Heap->tryRealloc(address, size, 8);
}

void GlslcInitialize() {
    glslcSetAllocator(Alloc, Free, Realloc, nullptr);
}

GLSLCcompileObject Compile(const char* const* shaderSources, const NVNshaderStage* shaderStages, int shaderCount, const u32* moduleSizes) {
    EXL_ASSERT(shaderCount > 0);
    EXL_ASSERT(shaderSources != nullptr && shaderStages != nullptr);

    GLSLCcompileObject compileObject{};

    if (!glslcInitialize(&compileObject)) {
        Logging.Log("glslcInitialize failed!");
        return compileObject;
    }

    // not sure which of these are truly necessary, but this is what nn::gfx does and it seems to work
    compileObject.options.optionFlags.outputGpuBinaries = 1;
    compileObject.options.optionFlags.outputShaderReflection = 1;
    compileObject.options.optionFlags.outputDebugInfo = GLSLC_DEBUG_LEVEL_NONE;
    compileObject.options.optionFlags.spillControl = DEFAULT_SPILL;
    compileObject.options.optionFlags.outputThinGpuBinaries = 1;
    compileObject.options.includeInfo.numPaths = 0;
    compileObject.options.xfbVaryingInfo.numVaryings = 0;
    compileObject.options.xfbVaryingInfo.varyings = nullptr;
    compileObject.options.forceIncludeStdHeader = nullptr;
    compileObject.options.includeInfo.paths = nullptr;

    if (moduleSizes != nullptr) {
        compileObject.options.optionFlags.language = GLSLC_LANGUAGE_SPIRV;
        compileObject.input.spirvModuleSizes = moduleSizes;
        compileObject.input.spirvEntryPointNames = nullptr;
        compileObject.input.spirvSpecInfo = nullptr;
    }

    compileObject.input.sources = shaderSources;
    compileObject.input.stages = shaderStages;
    compileObject.input.count = shaderCount;

    if (!glslcCompile(&compileObject) || compileObject.lastCompiledResults->compilationStatus->allocError) {
        if (compileObject.lastCompiledResults->compilationStatus->allocError) {
            Logging.Log("glslcCompile failed with an allocation error!");
        } else {
            Logging.Log("glslcCompile failed!");
        }
        Logging.Log(compileObject.lastCompiledResults->compilationStatus->infoLog);
        return compileObject;
    }

    Logging.Log("Compilation successful");
    return compileObject;
}