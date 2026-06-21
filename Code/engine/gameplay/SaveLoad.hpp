#pragma once
#include <string>
#include <string_view>
#include <vector>
#include <cstdint>
#include <span>

namespace crf {

struct SaveSlot {
    std::string name;
    std::string timestamp;
    uint32_t version;
    uint64_t playTime;
};

class SaveLoad {
public:
    static bool saveGame(std::string_view path, std::span<const uint8_t> data);
    static std::vector<uint8_t> loadGame(std::string_view path);
    static std::vector<SaveSlot> listSaves(std::string_view directory);
    static bool deleteSave(std::string_view path);
    static bool saveExists(std::string_view path);
};

} // namespace crf
