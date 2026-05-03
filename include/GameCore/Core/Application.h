#pragma once

#include "GameCore/Core/ApplicationContext.h"
#include "GameCore/Core/Diagnostics.h"
#include "GameCore/Core/InputManager.h"
#include "GameCore/Core/ResourceManager.h"
#include "GameCore/Core/Scene.h"
#include "GameCore/Core/Time.h"

#include <cstddef>
#include <cstdint>
#include <memory>

namespace GameCore::Core
{
    struct ApplicationRunOptions
    {
        float fixedDeltaSeconds{1.0F / 60.0F};
        std::uint64_t maxFrames{0};
        bool sleepToMaintainRate{false};
        IClock* clock{nullptr};
        std::size_t maxFixedStepsPerLoop{5};
    };

    class Application
        : public ApplicationContext
    {
    public:
        void setScene(std::unique_ptr<Scene> scene);

        void runFrames(std::uint64_t frameCount, float deltaSeconds);
        bool stepFrame(float deltaSeconds);
        void run(const ApplicationRunOptions& options = {});
        void stop() override;
        void setPaused(bool paused);

        [[nodiscard]] bool isRunning() const;
        [[nodiscard]] bool isPaused() const;
        [[nodiscard]] std::uint64_t frameIndex() const;
        [[nodiscard]] std::uint64_t fixedFrameIndex() const;
        [[nodiscard]] float totalSeconds() const;

        InputManager& input() override;
        const InputManager& input() const;

        ResourceManager& resources() override;
        Diagnostics& diagnostics() override;
        const ResourceManager& resources() const;

        Scene* activeScene();
        const Scene* activeScene() const;

    private:
        std::unique_ptr<Scene> m_activeScene;
        InputManager m_input;
        ResourceManager m_resources;
        Diagnostics m_diagnostics;
        std::uint64_t m_frameIndex{0};
        std::uint64_t m_fixedFrameIndex{0};
        float m_totalSeconds{0.0F};
        float m_fixedTimeAccumulator{0.0F};
        bool m_running{false};
        bool m_paused{false};
    };
}
