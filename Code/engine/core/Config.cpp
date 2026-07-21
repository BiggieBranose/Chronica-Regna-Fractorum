#include "Config.hpp"
#include "File.hpp"
#include <sstream>
#include <charconv>

namespace crf {

Config& Config::instance() {
    static Config s_instance;
    return s_instance;
}

bool Config::load(std::string_view path) {
    auto content = File::readText(path);
    if (!content) return false;

    std::istringstream stream(*content);
    std::string line;
    while (std::getline(stream, line)) {
        auto commentPos = line.find_first_of("#;");
        if (commentPos != std::string::npos)
            line = line.substr(0, commentPos);

        auto eqPos = line.find('=');
        if (eqPos == std::string::npos) continue;

        auto key = line.substr(0, eqPos);
        auto val = line.substr(eqPos + 1);

        key.erase(0, key.find_first_not_of(" \t\r"));
        key.erase(key.find_last_not_of(" \t\r") + 1);
        val.erase(0, val.find_first_not_of(" \t\r"));
        val.erase(val.find_last_not_of(" \t\r") + 1);

        if (!key.empty())
            m_entries[std::move(key)] = std::move(val);
    }
    return true;
}

bool Config::save(std::string_view path) {
    std::string content;
    for (const auto& [key, val] : m_entries) {
        content += key + " = " + val + "\n";
    }
    return File::writeText(path, content);
}

std::optional<std::string> Config::get(std::string_view key) const {
    auto it = m_entries.find(std::string(key));
    if (it == m_entries.end()) return std::nullopt;
    return it->second;
}

void Config::set(std::string_view key, std::string_view value) {
    m_entries[std::string(key)] = std::string(value);
}

i32 Config::getInt(std::string_view key, i32 fallback) const {
    auto val = get(key);
    if (!val) return fallback;
    i32 result;
    auto [ptr, ec] = std::from_chars(val->data(), val->data() + val->size(), result);
    if (ec != std::errc{}) return fallback;
    return result;
}

f32 Config::getFloat(std::string_view key, f32 fallback) const {
    auto val = get(key);
    if (!val) return fallback;
    f32 result;
    auto [ptr, ec] = std::from_chars(val->data(), val->data() + val->size(), result);
    if (ec != std::errc{}) return fallback;
    return result;
}

bool Config::getBool(std::string_view key, bool fallback) const {
    auto val = get(key);
    if (!val) return fallback;
    if (*val == "true" || *val == "1" || *val == "yes") return true;
    if (*val == "false" || *val == "0" || *val == "no") return false;
    return fallback;
}

} // namespace crf
