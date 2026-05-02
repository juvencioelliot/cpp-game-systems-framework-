#pragma once

#include "GameCore/Core/System.h"

namespace GameCore::Systems
{
    class TransformSystem final : public Core::ISystem
    {
    public:
        void update(Core::World& world, const Core::FrameContext& context) override;
    };
}
