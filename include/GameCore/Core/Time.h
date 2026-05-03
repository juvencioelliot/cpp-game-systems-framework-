#pragma once

namespace GameCore::Core
{
    class IClock
    {
    public:
        virtual ~IClock() = default;

        [[nodiscard]] virtual double nowSeconds() const = 0;
    };

    class SteadyClock final : public IClock
    {
    public:
        [[nodiscard]] double nowSeconds() const override;
    };
}
