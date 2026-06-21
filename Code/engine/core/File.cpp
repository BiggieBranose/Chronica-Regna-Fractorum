#include "File.hpp"
#include "Log.hpp"
#include <fstream>

namespace crf {

std::optional<std::string> File::readText(std::string_view path) {
    std::ifstream file(path.data(), std::ios::in | std::ios::binary);
    if (!file) return std::nullopt;
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return content;
}

std::optional<std::vector<uint8_t>> File::readBinary(std::string_view path) {
    std::ifstream file(path.data(), std::ios::in | std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;
    auto size = file.tellg();
    file.seekg(0);
    std::vector<uint8_t> content(size);
    file.read(reinterpret_cast<char*>(content.data()), size);
    return content;
}

bool File::writeText(std::string_view path, std::string_view content) {
    std::ofstream file(path.data(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(content.data(), content.size());
    return true;
}

bool File::writeBinary(std::string_view path, std::span<const uint8_t> data) {
    std::ofstream file(path.data(), std::ios::out | std::ios::binary | std::ios::trunc);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return true;
}

bool File::exists(std::string_view path) {
    return std::filesystem::exists(path);
}

std::string File::getBasePath(std::string_view path) {
    auto p = std::filesystem::path(path);
    return p.parent_path().string();
}

std::string File::getFileName(std::string_view path) {
    auto p = std::filesystem::path(path);
    return p.filename().string();
}

std::string File::getExtension(std::string_view path) {
    auto p = std::filesystem::path(path);
    return p.extension().string();
}

std::string File::join(std::string_view a, std::string_view b) {
    return (std::filesystem::path(a) / std::filesystem::path(b)).string();
}

} // namespace crf
