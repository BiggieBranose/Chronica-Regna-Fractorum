#pragma once
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include <typeindex>

namespace crf {

class Asset {
public:
    virtual ~Asset() = default;
    virtual bool load(std::string_view path) = 0;
    virtual void unload() = 0;
    bool isValid() const { return m_valid; }
    const std::string& getPath() const { return m_path; }

protected:
    bool m_valid = false;
    std::string m_path;
};

template<typename T>
concept AssetType = std::is_base_of_v<Asset, T>;

class AssetManager {
public:
    AssetManager() = default;
    ~AssetManager();

    template<AssetType T>
    std::shared_ptr<T> get(std::string_view path) {
        std::string key(path);
        auto it = m_assets.find(key);
        if (it != m_assets.end())
            return std::static_pointer_cast<T>(it->second);

        auto asset = std::make_shared<T>();
        if (asset->load(path)) {
            asset->m_path = key;
            m_assets[key] = asset;
            return asset;
        }
        return nullptr;
    }

    void unloadAll();

private:
    std::unordered_map<std::string, std::shared_ptr<Asset>> m_assets;
};

} // namespace crf
