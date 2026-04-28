#pragma once

#include "GameCore/Core/ApplicationContext.h"
#include "GameCore/Core/InputManager.h"
#include "GameCore/Core/ResourceManager.h"
#include "GameCore/Core/Scene.h"

#include <cstdint>
#include <memory>

namespace GameCore::Core
{
    class Application
        : public ApplicationContext
    {
    public:
        void setScene(std::unique_ptr<Scene> scene);

        void runFrames(std::uint64_t frameCount, float deltaSeconds);
        void stop() override;

        [[nodiscard]] bool isRunning() const;
        [[nodiscard]] std::uint64_t frameIndex() const;

        InputManager& input() override;
        const InputManager& input() const;

        ResourceManager& resources() override;
        const ResourceManager& resources() const;

        Scene* activeScene();
        const Scene* activeScene() const;

    private:
        std::unique_ptr<Scene> m_activeScene;
        InputManager m_input;
        ResourceManager m_resources;
        std::uint64_t m_frameIndex{0};
        bool m_running{false};
    };
}
