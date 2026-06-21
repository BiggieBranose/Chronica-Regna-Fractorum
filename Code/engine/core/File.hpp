#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <filesystem>
#include <span>

namespace crf {

class File {
public:
    static std::optional<std::string>  readText(std::string_view path);
    static std::optional<std::vector<uint8_t>> readBinary(std::string_view path);
    static bool writeText(std::string_view path, std::string_view content);
    static bool writeBinary(std::string_view path, std::span<const uint8_t> data);
    static bool exists(std::string_view path);
    static std::string getBasePath(std::string_view path);
    static std::string getFileName(std::string_view path);
    static std::string getExtension(std::string_view path);
    static std::string join(std::string_view a, std::string_view b);
};

} // namespace crf
