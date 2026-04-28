#include "GameCore/Core/Scene.h"

namespace GameCore::Core
{
    void Scene::initialize()
    {
        if (m_initialized)
        {
            return;
        }

        m_initialized = true;
        onInitialize();
    }

    void Scene::update(const FrameContext& context)
    {
        if (!m_initialized)
        {
            initialize();
        }

        m_systems.update(m_world, context);
        onUpdate(context);
    }

    void Scene::shutdown()
    {
        if (!m_initialized)
        {
            return;
        }

        onShutdown();
        m_systems.clear();
        m_world.events().clearAll();
        m_initialized = false;
    }

    bool Scene::isInitialized() const
    {
        return m_initialized;
    }

    World& Scene::world()
    {
        return m_world;
    }

    const World& Scene::world() const
    {
        return m_world;
    }

    SystemScheduler& Scene::systems()
    {
        return m_systems;
    }

    const SystemScheduler& Scene::systems() const
    {
        return m_systems;
    }

    ApplicationContext* Scene::application()
    {
        return m_application;
    }

    const ApplicationContext* Scene::application() const
    {
        return m_application;
    }

    void Scene::setApplicationContext(ApplicationContext* application)
    {
        m_application = application;
    }
}
