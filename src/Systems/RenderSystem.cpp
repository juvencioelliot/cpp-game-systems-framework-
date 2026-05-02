#include "GameCore/Systems/RenderSystem.h"

#include "GameCore/Components/NameComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Components/ProgressComponent.h"
#include "GameCore/Components/RenderableComponent.h"
#include "GameCore/Components/TransformComponent.h"
#include "GameCore/Core/World.h"

#include <algorithm>
#include <iomanip>
#include <ostream>
#include <stdexcept>
#include <utility>

namespace GameCore::Systems
{
    namespace
    {
        constexpr int ArenaWidth = 28;
        constexpr int ArenaHeight = 7;
        constexpr int GridLayer = 10;
        constexpr int ProgressLayer = 20;
        constexpr int TextLayer = 30;

        [[nodiscard]] bool commandComesBefore(const DrawCommand& left, const DrawCommand& right)
        {
            return left.layer < right.layer;
        }

        [[nodiscard]] std::string entityDisplayName(Core::World& world,
                                                    const Core::EntityID entity,
                                                    const std::string& fallback)
        {
            if (const auto* name = world.getComponent<Components::NameComponent>(entity);
                name != nullptr && !name->value.empty())
            {
                return name->value;
            }

            return fallback;
        }

        [[nodiscard]] bool entityRenderPosition(Core::World& world,
                                                const Core::EntityID entity,
                                                int& x,
                                                int& y)
        {
            if (const auto* worldTransform =
                    world.getComponent<Components::WorldTransformComponent>(entity))
            {
                x = static_cast<int>(worldTransform->x);
                y = static_cast<int>(worldTransform->y);
                return true;
            }

            if (const auto* position = world.getComponent<Components::PositionComponent>(entity))
            {
                x = position->x;
                y = position->y;
                return true;
            }

            return false;
        }

        void appendProgressCommand(Core::World& world,
                                   const Core::EntityID entity,
                                   const int layer,
                                   const char glyph,
                                   const std::string& text,
                                   const bool active,
                                   std::vector<DrawCommand>& commands)
        {
            if (const auto* progress = world.getComponent<Components::ProgressComponent>(entity))
            {
                commands.push_back(DrawCommand{
                    DrawCommandType::ProgressBar,
                    layer,
                    0,
                    0,
                    260,
                    18,
                    glyph,
                    text,
                    progress->value,
                    progress->maxValue,
                    active,
                });
            }
        }

    }

    int progressPercentage(const int value, const int maxValue)
    {
        if (maxValue <= 0)
        {
            return 0;
        }

        return std::clamp((value * 100) / maxValue, 0, 100);
    }

    bool IRenderBackend::shouldClose() const
    {
        return false;
    }

    TerminalRenderBackend::TerminalRenderBackend(std::ostream& output, const bool clearScreen)
        : m_output(output),
          m_clearScreen(clearScreen)
    {
    }

    DrawFrame buildDrawFrame(Core::World& world,
                             const Core::FrameContext& context,
                             const std::vector<RenderEntityLabel>& entities)
    {
        DrawFrame frame;
        frame.frameIndex = context.frameIndex;
        frame.deltaSeconds = context.deltaSeconds;
        frame.commands.reserve(entities.size() * 2);

        for (const auto& label : entities)
        {
            const bool alive = world.isAlive(label.entity);

            int x = 0;
            int y = 0;
            if (alive && entityRenderPosition(world, label.entity, x, y))
            {
                frame.commands.push_back(DrawCommand{
                    DrawCommandType::GridCell,
                    GridLayer,
                    x,
                    y,
                    1,
                    1,
                    label.glyph,
                    label.name,
                    0,
                    0,
                    true,
                });
            }

            appendProgressCommand(world,
                                  label.entity,
                                  ProgressLayer,
                                  label.glyph,
                                  entityDisplayName(world, label.entity, label.name),
                                  alive,
                                  frame.commands);

            if (world.getComponent<Components::ProgressComponent>(label.entity) == nullptr)
            {
                frame.commands.push_back(DrawCommand{
                    DrawCommandType::Text,
                    TextLayer,
                    0,
                    0,
                    0,
                    0,
                    label.glyph,
                    label.name,
                    0,
                    0,
                    alive,
                });
            }
        }

        std::stable_sort(frame.commands.begin(), frame.commands.end(), commandComesBefore);
        return frame;
    }

    DrawFrame buildDrawFrame(Core::World& world, const Core::FrameContext& context)
    {
        DrawFrame frame;
        frame.frameIndex = context.frameIndex;
        frame.deltaSeconds = context.deltaSeconds;
        frame.commands.reserve(world.storage<Components::RenderableComponent>().size() * 2);

        world.each<Components::RenderableComponent>(
            [&world, &frame](const Core::EntityID entity,
                             const Components::RenderableComponent& renderable) {
                if (!renderable.visible || !world.isAlive(entity))
                {
                    return;
                }

                const std::string name = entityDisplayName(world, entity, "");

                int x = 0;
                int y = 0;
                if (entityRenderPosition(world, entity, x, y))
                {
                    frame.commands.push_back(DrawCommand{
                        DrawCommandType::GridCell,
                        renderable.layer,
                        x,
                        y,
                        1,
                        1,
                        renderable.glyph,
                        name,
                        0,
                        0,
                        true,
                    });
                }

                appendProgressCommand(world,
                                      entity,
                                      renderable.layer + 10,
                                      renderable.glyph,
                                      name,
                                      true,
                                      frame.commands);
            });

        std::stable_sort(frame.commands.begin(), frame.commands.end(), commandComesBefore);
        return frame;
    }

    void TerminalRenderBackend::render(const DrawFrame& frame)
    {
        if (m_clearScreen)
        {
            m_output << "\x1B[2J\x1B[H";
        }

        std::vector<std::string> arena(ArenaHeight, std::string(ArenaWidth, '.'));
        for (const auto& command : frame.commands)
        {
            if (command.type != DrawCommandType::GridCell || !command.active)
            {
                continue;
            }

            const int x = std::clamp(command.x, 0, ArenaWidth - 1);
            const int y = std::clamp(command.y, 0, ArenaHeight - 1);
            arena[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = command.glyph;
        }

        m_output << "GameCoreCPP Runtime Renderer | frame " << frame.frameIndex
                 << " | dt " << std::fixed << std::setprecision(3) << frame.deltaSeconds << "s\n";
        m_output << '+';
        for (int x = 0; x < ArenaWidth; ++x)
        {
            m_output << '-';
        }
        m_output << "+\n";

        for (const auto& row : arena)
        {
            m_output << '|' << row << "|\n";
        }

        m_output << '+';
        for (int x = 0; x < ArenaWidth; ++x)
        {
            m_output << '-';
        }
        m_output << "+\n";

        for (const auto& command : frame.commands)
        {
            if (command.type != DrawCommandType::ProgressBar &&
                command.type != DrawCommandType::Text)
            {
                continue;
            }

            m_output << command.glyph << " " << command.text;
            if (!command.active)
            {
                m_output << " defeated";
            }
            else if (command.type == DrawCommandType::ProgressBar)
            {
                m_output << " Progress " << command.value << "/" << command.maxValue
                         << " [" << progressPercentage(command.value, command.maxValue) << "%]";
            }
            m_output << '\n';
        }
        m_output << '\n';
    }

    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend)
        : m_backend(std::move(backend)),
          m_useComponentQuery(true)
    {
        if (m_backend == nullptr)
        {
            throw std::invalid_argument("RenderSystem requires a render backend.");
        }
    }

    RenderSystem::RenderSystem(std::unique_ptr<IRenderBackend> backend,
                               std::vector<RenderEntityLabel> entities)
        : m_backend(std::move(backend)),
          m_entities(std::move(entities))
    {
        if (m_backend == nullptr)
        {
            throw std::invalid_argument("RenderSystem requires a render backend.");
        }
    }

    void RenderSystem::update(Core::World& world, const Core::FrameContext& context)
    {
        if (m_useComponentQuery)
        {
            m_backend->render(buildDrawFrame(world, context));
            return;
        }

        m_backend->render(buildDrawFrame(world, context, m_entities));
    }

    bool RenderSystem::shouldClose() const
    {
        return m_backend->shouldClose();
    }
}
