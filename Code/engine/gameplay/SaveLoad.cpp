#include "SaveLoad.hpp"
#include "../core/File.hpp"
#include "../core/Log.hpp"

namespace crf {

bool SaveLoad::saveGame(std::string_view path, std::span<const uint8_t> data) {
    bool ok = File::writeBinary(path, data);
    if (ok) Log::info("Game saved: {}", path);
    else Log::error("Failed to save game: {}", path);
    return ok;
}

std::vector<uint8_t> SaveLoad::loadGame(std::string_view path) {
    auto data = File::readBinary(path);
    if (data) return std::move(*data);
    Log::error("Failed to load game: {}", path);
    return {};
}

std::vector<SaveSlot> SaveLoad::listSaves(std::string_view directory) {
    std::vector<SaveSlot> slots;
    // TODO: iterate directory for save files
    (void)directory;
    return slots;
}

bool SaveLoad::deleteSave(std::string_view path) {
    bool ok = std::filesystem::remove(path);
    if (ok) Log::info("Save deleted: {}", path);
    return ok;
}

bool SaveLoad::saveExists(std::string_view path) {
    return File::exists(path);
}

} // namespace crf
