#pragma once

#include "GameCore/Core/System.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace GameCore::Core
{
    enum class SystemPhase
    {
        Input,
        Simulation,
        PostSimulation,
        Render,
    };

    struct SystemOrder
    {
        SystemPhase phase{SystemPhase::Simulation};
        int priority{0};
    };

    class SystemScheduler
    {
    public:
        template <typename System, typename... Args>
        System& addSystem(Args&&... args)
        {
            return addSystem<System>(SystemOrder{}, std::forward<Args>(args)...);
        }

        template <typename System, typename... Args>
        System& addSystem(SystemOrder order, Args&&... args)
        {
            static_assert(std::is_base_of<ISystem, System>::value,
                          "SystemScheduler can only own ISystem implementations.");

            auto system = std::make_unique<System>(std::forward<Args>(args)...);
            auto& reference = *system;
            m_systems.push_back(SystemEntry{
                std::move(system),
                order,
                m_nextInsertionOrder++,
            });
            m_needsSort = true;
            return reference;
        }

        void update(World& world, const FrameContext& context);
        void clear();

        [[nodiscard]] std::size_t systemCount() const;

    private:
        struct SystemEntry
        {
            std::unique_ptr<ISystem> system;
            SystemOrder order;
            std::size_t insertionOrder{0};
        };

        std::vector<SystemEntry> m_systems;
        std::size_t m_nextInsertionOrder{0};
        bool m_needsSort{false};
    };
}
