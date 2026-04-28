#pragma once

#include "GameCore/Core/Application.h"
#include "GameCore/Core/Scene.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>

namespace GameCore::Core
{
    class SceneManager
    {
    public:
        using SceneFactory = std::function<std::unique_ptr<Scene>()>;

        void registerScene(std::string name, SceneFactory factory);
        void unregisterScene(const std::string& name);

        [[nodiscard]] bool hasScene(const std::string& name) const;
        [[nodiscard]] std::size_t sceneCount() const;

        std::unique_ptr<Scene> createScene(const std::string& name) const;
        bool changeScene(Application& application, const std::string& name) const;

    private:
        std::unordered_map<std::string, SceneFactory> m_sceneFactories;
    };
}
