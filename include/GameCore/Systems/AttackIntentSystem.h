#pragma once

#include "GameCore/Core/System.h"
#include "GameCore/Systems/CombatSystem.h"

namespace GameCore::Systems
{
    class AttackIntentSystem final : public Core::ISystem
    {
    public:
        void update(Core::World& world, const Core::FrameContext& context) override;

    private:
        CombatSystem m_combat;
    };
}
