#include "compile.hpp"
#include "file.hpp"

#include <array>
#include <string>

#include "lib.hpp"
#include "nn.hpp"

static constexpr const char* sShaderExtensions[] = {
    ".vert", ".frag", ".geom", ".tesc", ".tese", ".comp",
};

bool CompileShader(const std::string inputPath, const std::string outputPath, NVNshaderStage stage = NVN_SHADER_STAGE_LARGE) {
    EXL_ASSERT(g_Heap != nullptr);

    if (stage == NVN_SHADER_STAGE_LARGE) {
        for (int i = 0; i < 6; ++i) {
            if (inputPath.ends_with(sShaderExtensions[i])) {
                stage = static_cast<NVNshaderStage>(i);
                break;
            }
        }
    }
    if (stage == NVN_SHADER_STAGE_LARGE) {
        Logging.Log("Failed to determine shader stage for %s", inputPath.c_str());
        return false;
    }

    char* shaderSource = static_cast<char*>(ReadFile(inputPath.c_str(), g_Heap));
    if (shaderSource == nullptr) {
        Logging.Log("Failed to read %s", inputPath.c_str());
        return false;
    }

    const char* sources[] = { shaderSource }; const NVNshaderStage stages[] = { stage };
    const auto compileObject = Compile(sources, stages, 1);
    if (!compileObject.lastCompiledResults->compilationStatus->success) {
        Logging.Log("Failed to compile %s", inputPath.c_str());
        g_Heap->free(shaderSource);
        return false;
    }

    g_Heap->free(shaderSource);
    const auto* binPtr = compileObject.lastCompiledResults->glslcOutput;
    const char* outputData = reinterpret_cast<const char*>(binPtr) + binPtr->headers[0].genericHeader.common.dataOffset;
    
    return WriteFile(outputPath.c_str(), outputData, binPtr->headers[0].genericHeader.common.size);
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
                char inputPath[0x1000];
                const s32 inputPathSize = nn::util::SNPrintf(inputPath, sizeof(inputPath), "sd:/shaders/%s", file.m_Name);
                inputPath[inputPathSize] = '\0';

                char outputPath[0x1000];
                const s32 outputPathSize = nn::util::SNPrintf(outputPath, sizeof(outputPath), "sd:/output/%s.bin", file.m_Name);
                outputPath[outputPathSize] = '\0';

                // check if file already exists
                nn::fs::FileHandle outputHandle{};
                if (nn::fs::OpenFile(&outputHandle, outputPath, nn::fs::OpenMode_Read) == 0) {
                    nn::fs::CloseFile(outputHandle);
                    continue;
                }

                if (CompileShader(inputPath, outputPath)) {
                    Logging.Log("Saved %s to %s", inputPath, outputPath);
                } else {
                    Logging.Log("Failed to compile %s", inputPath);
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