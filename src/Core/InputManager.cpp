#include "GameCore/Core/InputManager.h"

#include <stdexcept>

namespace GameCore::Core
{
    void InputManager::setActionDown(const std::string& action, bool isDown)
    {
        if (action.empty())
        {
            throw std::invalid_argument("Input action name cannot be empty.");
        }

        m_actions[action].currentDown = isDown;
    }

    void InputManager::beginFrame()
    {
        for (auto& action : m_actions)
        {
            action.second.previousDown = action.second.currentDown;
        }
    }

    void InputManager::clear()
    {
        m_actions.clear();
    }

    bool InputManager::isHeld(const std::string& action) const
    {
        const auto* state = findAction(action);
        return state != nullptr && state->currentDown;
    }

    bool InputManager::wasPressed(const std::string& action) const
    {
        const auto* state = findAction(action);
        return state != nullptr && state->currentDown && !state->previousDown;
    }

    bool InputManager::wasReleased(const std::string& action) const
    {
        const auto* state = findAction(action);
        return state != nullptr && !state->currentDown && state->previousDown;
    }

    bool InputManager::hasAction(const std::string& action) const
    {
        return findAction(action) != nullptr;
    }

    std::size_t InputManager::actionCount() const
    {
        return m_actions.size();
    }

    const InputManager::ActionState* InputManager::findAction(const std::string& action) const
    {
        const auto it = m_actions.find(action);
        return it == m_actions.end() ? nullptr : &it->second;
    }
}
