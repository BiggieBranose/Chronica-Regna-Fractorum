#pragma once
#include <string>
#include <unordered_map>
#include <glm/glm.hpp>

namespace crf {

class Config {
public:
    static Config& instance();

    void load(std::string_view path);
    void save(std::string_view path);

    int         getInt(std::string_view key, int def = 0) const;
    float       getFloat(std::string_view key, float def = 0.0f) const;
    bool        getBool(std::string_view key, bool def = false) const;
    std::string getString(std::string_view key, std::string_view def = "") const;
    glm::ivec2  getIVec2(std::string_view key, glm::ivec2 def = {}) const;

    void setInt(std::string_view key, int val);
    void setFloat(std::string_view key, float val);
    void setBool(std::string_view key, bool val);
    void setString(std::string_view key, std::string_view val);

private:
    Config() = default;
    std::unordered_map<std::string, std::string> m_data;
};

} // namespace crf
