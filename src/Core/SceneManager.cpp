#include "GameCore/Core/SceneManager.h"

#include <stdexcept>
#include <utility>

namespace GameCore::Core
{
    void SceneManager::registerScene(std::string name, SceneFactory factory)
    {
        if (!factory)
        {
            throw std::invalid_argument("Scene factory cannot be empty.");
        }

        m_sceneFactories[std::move(name)] = std::move(factory);
    }

    void SceneManager::unregisterScene(const std::string& name)
    {
        m_sceneFactories.erase(name);
    }

    bool SceneManager::hasScene(const std::string& name) const
    {
        return m_sceneFactories.find(name) != m_sceneFactories.end();
    }

    std::size_t SceneManager::sceneCount() const
    {
        return m_sceneFactories.size();
    }

    std::unique_ptr<Scene> SceneManager::createScene(const std::string& name) const
    {
        const auto it = m_sceneFactories.find(name);
        if (it == m_sceneFactories.end())
        {
            return nullptr;
        }

        return it->second();
    }

    bool SceneManager::changeScene(Application& application, const std::string& name) const
    {
        auto scene = createScene(name);
        if (scene == nullptr)
        {
            return false;
        }

        application.setScene(std::move(scene));
        return true;
    }
}
