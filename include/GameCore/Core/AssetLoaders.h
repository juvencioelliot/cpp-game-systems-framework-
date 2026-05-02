#pragma once

#include "GameCore/Core/EntityPrefab.h"
#include "GameCore/Core/ResourceManager.h"

#include <cstdint>
#include <string>
#include <vector>

namespace GameCore::Core
{
    struct TextResource
    {
        std::string text;
        std::string sourcePath;
    };

    struct BinaryResource
    {
        std::vector<std::uint8_t> bytes;
        std::string sourcePath;
    };

    class AssetLoaders
    {
    public:
        static ResourceHandle<TextResource> loadText(ResourceManager& resources,
                                                     const std::string& id,
                                                     const std::string& path);

        static ResourceHandle<BinaryResource> loadBinary(ResourceManager& resources,
                                                         const std::string& id,
                                                         const std::string& path);

        static ResourceHandle<PrefabDocument> loadKeyValuePrefab(ResourceManager& resources,
                                                                 const std::string& id,
                                                                 const std::string& path);

        static ResourceHandle<PrefabDocument> loadKeyValuePrefab(ResourceManager& resources,
                                                                 const std::string& id,
                                                                 const std::string& path,
                                                                 const PrefabComponentRegistry& registry);
    };
}
