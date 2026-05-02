#include "GameCore/Core/SystemScheduler.h"

#include <algorithm>

namespace GameCore::Core
{
    void SystemScheduler::update(World& world, const FrameContext& context)
    {
        if (m_needsSort)
        {
            std::stable_sort(m_systems.begin(),
                             m_systems.end(),
                             [](const SystemEntry& left, const SystemEntry& right) {
                                 if (left.order.phase != right.order.phase)
                                 {
                                     return static_cast<int>(left.order.phase) <
                                            static_cast<int>(right.order.phase);
                                 }

                                 if (left.order.priority != right.order.priority)
                                 {
                                     return left.order.priority < right.order.priority;
                                 }

                                 return left.insertionOrder < right.insertionOrder;
                             });
            m_needsSort = false;
        }

        for (auto& system : m_systems)
        {
            system.system->update(world, context);
        }
    }

    void SystemScheduler::clear()
    {
        m_systems.clear();
        m_nextInsertionOrder = 0;
        m_needsSort = false;
    }

    std::size_t SystemScheduler::systemCount() const
    {
        return m_systems.size();
    }
}
