#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace SaveFile {

// The destination remains intact if serialization or replacement fails.
bool replace(const std::string& path);

template <class Writer>
bool write(const std::string& path, std::ios::openmode mode, Writer writer) {
    std::ofstream output(path + ".tmp", mode | std::ios::trunc);
    if (!output)
        return false;
    const bool written = writer(output);
    output.flush();
    const bool complete = written && output.good();
    output.close();
    return complete && replace(path);
}

} // namespace SaveFile
