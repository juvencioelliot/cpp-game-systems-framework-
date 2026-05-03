#include "GameCore/Core/Application.h"

#include <algorithm>
#include <chrono>
#include <thread>

namespace GameCore::Core
{
    namespace
    {
        constexpr float AccumulatorEpsilon = 0.000001F;

        [[nodiscard]] float elapsedSeconds(const IClock& clock, double& previousTimeSeconds)
        {
            const double now = clock.nowSeconds();
            const float elapsed = static_cast<float>(std::max(0.0, now - previousTimeSeconds));
            previousTimeSeconds = now;
            return elapsed;
        }
    }

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
            if (m_paused)
            {
                break;
            }

            stepFrame(deltaSeconds);
        }

        m_running = false;
    }

    bool Application::stepFrame(float deltaSeconds)
    {
        if (m_activeScene == nullptr)
        {
            return false;
        }

        m_activeScene->update(
            FrameContext{deltaSeconds, m_frameIndex, m_totalSeconds, m_fixedFrameIndex});
        ++m_frameIndex;
        ++m_fixedFrameIndex;
        m_totalSeconds += deltaSeconds;
        m_input.beginFrame();
        return true;
    }

    void Application::run(const ApplicationRunOptions& options)
    {
        if (m_activeScene == nullptr || options.fixedDeltaSeconds <= 0.0F ||
            options.maxFixedStepsPerLoop == 0)
        {
            return;
        }

        m_running = true;
        std::uint64_t framesRun = 0;
        SteadyClock steadyClock;
        IClock* clock = options.clock;
        if (clock == nullptr && options.sleepToMaintainRate)
        {
            clock = &steadyClock;
        }

        double previousTimeSeconds = 0.0;
        if (clock != nullptr)
        {
            previousTimeSeconds = clock->nowSeconds();
            if (options.clock == nullptr)
            {
                previousTimeSeconds -= options.fixedDeltaSeconds;
            }
        }

        while (m_running && (options.maxFrames == 0 || framesRun < options.maxFrames))
        {
            if (m_paused)
            {
                break;
            }

            const auto frameStart = std::chrono::steady_clock::now();
            const float elapsed = clock == nullptr
                                      ? options.fixedDeltaSeconds
                                      : elapsedSeconds(*clock, previousTimeSeconds);

            m_fixedTimeAccumulator += elapsed;

            std::size_t fixedStepsThisLoop = 0;
            while (m_running &&
                   m_fixedTimeAccumulator + AccumulatorEpsilon >= options.fixedDeltaSeconds &&
                   fixedStepsThisLoop < options.maxFixedStepsPerLoop &&
                   (options.maxFrames == 0 || framesRun < options.maxFrames))
            {
                stepFrame(options.fixedDeltaSeconds);
                m_fixedTimeAccumulator -= options.fixedDeltaSeconds;
                ++fixedStepsThisLoop;
                ++framesRun;
            }

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

    void Application::setPaused(const bool paused)
    {
        m_paused = paused;
        if (m_paused)
        {
            m_fixedTimeAccumulator = 0.0F;
        }
    }

    bool Application::isRunning() const
    {
        return m_running;
    }

    bool Application::isPaused() const
    {
        return m_paused;
    }

    std::uint64_t Application::frameIndex() const
    {
        return m_frameIndex;
    }

    std::uint64_t Application::fixedFrameIndex() const
    {
        return m_fixedFrameIndex;
    }

    float Application::totalSeconds() const
    {
        return m_totalSeconds;
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
