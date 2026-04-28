#include "GameCore/Core/KeyValuePrefabLoader.h"

#include "GameCore/Core/FileSystem.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace GameCore::Core
{
    namespace
    {
        std::string trim(const std::string& value)
        {
            auto begin = value.begin();
            while (begin != value.end() && std::isspace(static_cast<unsigned char>(*begin)))
            {
                ++begin;
            }

            auto end = value.end();
            while (end != begin && std::isspace(static_cast<unsigned char>(*(end - 1))))
            {
                --end;
            }

            return std::string(begin, end);
        }

        int parseInt(const std::string& key, const std::string& value, int lineNumber)
        {
            try
            {
                std::size_t parsedCharacters = 0;
                const int parsedValue = std::stoi(value, &parsedCharacters);
                if (parsedCharacters != value.size())
                {
                    throw std::invalid_argument("trailing characters");
                }

                return parsedValue;
            }
            catch (const std::exception&)
            {
                throw std::runtime_error("Invalid integer for key '" + key +
                                         "' on line " + std::to_string(lineNumber) + ".");
            }
        }

        EntityPrefab& requireCurrentEntity(PrefabDocument& document, int lineNumber)
        {
            if (document.entities.empty())
            {
                throw std::runtime_error("Prefab property appears before an entity section on line " +
                                         std::to_string(lineNumber) + ".");
            }

            return document.entities.back();
        }

        void applyProperty(EntityPrefab& prefab,
                           const std::string& key,
                           const std::string& value,
                           int lineNumber)
        {
            if (key == "name")
            {
                prefab.name = value;
                return;
            }

            if (key == "health.current")
            {
                if (!prefab.health.has_value())
                {
                    prefab.health = Components::HealthComponent{};
                }

                prefab.health->currentHealth = parseInt(key, value, lineNumber);
                return;
            }

            if (key == "health.max")
            {
                if (!prefab.health.has_value())
                {
                    prefab.health = Components::HealthComponent{};
                }

                prefab.health->maxHealth = parseInt(key, value, lineNumber);
                return;
            }

            if (key == "attack.damage")
            {
                if (!prefab.attack.has_value())
                {
                    prefab.attack = Components::AttackComponent{};
                }

                prefab.attack->damage = parseInt(key, value, lineNumber);
                return;
            }

            if (key == "position.x")
            {
                if (!prefab.position.has_value())
                {
                    prefab.position = Components::PositionComponent{};
                }

                prefab.position->x = parseInt(key, value, lineNumber);
                return;
            }

            if (key == "position.y")
            {
                if (!prefab.position.has_value())
                {
                    prefab.position = Components::PositionComponent{};
                }

                prefab.position->y = parseInt(key, value, lineNumber);
                return;
            }

            throw std::runtime_error("Unknown prefab key '" + key +
                                     "' on line " + std::to_string(lineNumber) + ".");
        }
    }

    PrefabDocument KeyValuePrefabLoader::loadFromText(const std::string& text)
    {
        PrefabDocument document;
        std::istringstream stream(text);
        std::string line;
        int lineNumber = 0;

        while (std::getline(stream, line))
        {
            ++lineNumber;

            const std::string trimmedLine = trim(line);
            if (trimmedLine.empty() || trimmedLine[0] == '#')
            {
                continue;
            }

            if (trimmedLine.front() == '[' && trimmedLine.back() == ']')
            {
                EntityPrefab prefab;
                prefab.name = trim(trimmedLine.substr(1, trimmedLine.size() - 2));
                if (prefab.name.empty())
                {
                    throw std::runtime_error("Entity section name cannot be empty on line " +
                                             std::to_string(lineNumber) + ".");
                }

                document.entities.push_back(std::move(prefab));
                continue;
            }

            const auto separator = trimmedLine.find('=');
            if (separator == std::string::npos)
            {
                throw std::runtime_error("Expected key=value on line " +
                                         std::to_string(lineNumber) + ".");
            }

            const std::string key = trim(trimmedLine.substr(0, separator));
            const std::string value = trim(trimmedLine.substr(separator + 1));
            if (key.empty())
            {
                throw std::runtime_error("Prefab key cannot be empty on line " +
                                         std::to_string(lineNumber) + ".");
            }

            applyProperty(requireCurrentEntity(document, lineNumber), key, value, lineNumber);
        }

        return document;
    }

    PrefabDocument KeyValuePrefabLoader::loadFromFile(const std::string& path)
    {
        return loadFromText(FileSystem::readTextFile(path));
    }
}
