#pragma once

#include <cstddef>
#include <string>
#include <unordered_map>

namespace GameCore::Core
{
    class InputManager
    {
    public:
        void setActionDown(const std::string& action, bool isDown);
        void beginFrame();
        void clear();

        [[nodiscard]] bool isHeld(const std::string& action) const;
        [[nodiscard]] bool wasPressed(const std::string& action) const;
        [[nodiscard]] bool wasReleased(const std::string& action) const;
        [[nodiscard]] bool hasAction(const std::string& action) const;
        [[nodiscard]] std::size_t actionCount() const;

    private:
        struct ActionState
        {
            bool currentDown{false};
            bool previousDown{false};
        };

        [[nodiscard]] const ActionState* findAction(const std::string& action) const;

        std::unordered_map<std::string, ActionState> m_actions;
    };
}
