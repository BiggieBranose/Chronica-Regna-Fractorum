#pragma once

#include "Types.hpp"
#include <unordered_map>

namespace crf {

class Config {
public:
    static Config& instance();

    bool load(std::string_view path);
    bool save(std::string_view path);

    std::optional<std::string> get(std::string_view key) const;
    void set(std::string_view key, std::string_view value);

    i32 getInt(std::string_view key, i32 fallback = 0) const;
    f32 getFloat(std::string_view key, f32 fallback = 0.0f) const;
    bool getBool(std::string_view key, bool fallback = false) const;

private:
    std::unordered_map<std::string, std::string> m_entries;
};

} // namespace crf
