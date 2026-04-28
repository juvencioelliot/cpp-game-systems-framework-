#pragma once

#include "GameCore/Core/ApplicationContext.h"
#include "GameCore/Core/SystemScheduler.h"
#include "GameCore/Core/World.h"

namespace GameCore::Core
{
    class Scene
    {
    public:
        virtual ~Scene() = default;

        void initialize();
        void update(const FrameContext& context);
        void shutdown();

        [[nodiscard]] bool isInitialized() const;

        World& world();
        const World& world() const;

        SystemScheduler& systems();
        const SystemScheduler& systems() const;

    protected:
        ApplicationContext* application();
        const ApplicationContext* application() const;

        virtual void onInitialize() {}
        virtual void onUpdate(const FrameContext&) {}
        virtual void onShutdown() {}

    private:
        friend class Application;

        void setApplicationContext(ApplicationContext* application);

        World m_world;
        SystemScheduler m_systems;
        ApplicationContext* m_application{nullptr};
        bool m_initialized{false};
    };
}
