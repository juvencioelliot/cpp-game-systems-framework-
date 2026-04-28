#pragma once

#include "GameCore/Core/System.h"

#include <cstddef>
#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace GameCore::Core
{
    template <typename StateID>
    class StateMachine
    {
    public:
        struct State
        {
            std::function<void(World&)> onEnter;
            std::function<void(World&)> onExit;
            std::function<void(World&, const FrameContext&)> onUpdate;
        };

        void addState(const StateID& id, State state)
        {
            m_states[id] = std::move(state);
        }

        [[nodiscard]] bool hasState(const StateID& id) const
        {
            return m_states.find(id) != m_states.end();
        }

        void allowTransition(const StateID& from, const StateID& to)
        {
            m_hasTransitionRules = true;
            m_allowedTransitions[from].insert(to);
        }

        [[nodiscard]] bool canTransitionTo(const StateID& target) const
        {
            if (!m_currentState.has_value())
            {
                return hasState(target);
            }

            if (!m_hasTransitionRules)
            {
                return hasState(target);
            }

            const auto it = m_allowedTransitions.find(*m_currentState);
            return it != m_allowedTransitions.end() &&
                   it->second.find(target) != it->second.end();
        }

        void setInitialState(const StateID& id, World& world)
        {
            requireState(id);

            if (m_currentState.has_value())
            {
                throw std::logic_error("Initial state can only be set once.");
            }

            m_currentState = id;
            runEnter(id, world);
        }

        bool changeState(const StateID& target, World& world)
        {
            requireState(target);

            if (m_currentState.has_value() && *m_currentState == target)
            {
                return true;
            }

            if (!canTransitionTo(target))
            {
                return false;
            }

            if (m_currentState.has_value())
            {
                runExit(*m_currentState, world);
            }

            m_currentState = target;
            runEnter(target, world);
            return true;
        }

        void update(World& world, const FrameContext& context)
        {
            if (!m_currentState.has_value())
            {
                return;
            }

            auto& state = m_states.at(*m_currentState);
            if (state.onUpdate)
            {
                state.onUpdate(world, context);
            }
        }

        void clear()
        {
            m_states.clear();
            m_allowedTransitions.clear();
            m_currentState.reset();
            m_hasTransitionRules = false;
        }

        [[nodiscard]] const std::optional<StateID>& currentState() const
        {
            return m_currentState;
        }

        [[nodiscard]] std::size_t stateCount() const
        {
            return m_states.size();
        }

    private:
        void requireState(const StateID& id) const
        {
            if (!hasState(id))
            {
                throw std::invalid_argument("State is not registered.");
            }
        }

        void runEnter(const StateID& id, World& world)
        {
            auto& state = m_states.at(id);
            if (state.onEnter)
            {
                state.onEnter(world);
            }
        }

        void runExit(const StateID& id, World& world)
        {
            auto& state = m_states.at(id);
            if (state.onExit)
            {
                state.onExit(world);
            }
        }

        std::unordered_map<StateID, State> m_states;
        std::unordered_map<StateID, std::unordered_set<StateID>> m_allowedTransitions;
        std::optional<StateID> m_currentState;
        bool m_hasTransitionRules{false};
    };
}
