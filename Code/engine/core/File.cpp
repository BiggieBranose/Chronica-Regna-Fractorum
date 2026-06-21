#include "File.hpp"
#include <fstream>
#include <filesystem>

namespace crf {

std::optional<Vec<byte>> File::readBinary(std::string_view path) {
    std::ifstream file(path.data(), std::ios::binary | std::ios::ate);
    if (!file) return std::nullopt;

    auto size = file.tellg();
    file.seekg(0, std::ios::beg);

    Vec<byte> data(static_cast<size_t>(size));
    if (!file.read(reinterpret_cast<char*>(data.data()), size)) {
        return std::nullopt;
    }
    return data;
}

std::optional<std::string> File::readText(std::string_view path) {
    std::ifstream file(path.data());
    if (!file) return std::nullopt;

    std::string content;
    file.seekg(0, std::ios::end);
    content.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(content.data(), static_cast<std::streamsize>(content.size()));
    return content;
}

bool File::writeBinary(std::string_view path, View<const byte> data) {
    std::ofstream file(path.data(), std::ios::binary);
    if (!file) return false;
    file.write(reinterpret_cast<const char*>(data.data()),
               static_cast<std::streamsize>(data.size()));
    return file.good();
}

bool File::writeText(std::string_view path, std::string_view text) {
    std::ofstream file(path.data());
    if (!file) return false;
    file << text;
    return file.good();
}

bool File::exists(std::string_view path) {
    return std::filesystem::exists(path);
}

std::string File::stem(std::string_view path) {
    return std::filesystem::path(path).stem().string();
}

std::string File::extension(std::string_view path) {
    return std::filesystem::path(path).extension().string();
}

std::string File::parent(std::string_view path) {
    return std::filesystem::path(path).parent_path().string();
}

std::string File::join(std::string_view a, std::string_view b) {
    return (std::filesystem::path(a) / b).string();
}

} // namespace crf
