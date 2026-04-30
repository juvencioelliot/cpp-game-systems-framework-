#include "GameCore/Systems/RenderSystem.h"

#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Components/PositionComponent.h"
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

    }

    int healthPercentage(const int currentHealth, const int maxHealth)
    {
        if (maxHealth <= 0)
        {
            return 0;
        }

        return std::clamp((currentHealth * 100) / maxHealth, 0, 100);
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

    void TerminalRenderBackend::render(const RenderFrame& frame)
    {
        if (m_clearScreen)
        {
            m_output << "\x1B[2J\x1B[H";
        }

        std::vector<std::string> arena(ArenaHeight, std::string(ArenaWidth, '.'));
        for (const auto& entity : frame.entities)
        {
            if (!entity.alive || !entity.hasPosition)
            {
                continue;
            }

            const int x = std::clamp(entity.x, 0, ArenaWidth - 1);
            const int y = std::clamp(entity.y, 0, ArenaHeight - 1);
            arena[static_cast<std::size_t>(y)][static_cast<std::size_t>(x)] = entity.glyph;
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

        for (const auto& entity : frame.entities)
        {
            m_output << entity.glyph << " " << entity.name;
            if (!entity.alive)
            {
                m_output << " defeated";
            }
            else if (entity.hasHealth)
            {
                m_output << " HP " << entity.currentHealth << "/" << entity.maxHealth
                         << " [" << healthPercentage(entity.currentHealth, entity.maxHealth) << "%]";
            }
            m_output << '\n';
        }
        m_output << '\n';
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
        RenderFrame frame;
        frame.frameIndex = context.frameIndex;
        frame.deltaSeconds = context.deltaSeconds;
        frame.entities.reserve(m_entities.size());

        for (const auto& label : m_entities)
        {
            RenderEntityState state;
            state.entity = label.entity;
            state.name = label.name;
            state.glyph = label.glyph;
            state.alive = world.isAlive(label.entity);

            if (const auto* position = world.getComponent<Components::PositionComponent>(label.entity))
            {
                state.x = position->x;
                state.y = position->y;
                state.hasPosition = true;
            }

            if (const auto* health = world.getComponent<Components::HealthComponent>(label.entity))
            {
                state.currentHealth = health->currentHealth;
                state.maxHealth = health->maxHealth;
                state.hasHealth = true;
            }

            frame.entities.push_back(std::move(state));
        }

        m_backend->render(frame);
    }

    bool RenderSystem::shouldClose() const
    {
        return m_backend->shouldClose();
    }
}
