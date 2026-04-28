#pragma once

namespace GameCore::Components
{
    struct HealthComponent
    {
        int currentHealth{100};
        int maxHealth{100};

        [[nodiscard]] bool isAlive() const
        {
            return currentHealth > 0;
        }
    };
}
