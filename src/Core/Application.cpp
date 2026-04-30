#include "GameCore/Core/Application.h"

#include <chrono>
#include <thread>

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

    void Application::run(const ApplicationRunOptions& options)
    {
        if (m_activeScene == nullptr)
        {
            return;
        }

        m_running = true;
        std::uint64_t framesRun = 0;

        while (m_running && (options.maxFrames == 0 || framesRun < options.maxFrames))
        {
            const auto frameStart = std::chrono::steady_clock::now();

            m_activeScene->update(FrameContext{options.fixedDeltaSeconds, m_frameIndex});
            ++m_frameIndex;
            ++framesRun;
            m_input.beginFrame();

            if (options.sleepToMaintainRate && options.fixedDeltaSeconds > 0.0F)
            {
                const auto targetFrameDuration = std::chrono::duration<float>(options.fixedDeltaSeconds);
                const auto elapsed = std::chrono::steady_clock::now() - frameStart;
                if (elapsed < targetFrameDuration)
                {
                    std::this_thread::sleep_for(targetFrameDuration - elapsed);
                }
            }
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

    Diagnostics& Application::diagnostics()
    {
        return m_diagnostics;
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
