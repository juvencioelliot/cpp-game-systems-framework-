#pragma once

#include "GameCore/Core/EntityPrefab.h"

#include <string>

namespace GameCore::Core
{
    class KeyValuePrefabLoader
    {
    public:
        static PrefabDocument loadFromText(const std::string& text);
        static PrefabDocument loadFromText(const std::string& text,
                                           const PrefabComponentRegistry& registry);
        static PrefabDocument loadFromFile(const std::string& path);
        static PrefabDocument loadFromFile(const std::string& path,
                                           const PrefabComponentRegistry& registry);
    };
}
