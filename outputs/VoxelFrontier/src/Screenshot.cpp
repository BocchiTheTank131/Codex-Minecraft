#include "Screenshot.h"

#include <glad/glad.h>

#include <chrono>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace Screenshot {

bool saveBmp(int width, int height, const std::string& directory) {
    if (width <= 0 || height <= 0)
        return false;
    namespace fs = std::filesystem;
    std::error_code error;
    fs::create_directories(directory, error);

    const auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    const fs::path path = fs::path(directory) / ("voxel_" + std::to_string(timestamp) + ".bmp");

    const int rowBytes = width * 3;
    const int rowStride = (rowBytes + 3) & ~3;
    std::vector<unsigned char> packedPixels(static_cast<std::size_t>(rowBytes) * height);
    std::vector<unsigned char> pixels(static_cast<std::size_t>(rowStride) * height, 0);
    GLint previousAlignment = 4;
    glGetIntegerv(GL_PACK_ALIGNMENT, &previousAlignment);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_BGR, GL_UNSIGNED_BYTE, packedPixels.data());
    glPixelStorei(GL_PACK_ALIGNMENT, previousAlignment);
    for (int y = 0; y < height; ++y)
        std::memcpy(pixels.data() + static_cast<std::size_t>(y) * rowStride,
                    packedPixels.data() + static_cast<std::size_t>(y) * rowBytes,
                    static_cast<std::size_t>(rowBytes));

    std::ofstream output(path, std::ios::binary);
    if (!output) {
        return false;
    }

    const auto write16 = [&](std::uint16_t value) {
        output.put(static_cast<char>(value));
        output.put(static_cast<char>(value >> 8U));
    };
    const auto write32 = [&](std::uint32_t value) {
        write16(static_cast<std::uint16_t>(value));
        write16(static_cast<std::uint16_t>(value >> 16U));
    };

    output.put('B');
    output.put('M');
    write32(54U + static_cast<std::uint32_t>(rowStride * height));
    write16(0);
    write16(0);
    write32(54);
    write32(40);
    write32(static_cast<std::uint32_t>(width));
    write32(static_cast<std::uint32_t>(height));
    write16(1);
    write16(24);
    write32(0);
    write32(static_cast<std::uint32_t>(rowStride * height));
    write32(2835);
    write32(2835);
    write32(0);
    write32(0);
    output.write(reinterpret_cast<const char*>(pixels.data()),
                 static_cast<std::streamsize>(pixels.size()));

    std::cout << "Screenshot saved: " << path.string() << '\n';
    return static_cast<bool>(output);
}

} // namespace Screenshot
