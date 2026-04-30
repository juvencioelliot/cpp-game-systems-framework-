#pragma once

#include <memory>

namespace GameCore::Core
{
    class InputManager;

    class SdlInputBackend
    {
    public:
        SdlInputBackend();
        ~SdlInputBackend();

        SdlInputBackend(const SdlInputBackend&) = delete;
        SdlInputBackend& operator=(const SdlInputBackend&) = delete;
        SdlInputBackend(SdlInputBackend&&) = delete;
        SdlInputBackend& operator=(SdlInputBackend&&) = delete;

        void process(InputManager& input);

    private:
        struct Impl;

        std::unique_ptr<Impl> m_impl;
    };
}
