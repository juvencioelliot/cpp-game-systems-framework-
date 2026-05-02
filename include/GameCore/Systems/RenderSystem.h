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
    [[nodiscard]] int progressPercentage(int value, int maxValue);

    struct RenderEntityLabel
    {
        Core::EntityID entity{Core::InvalidEntity};
        std::string name;
        char glyph{'?'};
    };

    enum class DrawCommandType
    {
        GridCell,
        ProgressBar,
        Text,
        Rect,
    };

    struct DrawCommand
    {
        DrawCommandType type{DrawCommandType::GridCell};
        int layer{0};
        int x{0};
        int y{0};
        int width{0};
        int height{0};
        char glyph{'?'};
        std::string text;
        int value{0};
        int maxValue{0};
        bool active{true};
    };

    struct DrawFrame
    {
        std::uint64_t frameIndex{0};
        float deltaSeconds{0.0F};
        std::vector<DrawCommand> commands;
    };

    [[nodiscard]] DrawFrame buildDrawFrame(Core::World& world,
                                           const Core::FrameContext& context,
                                           const std::vector<RenderEntityLabel>& entities);
    [[nodiscard]] DrawFrame buildDrawFrame(Core::World& world,
                                           const Core::FrameContext& context);

    class IRenderBackend
    {
    public:
        virtual ~IRenderBackend() = default;

        virtual void render(const DrawFrame& frame) = 0;
        [[nodiscard]] virtual bool shouldClose() const;
    };

    class TerminalRenderBackend final : public IRenderBackend
    {
    public:
        explicit TerminalRenderBackend(std::ostream& output, bool clearScreen = true);

        void render(const DrawFrame& frame) override;

    private:
        std::ostream& m_output;
        bool m_clearScreen{true};
    };

    class RenderSystem final : public Core::ISystem
    {
    public:
        explicit RenderSystem(std::unique_ptr<IRenderBackend> backend);
        RenderSystem(std::unique_ptr<IRenderBackend> backend,
                     std::vector<RenderEntityLabel> entities);

        void update(Core::World& world, const Core::FrameContext& context) override;
        [[nodiscard]] bool shouldClose() const;

    private:
        std::unique_ptr<IRenderBackend> m_backend;
        std::vector<RenderEntityLabel> m_entities;
        bool m_useComponentQuery{false};
    };
}
