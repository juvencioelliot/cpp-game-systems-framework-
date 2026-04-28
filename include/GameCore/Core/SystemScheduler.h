#pragma once

#include "GameCore/Core/System.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace GameCore::Core
{
    class SystemScheduler
    {
    public:
        template <typename System, typename... Args>
        System& addSystem(Args&&... args)
        {
            static_assert(std::is_base_of<ISystem, System>::value,
                          "SystemScheduler can only own ISystem implementations.");

            auto system = std::make_unique<System>(std::forward<Args>(args)...);
            auto& reference = *system;
            m_systems.push_back(std::move(system));
            return reference;
        }

        void update(World& world, const FrameContext& context);
        void clear();

        [[nodiscard]] std::size_t systemCount() const;

    private:
        std::vector<std::unique_ptr<ISystem>> m_systems;
    };
}
