#pragma once

#include "GameCore/Core/System.h"

namespace GameCore::Systems
{
    class MovementSystem final : public Core::ISystem
    {
    public:
        MovementSystem(int minX, int minY, int maxX, int maxY);

        void update(Core::World& world, const Core::FrameContext& context) override;

    private:
        int m_minX{0};
        int m_minY{0};
        int m_maxX{0};
        int m_maxY{0};
    };
}
