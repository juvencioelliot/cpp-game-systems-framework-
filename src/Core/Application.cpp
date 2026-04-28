#include "GameCore/Core/Application.h"

namespace GameCore::Core
{
    void Application::setScene(std::unique_ptr<Scene> scene)
    {
        if (m_activeScene != nullptr)
        {
            m_activeScene->shutdown();
            m_activeScene->setApplicationContext(nullptr);
        }

        m_activeScene = std::move(scene);
        if (m_activeScene != nullptr)
        {
            m_activeScene->setApplicationContext(this);
        }
    }

    void Application::runFrames(std::uint64_t frameCount, float deltaSeconds)
    {
        if (m_activeScene == nullptr)
        {
            return;
        }

        m_running = true;

        for (std::uint64_t i = 0; i < frameCount && m_running; ++i)
        {
            m_activeScene->update(FrameContext{deltaSeconds, m_frameIndex});
            ++m_frameIndex;
            m_input.beginFrame();
        }

        m_running = false;
    }

    void Application::stop()
    {
        m_running = false;
    }

    bool Application::isRunning() const
    {
        return m_running;
    }

    std::uint64_t Application::frameIndex() const
    {
        return m_frameIndex;
    }

    InputManager& Application::input()
    {
        return m_input;
    }

    const InputManager& Application::input() const
    {
        return m_input;
    }

    ResourceManager& Application::resources()
    {
        return m_resources;
    }

    const ResourceManager& Application::resources() const
    {
        return m_resources;
    }

    Scene* Application::activeScene()
    {
        return m_activeScene.get();
    }

    const Scene* Application::activeScene() const
    {
        return m_activeScene.get();
    }
}
