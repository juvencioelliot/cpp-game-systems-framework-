#include "GameCore/Core/AssetLoaders.h"

#include "GameCore/Core/FileSystem.h"
#include "GameCore/Core/KeyValuePrefabLoader.h"

#include <memory>

namespace GameCore::Core
{
    ResourceHandle<TextResource> AssetLoaders::loadText(ResourceManager& resources,
                                                        const std::string& id,
                                                        const std::string& path)
    {
        return resources.load<TextResource>(id, [path]() {
            return std::make_shared<TextResource>(TextResource{
                FileSystem::readTextFile(path),
                path,
            });
        }, path);
    }

    ResourceHandle<BinaryResource> AssetLoaders::loadBinary(ResourceManager& resources,
                                                            const std::string& id,
                                                            const std::string& path)
    {
        return resources.load<BinaryResource>(id, [path]() {
            return std::make_shared<BinaryResource>(BinaryResource{
                FileSystem::readBinaryFile(path),
                path,
            });
        }, path);
    }

    ResourceHandle<PrefabDocument> AssetLoaders::loadKeyValuePrefab(ResourceManager& resources,
                                                                     const std::string& id,
                                                                     const std::string& path)
    {
        return resources.load<PrefabDocument>(id, [path]() {
            return std::make_shared<PrefabDocument>(
                KeyValuePrefabLoader::loadFromFile(path));
        }, path);
    }

    ResourceHandle<PrefabDocument> AssetLoaders::loadKeyValuePrefab(
        ResourceManager& resources,
        const std::string& id,
        const std::string& path,
        const PrefabComponentRegistry& registry)
    {
        return resources.load<PrefabDocument>(id, [path, registry]() {
            return std::make_shared<PrefabDocument>(
                KeyValuePrefabLoader::loadFromFile(path, registry));
        }, path);
    }
}
