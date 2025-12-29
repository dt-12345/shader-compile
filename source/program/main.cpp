#include "compile.hpp"
#include "file.hpp"

#include "lib.hpp"
#include "nn.hpp"

static constexpr const char* sShaderExtensions[] = {
    ".vert", ".frag", ".geom", ".tesc", ".tese", ".comp",
};

bool EndsWith(const char* str, const char* suffix) {
    if (str == nullptr || suffix == nullptr) {
        return false;
    }

    const size_t strSize = strnlen(str, nn::fs::MaxDirectoryEntryNameSize + 1);
    const size_t suffixSize = strnlen(suffix, nn::fs::MaxDirectoryEntryNameSize + 1);
    if (suffixSize > strSize) {
        return false;
    }

    return strncmp(str + strSize - suffixSize, suffix, suffixSize) == 0;
}

bool CompileShader(const char* inputPath, const char* outputPath, NVNshaderStage stage = NVN_SHADER_STAGE_LARGE) {
    EXL_ASSERT(g_Heap != nullptr);

    if (stage == NVN_SHADER_STAGE_LARGE) {
        for (int i = 0; i < 6; ++i) {
            if (EndsWith(inputPath, sShaderExtensions[i])) {
                stage = static_cast<NVNshaderStage>(i);
                break;
            }
        }
    }
    if (stage == NVN_SHADER_STAGE_LARGE) {
        Logging.Log("Failed to determine shader stage for %s", inputPath);
        return false;
    }

    char* shaderSource = static_cast<char*>(ReadFile(inputPath, g_Heap));
    if (shaderSource == nullptr) {
        Logging.Log("Failed to read %s", inputPath);
        return false;
    }

    bool res = false;
    const char* sources[] = { shaderSource }; const NVNshaderStage stages[] = { stage };
    auto compileObject = Compile(sources, stages, 1);
    if (!compileObject.lastCompiledResults->compilationStatus->success) {
        Logging.Log("Failed to compile %s", inputPath);
    } else {
        const auto* binPtr = compileObject.lastCompiledResults->glslcOutput;
        const char* outputData = reinterpret_cast<const char*>(binPtr) + binPtr->headers[0].genericHeader.common.dataOffset;
        res = WriteFile(outputPath, outputData, binPtr->headers[0].genericHeader.common.size);
    }

    g_Heap->free(shaderSource);
    glslcFinalize(&compileObject);
    return res;
}

struct ShaderSet {
    const char* vertexPath;
    const char* tessControlPath;
    const char* tessEvalPath;
    const char* geometryPath;
    const char* fragmentPath;
};

bool CompileShader(const ShaderSet* inputPaths, const ShaderSet* outputPaths) {
    EXL_ASSERT(g_Heap != nullptr);
    EXL_ASSERT(inputPaths != nullptr && outputPaths != nullptr);

    char* sources[5] = {};
    NVNshaderStage stages[5] = {};
    const char* outputs[5] = {};
    int count = 0;
    bool res = false;
    
    if (inputPaths->vertexPath && outputPaths->vertexPath) {
        char* shaderSource = static_cast<char*>(ReadFile(inputPaths->vertexPath, g_Heap));
        if (shaderSource != nullptr) {
            sources[count] = shaderSource;
            outputs[count] = outputPaths->vertexPath;
            stages[count++] = NVN_SHADER_STAGE_VERTEX;
        } else {
            Logging.Log("Failed to read file %s", inputPaths->vertexPath);
        }
    }
    if (inputPaths->tessControlPath && outputPaths->tessControlPath) {
        char* shaderSource = static_cast<char*>(ReadFile(inputPaths->tessControlPath, g_Heap));
        if (shaderSource != nullptr) {
            sources[count] = shaderSource;
            outputs[count] = outputPaths->tessControlPath;
            stages[count++] = NVN_SHADER_STAGE_TESS_CONTROL;
        } else {
            Logging.Log("Failed to read file %s", inputPaths->tessControlPath);
        }
    }
    if (inputPaths->tessEvalPath && outputPaths->tessEvalPath) {
        char* shaderSource = static_cast<char*>(ReadFile(inputPaths->tessEvalPath, g_Heap));
        if (shaderSource != nullptr) {
            sources[count] = shaderSource;
            outputs[count] = outputPaths->tessEvalPath;
            stages[count++] = NVN_SHADER_STAGE_TESS_EVALUATION;
        } else {
            Logging.Log("Failed to read file %s", inputPaths->tessEvalPath);
        }
    }
    if (inputPaths->geometryPath && outputPaths->geometryPath) {
        char* shaderSource = static_cast<char*>(ReadFile(inputPaths->geometryPath, g_Heap));
        if (shaderSource != nullptr) {
            sources[count] = shaderSource;
            outputs[count] = outputPaths->geometryPath;
            stages[count++] = NVN_SHADER_STAGE_GEOMETRY;
        } else {
            Logging.Log("Failed to read file %s", inputPaths->geometryPath);
        }
    }
    if (inputPaths->fragmentPath && outputPaths->fragmentPath) {
        char* shaderSource = static_cast<char*>(ReadFile(inputPaths->fragmentPath, g_Heap));
        if (shaderSource != nullptr) {
            sources[count] = shaderSource;
            outputs[count] = outputPaths->fragmentPath;
            stages[count++] = NVN_SHADER_STAGE_FRAGMENT;
        } else {
            Logging.Log("Failed to read file %s", inputPaths->fragmentPath);
        }
    }

    Logging.Log("Compiling...");
    auto compileObject = Compile(sources, stages, count);
    if (!compileObject.lastCompiledResults->compilationStatus->success) {
        Logging.Log("Failed to compile shader");
    } else {
        res = true;
        for (int i = 0; i < count; ++i) {
            if (outputs[i] != nullptr) {
                const auto* binPtr = compileObject.lastCompiledResults->glslcOutput;
                const char* outputData = reinterpret_cast<const char*>(binPtr) + binPtr->headers[i].genericHeader.common.dataOffset;
                res = WriteFile(outputs[i], outputData, binPtr->headers[i].genericHeader.common.size) && res;
            }
        }
    }

    glslc_Free(&compileObject);
    if (sources[0] != nullptr) {
        g_Heap->free(sources[0]);
    }
    if (sources[1] != nullptr) {
        g_Heap->free(sources[1]);
    }
    if (sources[2] != nullptr) {
        g_Heap->free(sources[2]);
    }
    if (sources[3] != nullptr) {
        g_Heap->free(sources[3]);
    }
    if (sources[4] != nullptr) {
        g_Heap->free(sources[4]);
    }

    return res;
}

HOOK_DEFINE_REPLACE(OperatorNewReplacement) {
    static void* Callback(size_t size) {
        EXL_ASSERT(g_Heap != nullptr);
        return g_Heap->tryAlloc(size, 0x10);
    }
};

HOOK_DEFINE_INLINE(AppMain) {
    static void Callback(exl::hook::InlineCtx* ctx) {
        // steal application root heap (we're blocking the entire program anyways so it doesn't matter)
        g_Heap = reinterpret_cast<sead::Heap*>(ctx->X[19]);
        OperatorNewReplacement::InstallAtOffset(0x01062ce0);
        GlslcInitialize();

        EXL_ABORT_UNLESS(nn::fs::MountSdCard("sd") == 0);
        nn::fs::CreateDirectory("sd:/output");

        while (true) {
            nn::fs::DirectoryHandle handle{};
            if (nn::fs::OpenDirectory(&handle, "sd:/shaders", nn::fs::OpenDirectoryMode::OpenDirectoryMode_File)) {
                continue;
            }

            long entryCount = 0;
            nn::fs::GetDirectoryEntryCount(&entryCount, handle);

            long readCount = 0;
            for (long long i = 0; i < entryCount; ++i) {
                nn::fs::DirectoryEntry file{};
                nn::fs::ReadDirectory(&readCount, &file, handle, 1);

                if (EndsWith(file.m_Name, ".vert") ||
                    EndsWith(file.m_Name, ".tesc") ||
                    EndsWith(file.m_Name, ".tese") ||
                    EndsWith(file.m_Name, ".geom") ||
                    EndsWith(file.m_Name, ".frag")) {
                    char basePath[nn::fs::MaxDirectoryEntryNameSize + 1];
                    const s32 basePathSize = strnlen(file.m_Name, nn::fs::MaxDirectoryEntryNameSize + 1);
                    strncpy(basePath, file.m_Name, basePathSize - 5);
                    basePath[basePathSize - 5] = '\0';

                    char inputPaths[5][nn::fs::MaxDirectoryEntryNameSize + 1] = {};
                    char outputPaths[5][nn::fs::MaxDirectoryEntryNameSize + 1] = {};

                    const char* extensions[5] = { "vert", "tesc", "tese", "geom", "frag", };
                    for (s32 j = 0; j < 5; ++j) {
                        const s32 inputPathSize = nn::util::SNPrintf(inputPaths[j], sizeof(inputPaths[j]), "sd:/shaders/%s.%s", basePath, extensions[j]);
                        inputPaths[j][inputPathSize] = '\0';
                        const s32 outputPathSize = nn::util::SNPrintf(outputPaths[j], sizeof(outputPaths[j]), "sd:/output/%s.%s.bin", basePath, extensions[j]);
                        outputPaths[j][outputPathSize] = '\0';
                    }

                    ShaderSet inputs = {
                        inputPaths[0], inputPaths[1], inputPaths[2], inputPaths[3], inputPaths[4],
                    };
                    ShaderSet outputs = {
                        outputPaths[0], outputPaths[1], outputPaths[2], outputPaths[3], outputPaths[4],
                    };

                    bool allExists = true;
                    for (s32 j = 0; j < 5; ++j) {
                        nn::fs::FileHandle inputHandle{};
                        // if this input file doesn't exist, ignore
                        if (nn::fs::OpenFile(&inputHandle, inputPaths[j], nn::fs::OpenMode_Read) == 0) {
                            nn::fs::CloseFile(inputHandle);
                        } else {
                            continue;
                        }
                        nn::fs::FileHandle outputHandle{};
                        if (nn::fs::OpenFile(&outputHandle, outputPaths[j], nn::fs::OpenMode_Read) == 0) {
                            nn::fs::CloseFile(outputHandle);
                        } else {
                            allExists = false;
                        }
                    }

                    if (allExists) {
                        continue;
                    }

                    Logging.Log("Compiling %s", file.m_Name);
                    if (CompileShader(&inputs, &outputs)) {
                        Logging.Log("Saved %s and related shaders", file.m_Name);
                    } else {
                        Logging.Log("Failed to compile %s and related shaders", file.m_Name);
                    }
                } else {
                    char inputPath[nn::fs::MaxDirectoryEntryNameSize + 1];
                    const s32 inputPathSize = nn::util::SNPrintf(inputPath, sizeof(inputPath), "sd:/shaders/%s", file.m_Name);
                    inputPath[inputPathSize] = '\0';

                    char outputPath[nn::fs::MaxDirectoryEntryNameSize + 1];
                    const s32 outputPathSize = nn::util::SNPrintf(outputPath, sizeof(outputPath), "sd:/output/%s.bin", file.m_Name);
                    outputPath[outputPathSize] = '\0';

                    // check if file already exists
                    nn::fs::FileHandle outputHandle{};
                    if (nn::fs::OpenFile(&outputHandle, outputPath, nn::fs::OpenMode_Read) == 0) {
                        nn::fs::CloseFile(outputHandle);
                        continue;
                    }

                    Logging.Log("Compiling %s", file.m_Name);
                    if (CompileShader(inputPath, outputPath)) {
                        Logging.Log("Saved %s to %s", inputPath, outputPath);
                    } else {
                        Logging.Log("Failed to compile %s", inputPath);
                    }
                }
            }

            nn::fs::CloseDirectory(handle);
        }
    }
};

extern "C" void exl_main(void* x0, void* x1) {
    /* Setup hooking environment. */
    exl::hook::Initialize();

    AppMain::InstallAtOffset(0x00e7a9b0);
}

extern "C" NORETURN void exl_exception_entry() {
    /* Note: this is only applicable in the context of applets/sysmodules. */
    EXL_ABORT("Default exception handler called!");
}