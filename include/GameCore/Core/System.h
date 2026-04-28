#pragma once

#include "GameCore/Core/World.h"

#include <cstdint>

namespace GameCore::Core
{
    struct FrameContext
    {
        float deltaSeconds{0.0F};
        std::uint64_t frameIndex{0};
    };

    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        virtual void update(World& world, const FrameContext& context) = 0;
    };
}
