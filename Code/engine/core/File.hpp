#pragma once

#include "Types.hpp"
#include <optional>
#include <filesystem>

namespace crf {

class File {
public:
    static std::optional<Vec<byte>> readBinary(std::string_view path);
    static std::optional<std::string> readText(std::string_view path);

    static bool writeBinary(std::string_view path, View<const byte> data);
    static bool writeText(std::string_view path, std::string_view text);

    static bool exists(std::string_view path);

    static std::string stem(std::string_view path);
    static std::string extension(std::string_view path);
    static std::string parent(std::string_view path);
    static std::string join(std::string_view a, std::string_view b);
};

} // namespace crf
