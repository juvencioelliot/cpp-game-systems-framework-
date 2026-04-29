#pragma once

#include "GameCore/Core/Diagnostics.h"
#include "GameCore/Core/InputManager.h"
#include "GameCore/Core/ResourceManager.h"

namespace GameCore::Core
{
    class ApplicationContext
    {
    public:
        virtual ~ApplicationContext() = default;

        virtual void stop() = 0;
        [[nodiscard]] virtual InputManager& input() = 0;
        [[nodiscard]] virtual ResourceManager& resources() = 0;
        [[nodiscard]] virtual Diagnostics& diagnostics() = 0;
    };
}
