#pragma once

#include <cstdint>

namespace GameCore::Core
{
    using EntityID = std::uint32_t;

    constexpr EntityID InvalidEntity = 0;

    constexpr std::uint32_t EntityIndexBits = 20;
    constexpr std::uint32_t EntityGenerationBits = 32 - EntityIndexBits;
    constexpr std::uint32_t EntityIndexMask = (1U << EntityIndexBits) - 1U;
    constexpr std::uint32_t EntityGenerationMask = (1U << EntityGenerationBits) - 1U;
    constexpr std::uint32_t EntityMaxIndex = EntityIndexMask;
    constexpr std::uint32_t EntityMaxGeneration = EntityGenerationMask;

    [[nodiscard]] constexpr EntityID makeEntityID(const std::uint32_t index,
                                                  const std::uint32_t generation)
    {
        return ((generation & EntityGenerationMask) << EntityIndexBits) |
               (index & EntityIndexMask);
    }

    [[nodiscard]] constexpr std::uint32_t entityIndex(const EntityID entity)
    {
        return entity & EntityIndexMask;
    }

    [[nodiscard]] constexpr std::uint32_t entityGeneration(const EntityID entity)
    {
        return entity >> EntityIndexBits;
    }
}
