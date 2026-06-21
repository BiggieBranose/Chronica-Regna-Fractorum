#include "Config.hpp"
#include "Log.hpp"
#include "File.hpp"
#include <sstream>

namespace crf {

Config& Config::instance() {
    static Config cfg;
    return cfg;
}

void Config::load(std::string_view path) {
    auto content = File::readText(path);
    if (!content) {
        Log::warn("Config file not found: {}", path);
        return;
    }
    std::istringstream stream(*content);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.empty() || line[0] == '#' || line[0] == ';') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = line.substr(0, eq);
        auto val = line.substr(eq + 1);
        while (!key.empty() && (key.front() == ' ' || key.front() == '\t')) key.erase(0, 1);
        while (!key.empty() && (key.back() == ' ' || key.back() == '\t')) key.pop_back();
        while (!val.empty() && (val.front() == ' ' || val.front() == '\t')) val.erase(0, 1);
        while (!val.empty() && (val.back() == ' ' || val.back() == '\t')) val.pop_back();
        m_data[key] = val;
    }
    Log::info("Config loaded: {} entries", m_data.size());
}

void Config::save(std::string_view path) {
    std::string out;
    for (auto& [k, v] : m_data)
        out += k + " = " + v + "\n";
    File::writeText(path, out);
}

int Config::getInt(std::string_view key, int def) const {
    auto it = m_data.find(std::string(key));
    if (it == m_data.end()) return def;
    return std::stoi(it->second);
}

float Config::getFloat(std::string_view key, float def) const {
    auto it = m_data.find(std::string(key));
    if (it == m_data.end()) return def;
    return std::stof(it->second);
}

bool Config::getBool(std::string_view key, bool def) const {
    auto it = m_data.find(std::string(key));
    if (it == m_data.end()) return def;
    auto& v = it->second;
    return v == "true" || v == "1" || v == "yes";
}

std::string Config::getString(std::string_view key, std::string_view def) const {
    auto it = m_data.find(std::string(key));
    if (it == m_data.end()) return std::string(def);
    return it->second;
}

glm::ivec2 Config::getIVec2(std::string_view key, glm::ivec2 def) const {
    auto it = m_data.find(std::string(key));
    if (it == m_data.end()) return def;
    auto& v = it->second;
    auto comma = v.find(',');
    if (comma == std::string::npos) return def;
    int x = std::stoi(v.substr(0, comma));
    int y = std::stoi(v.substr(comma + 1));
    return {x, y};
}

void Config::setInt(std::string_view key, int val) { m_data[std::string(key)] = std::to_string(val); }
void Config::setFloat(std::string_view key, float val) { m_data[std::string(key)] = std::to_string(val); }
void Config::setBool(std::string_view key, bool val) { m_data[std::string(key)] = val ? "true" : "false"; }
void Config::setString(std::string_view key, std::string_view val) { m_data[std::string(key)] = std::string(val); }

} // namespace crf
