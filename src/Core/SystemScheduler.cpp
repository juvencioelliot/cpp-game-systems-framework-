#include "GameCore/Core/SystemScheduler.h"

namespace GameCore::Core
{
    void SystemScheduler::update(World& world, const FrameContext& context)
    {
        for (auto& system : m_systems)
        {
            system->update(world, context);
        }
    }

    void SystemScheduler::clear()
    {
        m_systems.clear();
    }

    std::size_t SystemScheduler::systemCount() const
    {
        return m_systems.size();
    }
}
