#include "SaveFile.h"

#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#endif

namespace SaveFile {

bool replace(const std::string& path) {
    const std::filesystem::path destination(path);
    const std::filesystem::path temporary(path + ".tmp");
#ifdef _WIN32
    const std::wstring target = destination.wstring();
    const std::wstring source = temporary.wstring();
    if (std::filesystem::exists(destination))
        return ReplaceFileW(target.c_str(), source.c_str(), nullptr, 0, nullptr, nullptr) != 0;
    return MoveFileExW(source.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
#else
    std::error_code error;
    std::filesystem::rename(temporary, destination, error);
    return !error;
#endif
}

} // namespace SaveFile
