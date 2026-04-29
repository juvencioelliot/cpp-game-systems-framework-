#include "GameCore/Components/AttackComponent.h"
#include "GameCore/Components/HealthComponent.h"
#include "GameCore/Components/PositionComponent.h"
#include "GameCore/Core/Application.h"
#include "GameCore/Core/AssetLoaders.h"
#include "GameCore/Core/Entity.h"
#include "GameCore/Core/EntityPrefab.h"
#include "GameCore/Core/EventBus.h"
#include "GameCore/Core/Scene.h"
#include "GameCore/Core/SceneManager.h"
#include "GameCore/Core/System.h"
#include "GameCore/Core/World.h"
#include "GameCore/Systems/CombatSystem.h"

#include <algorithm>
#include <fstream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#ifndef GAMECORE_DEMO_ASSET_DIR
#define GAMECORE_DEMO_ASSET_DIR "demo/assets"
#endif

#ifndef GAMECORE_DEMO_OUTPUT_DIR
#define GAMECORE_DEMO_OUTPUT_DIR "build"
#endif

namespace
{
    struct CombatLogEvent
    {
        std::string message;
    };

    struct CombatantNames
    {
        std::string player{"Player"};
        std::string enemy{"Enemy"};
    };

    struct CombatFrame
    {
        int round{0};
        int playerHealth{0};
        int playerMaxHealth{1};
        int enemyHealth{0};
        int enemyMaxHealth{1};
        std::vector<std::string> log;
    };

    std::string htmlEscape(const std::string& value)
    {
        std::string escaped;
        escaped.reserve(value.size());

        for (const char character : value)
        {
            switch (character)
            {
            case '&':
                escaped += "&amp;";
                break;
            case '<':
                escaped += "&lt;";
                break;
            case '>':
                escaped += "&gt;";
                break;
            case '"':
                escaped += "&quot;";
                break;
            default:
                escaped += character;
                break;
            }
        }

        return escaped;
    }

    int healthPercent(int currentHealth, int maxHealth)
    {
        if (maxHealth <= 0)
        {
            return 0;
        }

        return std::clamp((currentHealth * 100) / maxHealth, 0, 100);
    }

    class CombatRoundSystem final : public GameCore::Core::ISystem
    {
    public:
        CombatRoundSystem(GameCore::Core::EntityID player,
                          GameCore::Core::EntityID enemy,
                          CombatantNames names,
                          std::vector<CombatFrame>& frames)
            : m_player(player),
              m_enemy(enemy),
              m_names(std::move(names)),
              m_frames(frames)
        {
        }

        void update(GameCore::Core::World& world,
                    const GameCore::Core::FrameContext&) override
        {
            using namespace GameCore;

            auto& health = world.storage<Components::HealthComponent>();
            auto& attacks = world.storage<Components::AttackComponent>();

            auto* playerHealth = health.get(m_player);
            auto* enemyHealth = health.get(m_enemy);
            if (playerHealth == nullptr || enemyHealth == nullptr)
            {
                publish(world, "Combat cannot continue: missing health data.");
                m_finished = true;
                return;
            }

            if (!playerHealth->isAlive() || !enemyHealth->isAlive())
            {
                finishCombat(world, *playerHealth);
                return;
            }

            CombatFrame frame;
            frame.round = m_round;

            const std::string roundMessage = "Round " + std::to_string(m_round);
            publish(world, roundMessage);
            frame.log.push_back(roundMessage);

            const std::string playerAttack = m_combat.attack(m_player, m_enemy, attacks, health);
            publish(world, playerAttack);
            frame.log.push_back(playerAttack);

            if (enemyHealth->isAlive())
            {
                const std::string enemyAttack = m_combat.attack(m_enemy, m_player, attacks, health);
                publish(world, enemyAttack);
                frame.log.push_back(enemyAttack);
            }

            frame.playerHealth = playerHealth->currentHealth;
            frame.playerMaxHealth = playerHealth->maxHealth;
            frame.enemyHealth = enemyHealth->currentHealth;
            frame.enemyMaxHealth = enemyHealth->maxHealth;
            m_frames.push_back(frame);

            if (!playerHealth->isAlive() || !enemyHealth->isAlive())
            {
                finishCombat(world, *playerHealth);
                return;
            }

            ++m_round;
        }

        [[nodiscard]] bool isFinished() const
        {
            return m_finished;
        }

        [[nodiscard]] const std::string& winner() const
        {
            return m_winner;
        }

    private:
        static void publish(GameCore::Core::World& world, const std::string& message)
        {
            world.events().publish(CombatLogEvent{message});
        }

        void finishCombat(GameCore::Core::World& world,
                          const GameCore::Components::HealthComponent& playerHealth)
        {
            if (m_finished)
            {
                return;
            }

            const bool playerWon = playerHealth.isAlive();
            m_winner = playerWon ? m_names.player : m_names.enemy;
            publish(world, m_winner + " wins!");
            world.destroyEntity(playerWon ? m_enemy : m_player);
            m_finished = true;
        }

        GameCore::Core::EntityID m_player{GameCore::Core::InvalidEntity};
        GameCore::Core::EntityID m_enemy{GameCore::Core::InvalidEntity};
        CombatantNames m_names;
        GameCore::Systems::CombatSystem m_combat;
        std::vector<CombatFrame>& m_frames;
        int m_round{1};
        std::string m_winner;
        bool m_finished{false};
    };

    class CombatDemoScene final : public GameCore::Core::Scene
    {
    protected:
        void onInitialize() override
        {
            using namespace GameCore;

            m_logListener = world().events().subscribe<CombatLogEvent>([this](const CombatLogEvent& event) {
                m_log.push_back(event.message);
            });

            if (auto* app = application())
            {
                app->input().setActionDown("AutoAdvance", true);
            }

            loadVisualAssets();

            const auto prefab = loadCombatPrefab();
            const auto entities = Core::PrefabInstantiator::instantiateAll(world(), *prefab);
            if (entities.size() < 2)
            {
                world().events().publish(CombatLogEvent{"Combat demo prefab must define two entities."});
                stopApplication();
                return;
            }

            m_player = entities[0];
            m_enemy = entities[1];

            m_combatSystem = &systems().addSystem<CombatRoundSystem>(
                m_player,
                m_enemy,
                CombatantNames{},
                m_frames);
        }

        void onUpdate(const GameCore::Core::FrameContext&) override
        {
            if (m_reportWritten || m_combatSystem == nullptr || !m_combatSystem->isFinished())
            {
                return;
            }

            writeHtmlReport(m_combatSystem->winner());
            m_reportWritten = true;
            stopApplication();
        }

        void onShutdown() override
        {
            if (m_logListener != 0)
            {
                world().events().unsubscribe<CombatLogEvent>(m_logListener);
                m_logListener = 0;
            }
        }

    private:
        static std::string assetPath(const std::string& filename)
        {
            return std::string(GAMECORE_DEMO_ASSET_DIR) + "/" + filename;
        }

        void loadVisualAssets()
        {
            if (auto* app = application())
            {
                m_arenaSvg = GameCore::Core::AssetLoaders::loadText(app->resources(),
                                                                     "demo/arena-svg",
                                                                     assetPath("arena.svg"));
                m_playerSvg = GameCore::Core::AssetLoaders::loadText(app->resources(),
                                                                      "demo/player-svg",
                                                                      assetPath("player.svg"));
                m_enemySvg = GameCore::Core::AssetLoaders::loadText(app->resources(),
                                                                     "demo/enemy-svg",
                                                                     assetPath("enemy.svg"));
            }
        }

        GameCore::Core::ResourceHandle<GameCore::Core::PrefabDocument> loadCombatPrefab()
        {
            if (auto* app = application())
            {
                return GameCore::Core::AssetLoaders::loadKeyValuePrefab(app->resources(),
                                                                        "demo/combat-prefab",
                                                                        assetPath("combat_demo.prefab"));
            }

            throw std::runtime_error("CombatDemoScene requires an application context.");
        }

        void stopApplication()
        {
            if (auto* app = application())
            {
                app->stop();
            }
        }

        void writeHtmlReport(const std::string& winner)
        {
            const std::string outputPath = std::string(GAMECORE_DEMO_OUTPUT_DIR) + "/GameCoreVisualDemo.html";
            std::ofstream output(outputPath);
            if (!output)
            {
                throw std::runtime_error("Could not write visual demo report: " + outputPath);
            }

            output << "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
                   << "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
                   << "<title>GameCoreCPP Visual Demo</title>"
                   << "<style>"
                   << ":root{color-scheme:light;background:#f8fafc;color:#111827;font-family:Inter,system-ui,sans-serif;}"
                   << "body{margin:0;padding:32px;background:#e5e7eb;}"
                   << "main{max-width:1120px;margin:0 auto;background:#fff;border:1px solid #d1d5db;box-shadow:0 20px 60px #0002;}"
                   << "header{padding:28px 32px;border-bottom:1px solid #e5e7eb;display:flex;justify-content:space-between;gap:24px;align-items:end;}"
                   << "h1{font-size:30px;margin:0 0 8px;}p{margin:0;color:#4b5563;line-height:1.45;}"
                   << ".badge{font-weight:700;background:#dcfce7;color:#166534;border:1px solid #86efac;padding:8px 12px;border-radius:999px;white-space:nowrap;}"
                   << ".arena{position:relative;margin:0;background:#dbeafe;min-height:260px;overflow:hidden;border-bottom:1px solid #e5e7eb;}"
                   << ".arena svg{display:block;width:100%;height:auto;}"
                   << ".fighter{position:absolute;width:132px;top:86px;text-align:center;}"
                   << ".fighter.player{left:16%;}.fighter.enemy{right:16%;}.fighter .sprite{filter:drop-shadow(0 14px 16px #0005);}"
                   << ".name{font-weight:800;margin-top:8px;}"
                   << ".content{display:grid;grid-template-columns:1fr 320px;gap:24px;padding:28px 32px;}"
                   << ".rounds{display:grid;gap:14px;}.round{border:1px solid #e5e7eb;border-radius:8px;padding:16px;background:#f9fafb;}"
                   << ".round h2{font-size:17px;margin:0 0 12px;}.stats{display:grid;grid-template-columns:1fr 1fr;gap:12px;margin-bottom:12px;}"
                   << ".bar{height:12px;background:#fee2e2;border-radius:999px;overflow:hidden;border:1px solid #fecaca;}"
                   << ".fill{height:100%;background:linear-gradient(90deg,#22c55e,#84cc16);}"
                   << ".statLabel{display:flex;justify-content:space-between;font-size:13px;margin-bottom:5px;color:#374151;}"
                   << "ul{margin:0;padding-left:19px;color:#374151;}li{margin:5px 0;}"
                   << "aside{border:1px solid #e5e7eb;border-radius:8px;padding:16px;background:#111827;color:#e5e7eb;align-self:start;}"
                   << "aside h2{font-size:17px;margin:0 0 12px;color:#fff;}code{color:#bfdbfe;}"
                   << ".systems{display:flex;flex-wrap:wrap;gap:8px;margin-top:12px;}.systems span{font-size:12px;border:1px solid #374151;border-radius:999px;padding:5px 8px;}"
                   << "@media(max-width:820px){body{padding:12px}.content{grid-template-columns:1fr;padding:18px}.fighter{width:92px;top:102px}.fighter.player{left:8%}.fighter.enemy{right:8%}header{display:block}}"
                   << "</style></head><body><main>";

            output << "<header><div><h1>GameCoreCPP Visual Demo</h1>"
                   << "<p>Prefab-loaded entities, SVG assets, scheduled systems, events, resources, and application lifecycle working together.</p></div>"
                   << "<div class=\"badge\">" << htmlEscape(winner) << " wins</div></header>";

            output << "<section class=\"arena\">" << m_arenaSvg->text
                   << "<div class=\"fighter player\"><div class=\"sprite\">" << m_playerSvg->text << "</div><div class=\"name\">Player</div></div>"
                   << "<div class=\"fighter enemy\"><div class=\"sprite\">" << m_enemySvg->text << "</div><div class=\"name\">Enemy</div></div>"
                   << "</section>";

            output << "<section class=\"content\"><div class=\"rounds\">";
            for (const auto& frame : m_frames)
            {
                output << "<article class=\"round\"><h2>Round " << frame.round << "</h2><div class=\"stats\">";
                writeHealthCard(output, "Player", frame.playerHealth, frame.playerMaxHealth);
                writeHealthCard(output, "Enemy", frame.enemyHealth, frame.enemyMaxHealth);
                output << "</div><ul>";
                for (const auto& message : frame.log)
                {
                    output << "<li>" << htmlEscape(message) << "</li>";
                }
                output << "</ul></article>";
            }

            output << "</div><aside><h2>Framework Path</h2>"
                   << "<p>This visual report was generated by the C++ demo using local assets loaded through <code>ResourceManager</code>.</p>"
                   << "<div class=\"systems\">"
                   << "<span>Application</span><span>SceneManager</span><span>Scene</span><span>World</span>"
                   << "<span>Prefabs</span><span>ResourceManager</span><span>AssetLoaders</span><span>EventBus</span>"
                   << "<span>SystemScheduler</span><span>InputManager</span>"
                   << "</div><h2 style=\"margin-top:22px\">Event Log</h2><ul>";

            for (const auto& message : m_log)
            {
                output << "<li>" << htmlEscape(message) << "</li>";
            }

            output << "</ul></aside></section></main></body></html>";
            output.close();

            if (auto* app = application())
            {
                app->diagnostics().info("GameCoreCPP Visual Demo generated: " + outputPath);
            }
        }

        static void writeHealthCard(std::ostream& output,
                                    const std::string& name,
                                    int currentHealth,
                                    int maxHealth)
        {
            output << "<div><div class=\"statLabel\"><strong>" << htmlEscape(name)
                   << "</strong><span>" << currentHealth << "/" << maxHealth
                   << "</span></div><div class=\"bar\"><div class=\"fill\" style=\"width:"
                   << healthPercent(currentHealth, maxHealth)
                   << "%\"></div></div></div>";
        }

        GameCore::Core::EntityID m_player{GameCore::Core::InvalidEntity};
        GameCore::Core::EntityID m_enemy{GameCore::Core::InvalidEntity};
        GameCore::Core::EventBus::ListenerID m_logListener{0};
        GameCore::Core::ResourceHandle<GameCore::Core::TextResource> m_arenaSvg;
        GameCore::Core::ResourceHandle<GameCore::Core::TextResource> m_playerSvg;
        GameCore::Core::ResourceHandle<GameCore::Core::TextResource> m_enemySvg;
        std::vector<CombatFrame> m_frames;
        std::vector<std::string> m_log;
        CombatRoundSystem* m_combatSystem{nullptr};
        bool m_reportWritten{false};
    };
}

int main()
{
    GameCore::Core::Application application;
    GameCore::Core::SceneManager scenes;

    scenes.registerScene("combat_demo", []() {
        return std::make_unique<CombatDemoScene>();
    });

    if (!scenes.changeScene(application, "combat_demo"))
    {
        return 1;
    }

    application.runFrames(16, 1.0F);
    return 0;
}
