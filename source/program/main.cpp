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
    EXL_ASSERT(inputPath != nullptr && outputPath != nullptr);

    if (stage == NVN_SHADER_STAGE_LARGE) {
        for (s32 i = 0; i < 6; ++i) {
            if (EndsWith(inputPath, sShaderExtensions[i])) {
                stage = static_cast<NVNshaderStage>(i);
                break;
            }
        }
    }

    long fileSize = 0;
    char* shaderSource = static_cast<char*>(ReadFile(inputPath, g_Heap, &fileSize));
    if (shaderSource == nullptr) {
        Logging.Log("Failed to read %s", inputPath);
        return false;
    }

    bool isSpirv;
    u32 moduleSizes[1] = { 0 };
    if (fileSize > 0x14 && *reinterpret_cast<u32*>(shaderSource) == cSpirvMagicNumber) {
        moduleSizes[0] = static_cast<u32>(fileSize);
        isSpirv = true;
    } else {
        isSpirv = false;
    }

    if (stage == NVN_SHADER_STAGE_LARGE && !isSpirv) {
        Logging.Log("Failed to determine shader stage for %s", inputPath);
        return false;
    }

    bool res = false;
    const char* sources[] = { shaderSource }; const NVNshaderStage stages[] = { stage };
    auto compileObject = Compile(sources, stages, 1, isSpirv ? moduleSizes : nullptr);
    if (!compileObject.lastCompiledResults->compilationStatus->success) {
        Logging.Log("Failed to compile %s", inputPath);
    } else {
        const auto* binPtr = compileObject.lastCompiledResults->glslcOutput;
        const char* outputData = reinterpret_cast<const char*>(binPtr) + binPtr->headers[0].genericHeader.common.dataOffset;
        res = WriteFile(outputPath, outputData, binPtr->headers[0].genericHeader.common.size);
    }

    glslcFinalize(&compileObject);
    g_Heap->free(shaderSource);
    return res;
}

bool CompileShader(const char* const* inputPaths, const char* const* outputPaths) {
    EXL_ASSERT(g_Heap != nullptr);
    EXL_ASSERT(inputPaths != nullptr && outputPaths != nullptr);

    char* sources[5] = {};
    NVNshaderStage stages[5] = {};
    const char* outputs[5] = {};
    u32 moduleSizes[5] = {};
    s32 count = 0;
    bool res = false;
    bool isSpirv = true;
    
    for (s32 i = 0; i < 5; ++i) {
        if (inputPaths[i] != nullptr && outputPaths[i] != nullptr) {
            long fileSize = 0;
            char* shaderSource = static_cast<char*>(ReadFile(inputPaths[i], g_Heap, &fileSize));
            if (shaderSource != nullptr) {
                if (fileSize > 0x14 && *reinterpret_cast<u32*>(shaderSource) == cSpirvMagicNumber) {
                    moduleSizes[count] = static_cast<u32>(fileSize);
                } else {
                    isSpirv = false;
                }
                sources[count] = shaderSource;
                outputs[count] = outputPaths[i];
                stages[count++] = static_cast<NVNshaderStage>(i);
            } else {
                Logging.Log("Failed to read file %s", inputPaths[i]);
            }
        }
    }

    Logging.Log("Compiling...");
    auto compileObject = Compile(sources, stages, count, isSpirv ? moduleSizes : nullptr);
    if (!compileObject.lastCompiledResults->compilationStatus->success) {
        Logging.Log("Failed to compile shader");
    } else {
        res = true;
        for (s32 i = 0; i < count; ++i) {
            if (outputs[i] != nullptr) {
                const auto* binPtr = compileObject.lastCompiledResults->glslcOutput;
                const char* outputData = reinterpret_cast<const char*>(binPtr) + binPtr->headers[i].genericHeader.common.dataOffset;
                res = WriteFile(outputs[i], outputData, binPtr->headers[i].genericHeader.common.size) && res;
            }
        }
    }

    glslcFinalize(&compileObject);
    for (s32 i = 0; i < 5; ++i) {
        if (sources[i] != nullptr) {
            g_Heap->free(sources[i]);
        }
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

                    const char* extensions[5] = { "vert", "frag", "geom", "tesc", "tese", };
                    for (s32 j = 0; j < 5; ++j) {
                        const s32 inputPathSize = nn::util::SNPrintf(inputPaths[j], sizeof(inputPaths[j]), "sd:/shaders/%s.%s", basePath, extensions[j]);
                        inputPaths[j][inputPathSize] = '\0';
                        const s32 outputPathSize = nn::util::SNPrintf(outputPaths[j], sizeof(outputPaths[j]), "sd:/output/%s.%s.bin", basePath, extensions[j]);
                        outputPaths[j][outputPathSize] = '\0';
                    }

                    const char* inputs[] = {
                        inputPaths[0], inputPaths[1], inputPaths[2], inputPaths[3], inputPaths[4],
                    };
                    const char* outputs[] = {
                        outputPaths[0], outputPaths[1], outputPaths[2], outputPaths[3], outputPaths[4],
                    };
                    bool allExists = true;
                    for (s32 j = 0; j < 5; ++j) {
                        nn::fs::FileHandle inputHandle{};
                        // if this input file doesn't exist, ignore
                        if (nn::fs::OpenFile(&inputHandle, inputs[j], nn::fs::OpenMode_Read) == 0) {
                            nn::fs::CloseFile(inputHandle);
                        } else {
                            inputs[j] = outputs[j] = nullptr;
                            continue;
                        }
                        nn::fs::FileHandle outputHandle{};
                        if (nn::fs::OpenFile(&outputHandle, outputs[j], nn::fs::OpenMode_Read) == 0) {
                            nn::fs::CloseFile(outputHandle);
                        } else {
                            allExists = false;
                        }
                    }

                    if (allExists) {
                        continue;
                    }

                    Logging.Log("Compiling %s", file.m_Name);
                    if (CompileShader(inputs, outputs)) {
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
            nn::os::SleepThread(nn::TimeSpan::FromSeconds(1));
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