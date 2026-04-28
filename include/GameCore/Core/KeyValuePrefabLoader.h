#pragma once

#include "GameCore/Core/EntityPrefab.h"

#include <string>

namespace GameCore::Core
{
    class KeyValuePrefabLoader
    {
    public:
        static PrefabDocument loadFromText(const std::string& text);
        static PrefabDocument loadFromFile(const std::string& path);
    };
}
