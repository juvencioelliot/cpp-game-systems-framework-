#include "GameCore/Core/Time.h"

#include <chrono>

namespace GameCore::Core
{
    double SteadyClock::nowSeconds() const
    {
        const auto now = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration<double>(now).count();
    }
}
