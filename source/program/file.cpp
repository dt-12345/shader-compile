#include "file.hpp"
#include "loggers.hpp"

#define ASSERT_RETURN(result, ret)                  \
    if (result != 0) {                              \
        Logging.Log("Error result for " #result);   \
        return ret;                                 \
    }

void* ReadFile(const char* path, sead::Heap* heap) {
    nn::fs::FileHandle handle{};
    ASSERT_RETURN(nn::fs::OpenFile(&handle, path, nn::fs::OpenMode_Read), nullptr)

    long fileSize = 0;
    ASSERT_RETURN(nn::fs::GetFileSize(&fileSize, handle), nullptr)

    // assuming these are text files so we add a null terminator
    char* buffer = static_cast<char*>(heap->tryAlloc(fileSize + 1, 8));
    if (buffer == nullptr) {
        return nullptr;
    }

    if (nn::fs::ReadFile(handle, 0, buffer, fileSize)) {
        Logging.Log("Failed to read file %s", path);
        heap->free(buffer);
        return nullptr;
    }

    nn::fs::CloseFile(handle);

    buffer[fileSize] = '\0';
    return buffer;
}

bool WriteFile(const char* path, const void* data, size_t size) {
    nn::fs::FileHandle handle{};
    if (nn::fs::OpenFile(&handle, path, nn::fs::OpenMode_Write)) {
        ASSERT_RETURN(nn::fs::CreateFile(path, 0), false)
        ASSERT_RETURN(nn::fs::OpenFile(&handle, path, nn::fs::OpenMode_Write), false)
    }

    ASSERT_RETURN(nn::fs::SetFileSize(handle, size), false);
    nn::fs::WriteOption option = nn::fs::WriteOption::CreateOption(nn::fs::WriteOptionFlag_Flush);
    ASSERT_RETURN(nn::fs::WriteFile(handle, 0, data, size, option), false)

    nn::fs::CloseFile(handle);

    return true;
}