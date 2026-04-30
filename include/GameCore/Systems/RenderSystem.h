#pragma once

#include "GameCore/Core/Entity.h"
#include "GameCore/Core/System.h"

#include <cstdint>
#include <iosfwd>
#include <memory>
#include <string>
#include <vector>

namespace GameCore::Systems
{
    [[nodiscard]] int healthPercentage(int currentHealth, int maxHealth);

    struct RenderEntityLabel
    {
        Core::EntityID entity{Core::InvalidEntity};
        std::string name;
        char glyph{'?'};
    };

    struct RenderEntityState
    {
        Core::EntityID entity{Core::InvalidEntity};
        std::string name;
        char glyph{'?'};
        int x{0};
        int y{0};
        int currentHealth{0};
        int maxHealth{0};
        bool hasPosition{false};
        bool hasHealth{false};
        bool alive{false};
    };

    struct RenderFrame
    {
        std::uint64_t frameIndex{0};
        float deltaSeconds{0.0F};
        std::vector<RenderEntityState> entities;
    };

    class IRenderBackend
    {
    public:
        virtual ~IRenderBackend() = default;

        virtual void render(const RenderFrame& frame) = 0;
        [[nodiscard]] virtual bool shouldClose() const;
    };

    class TerminalRenderBackend final : public IRenderBackend
    {
    public:
        explicit TerminalRenderBackend(std::ostream& output, bool clearScreen = true);

        void render(const RenderFrame& frame) override;

    private:
        std::ostream& m_output;
        bool m_clearScreen{true};
    };

    class RenderSystem final : public Core::ISystem
    {
    public:
        RenderSystem(std::unique_ptr<IRenderBackend> backend,
                     std::vector<RenderEntityLabel> entities);

        void update(Core::World& world, const Core::FrameContext& context) override;
        [[nodiscard]] bool shouldClose() const;

    private:
        std::unique_ptr<IRenderBackend> m_backend;
        std::vector<RenderEntityLabel> m_entities;
    };
}
