#pragma once

#include "GameCore/Systems/RenderSystem.h"

#include <memory>
#include <string>

namespace GameCore::Systems
{
    class SdlRenderBackend final : public IRenderBackend
    {
    public:
        SdlRenderBackend(std::string title, int width, int height, int cellSize);
        ~SdlRenderBackend() override;

        SdlRenderBackend(const SdlRenderBackend&) = delete;
        SdlRenderBackend& operator=(const SdlRenderBackend&) = delete;
        SdlRenderBackend(SdlRenderBackend&&) = delete;
        SdlRenderBackend& operator=(SdlRenderBackend&&) = delete;

        void render(const RenderFrame& frame) override;
        [[nodiscard]] bool shouldClose() const override;

    private:
        struct Impl;

        std::unique_ptr<Impl> m_impl;
    };
}
