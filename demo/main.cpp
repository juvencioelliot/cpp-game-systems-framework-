#include "CombatGameplay.h"

#include "GameCore/Components/MoveIntentComponent.h"
#include "GameCore/Components/NameComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Components/RenderableComponent.h"
#include "GameCore/Core/Application.h"
#include "GameCore/Core/AssetLoaders.h"
#include "GameCore/Core/Entity.h"
#include "GameCore/Core/EntityPrefab.h"
#include "GameCore/Core/EventBus.h"
#include "GameCore/Core/Scene.h"
#include "GameCore/Core/SceneManager.h"
#include "GameCore/Core/System.h"
#include "GameCore/Core/World.h"
#include "GameCore/Systems/MovementSystem.h"
#include "GameCore/Systems/RenderSystem.h"

#if GAMECORE_HAS_SDL2
#include "GameCore/Core/SdlInputBackend.h"
#include "GameCore/Systems/SdlRenderBackend.h"
#endif

#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef GAMECORE_DEMO_ASSET_DIR
#define GAMECORE_DEMO_ASSET_DIR "demo/assets"
#endif

namespace
{
    constexpr int ArenaMinX = 0;
    constexpr int ArenaMinY = 0;
    constexpr int ArenaMaxX = 27;
    constexpr int ArenaMaxY = 6;
    constexpr float EnemyDecisionIntervalSeconds = 0.48F;

    std::unique_ptr<GameCore::Systems::IRenderBackend> createDemoRenderBackend()
    {
#if GAMECORE_HAS_SDL2
        std::cout << "Using SDL2 render backend.\n"
                  << "Controls: WASD/arrow keys move, Space/Enter attacks, Escape/window close exits.\n";
        return std::make_unique<GameCore::Systems::SdlRenderBackend>(
            "GameCoreCPP SDL2 Demo",
            900,
            420,
            28);
#else
        std::cout << "Using terminal render backend. SDL2 input is unavailable in this build.\n";
        return std::make_unique<GameCore::Systems::TerminalRenderBackend>(std::cout);
#endif
    }

    int sign(const int value)
    {
        if (value < 0)
        {
            return -1;
        }

        return value > 0 ? 1 : 0;
    }

    bool areAdjacent(const GameCore::Components::PositionComponent& first,
                     const GameCore::Components::PositionComponent& second)
    {
        return std::abs(first.x - second.x) + std::abs(first.y - second.y) <= 1;
    }

    class CombatDemoScene final : public GameCore::Core::Scene
    {
    protected:
        void onInitialize() override
        {
            using namespace GameCore;

            m_logListener = world().events().subscribe<CombatDemo::CombatMessageEvent>(
                [this](const CombatDemo::CombatMessageEvent& event) {
                    addLog(event.message);
                });
            m_defeatListener = world().events().subscribe<CombatDemo::EntityDefeatedEvent>(
                [this](const CombatDemo::EntityDefeatedEvent& event) {
                    world().deferRemoveComponent<GameCore::Components::RenderableComponent>(event.entity);

                    if (event.entity == m_enemy)
                    {
                        addLog("Player wins! Close the window or press Escape to exit.");
                        m_combatFinished = true;
                    }
                    else if (event.entity == m_player)
                    {
                        addLog("Enemy wins! Close the window or press Escape to exit.");
                        m_combatFinished = true;
                    }
                });

            const auto prefab = loadCombatPrefab();
            const auto entities = Core::PrefabInstantiator::instantiateAll(world(), *prefab);
            if (entities.size() < 2)
            {
                addLog("Combat demo prefab must define two entities.");
                stopApplication();
                return;
            }

            m_player = entities[0];
            m_enemy = entities[1];
            world().addComponent(m_player, Components::NameComponent{"Player"});
            world().addComponent(m_player, Components::MoveIntentComponent{});
            world().addComponent(m_player, Components::RenderableComponent{'P', 0, true});
            world().addComponent(m_player, CombatDemo::AttackIntentComponent{});
            world().addComponent(m_enemy, Components::NameComponent{"Enemy"});
            world().addComponent(m_enemy, Components::MoveIntentComponent{});
            world().addComponent(m_enemy, Components::RenderableComponent{'E', 0, true});
            world().addComponent(m_enemy, CombatDemo::AttackIntentComponent{});

            systems().addSystem<Systems::MovementSystem>(
                Core::SystemOrder{Core::SystemPhase::Simulation, 0},
                ArenaMinX,
                ArenaMinY,
                ArenaMaxX,
                ArenaMaxY);
            systems().addSystem<CombatDemo::AttackIntentSystem>(
                Core::SystemOrder{Core::SystemPhase::Simulation, 10});

            m_renderSystem = &systems().addSystem<Systems::RenderSystem>(
                Core::SystemOrder{Core::SystemPhase::Render, 0},
                createDemoRenderBackend());

            addLog("Reach the enemy and press Space or Enter to attack.");
        }

        void onBeforeSystems(const GameCore::Core::FrameContext& context) override
        {
            processInput();

            if (!m_combatFinished)
            {
                writePlayerIntent();
                writeEnemyIntent(context.deltaSeconds);
            }
        }

        void onAfterSystems(const GameCore::Core::FrameContext&) override
        {
            for (const auto& message : m_recentLog)
            {
                std::cout << "  " << message << '\n';
            }
            std::cout << '\n';

            if (shouldQuit())
            {
                stopApplication();
                return;
            }
        }

        void onShutdown() override
        {
            if (m_logListener != 0)
            {
                world().events().unsubscribe<CombatDemo::CombatMessageEvent>(m_logListener);
                m_logListener = 0;
            }

            if (m_defeatListener != 0)
            {
                world().events().unsubscribe<CombatDemo::EntityDefeatedEvent>(m_defeatListener);
                m_defeatListener = 0;
            }
        }

    private:
        static std::string assetPath(const std::string& filename)
        {
            return std::string(GAMECORE_DEMO_ASSET_DIR) + "/" + filename;
        }

        GameCore::Core::ResourceHandle<GameCore::Core::PrefabDocument> loadCombatPrefab()
        {
            if (auto* app = application())
            {
                GameCore::Core::PrefabComponentRegistry registry;
                CombatDemo::registerCombatPrefabComponents(registry);
                return GameCore::Core::AssetLoaders::loadKeyValuePrefab(app->resources(),
                                                                        "demo/combat-prefab",
                                                                        assetPath("combat_demo.prefab"),
                                                                        registry);
            }

            throw std::runtime_error("CombatDemoScene requires an application context.");
        }

        void processInput()
        {
#if GAMECORE_HAS_SDL2
            if (auto* app = application())
            {
                m_input.process(app->input());
            }
#endif
        }

        [[nodiscard]] bool shouldQuit()
        {
            if (auto* app = application())
            {
                if (app->input().wasPressed("Quit"))
                {
                    return true;
                }
            }

            return m_renderSystem != nullptr && m_renderSystem->shouldClose();
        }

        void writePlayerIntent()
        {
            if (!world().isAlive(m_player))
            {
                return;
            }

            auto* app = application();
            auto* moveIntent = world().getComponent<GameCore::Components::MoveIntentComponent>(m_player);
            auto* attackIntent = world().getComponent<CombatDemo::AttackIntentComponent>(m_player);
            auto* playerPosition = world().getComponent<GameCore::Components::PositionComponent>(m_player);
            auto* enemyPosition = world().getComponent<GameCore::Components::PositionComponent>(m_enemy);
            if (app == nullptr || moveIntent == nullptr || attackIntent == nullptr ||
                playerPosition == nullptr || enemyPosition == nullptr)
            {
                return;
            }

            int dx = 0;
            int dy = 0;
            if (app->input().isHeld("MoveLeft"))
            {
                --dx;
            }
            if (app->input().isHeld("MoveRight"))
            {
                ++dx;
            }
            if (app->input().isHeld("MoveUp"))
            {
                --dy;
            }
            if (app->input().isHeld("MoveDown"))
            {
                ++dy;
            }

            if (dx != 0)
            {
                dy = 0;
            }

            const int targetX = std::clamp(playerPosition->x + dx, ArenaMinX, ArenaMaxX);
            const int targetY = std::clamp(playerPosition->y + dy, ArenaMinY, ArenaMaxY);
            if (targetX != enemyPosition->x || targetY != enemyPosition->y)
            {
                moveIntent->dx = dx;
                moveIntent->dy = dy;
            }

            if (app->input().wasPressed("PrimaryAction"))
            {
                if (areAdjacent(*playerPosition, *enemyPosition))
                {
                    attackIntent->target = m_enemy;
                }
                else
                {
                    addLog("Enemy is out of range.");
                }
            }
        }

        void writeEnemyIntent(const float deltaSeconds)
        {
            if (!world().isAlive(m_enemy) || !world().isAlive(m_player))
            {
                return;
            }

            m_enemyDecisionSeconds += deltaSeconds;
            if (m_enemyDecisionSeconds + 0.000001F < EnemyDecisionIntervalSeconds)
            {
                return;
            }
            m_enemyDecisionSeconds = 0.0F;

            auto* moveIntent = world().getComponent<GameCore::Components::MoveIntentComponent>(m_enemy);
            auto* attackIntent = world().getComponent<CombatDemo::AttackIntentComponent>(m_enemy);
            auto* enemyPosition = world().getComponent<GameCore::Components::PositionComponent>(m_enemy);
            auto* playerPosition = world().getComponent<GameCore::Components::PositionComponent>(m_player);
            if (moveIntent == nullptr || attackIntent == nullptr ||
                enemyPosition == nullptr || playerPosition == nullptr)
            {
                return;
            }

            if (areAdjacent(*enemyPosition, *playerPosition))
            {
                attackIntent->target = m_player;
                return;
            }

            const int dx = sign(playerPosition->x - enemyPosition->x);
            const int dy = dx == 0 ? sign(playerPosition->y - enemyPosition->y) : 0;
            const int targetX = std::clamp(enemyPosition->x + dx, ArenaMinX, ArenaMaxX);
            const int targetY = std::clamp(enemyPosition->y + dy, ArenaMinY, ArenaMaxY);
            if (targetX != playerPosition->x || targetY != playerPosition->y)
            {
                moveIntent->dx = dx;
                moveIntent->dy = dy;
            }
        }

        void addLog(std::string message)
        {
            m_recentLog.push_back(std::move(message));
            if (m_recentLog.size() > 6)
            {
                m_recentLog.erase(m_recentLog.begin());
            }
        }

        void stopApplication()
        {
            if (auto* app = application())
            {
                app->stop();
            }
        }

        GameCore::Core::EntityID m_player{GameCore::Core::InvalidEntity};
        GameCore::Core::EntityID m_enemy{GameCore::Core::InvalidEntity};
        GameCore::Core::EventBus::ListenerID m_logListener{0};
        GameCore::Core::EventBus::ListenerID m_defeatListener{0};
        std::vector<std::string> m_recentLog;
        GameCore::Systems::RenderSystem* m_renderSystem{nullptr};
        float m_enemyDecisionSeconds{0.0F};
        bool m_combatFinished{false};
#if GAMECORE_HAS_SDL2
        GameCore::Core::SdlInputBackend m_input;
#endif
    };
}

int main()
{
    GameCore::Core::Application application;
    GameCore::Core::SceneManager scenes;

    application.diagnostics().setMinimumLevel(GameCore::Core::LogLevel::Warning);

    scenes.registerScene("combat_demo", []() {
        return std::make_unique<CombatDemoScene>();
    });

    if (!scenes.changeScene(application, "combat_demo"))
    {
        return 1;
    }

    application.run(GameCore::Core::ApplicationRunOptions{
        0.12F,
        0,
        true,
    });
    return 0;
}
