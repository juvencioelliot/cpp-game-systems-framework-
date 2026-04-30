#include "GameCore/Systems/SdlRenderBackend.h"

#include <SDL.h>

#include <algorithm>
#include <stdexcept>

namespace GameCore::Systems
{
    namespace
    {
        constexpr int ArenaColumns = 28;
        constexpr int ArenaRows = 7;
        constexpr int HeaderHeight = 48;
        constexpr int FooterHeight = 96;

        SDL_Color entityColor(char glyph)
        {
            switch (glyph)
            {
            case 'P':
                return SDL_Color{59, 130, 246, 255};
            case 'E':
                return SDL_Color{239, 68, 68, 255};
            default:
                return SDL_Color{245, 158, 11, 255};
            }
        }

        void fillRect(SDL_Renderer* renderer,
                      int x,
                      int y,
                      int width,
                      int height,
                      SDL_Color color)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_Rect rect{x, y, width, height};
            SDL_RenderFillRect(renderer, &rect);
        }

        void drawRect(SDL_Renderer* renderer,
                      int x,
                      int y,
                      int width,
                      int height,
                      SDL_Color color)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_Rect rect{x, y, width, height};
            SDL_RenderDrawRect(renderer, &rect);
        }
    }

    struct SdlRenderBackend::Impl
    {
        SDL_Window* window{nullptr};
        SDL_Renderer* renderer{nullptr};
        int width{0};
        int height{0};
        int cellSize{0};
        bool ownsSdl{false};
        bool shouldClose{false};
    };

    SdlRenderBackend::SdlRenderBackend(std::string title,
                                       const int width,
                                       const int height,
                                       const int cellSize)
        : m_impl(std::make_unique<Impl>())
    {
        if (width <= 0 || height <= 0 || cellSize <= 0)
        {
            throw std::invalid_argument("SDL render backend dimensions must be positive.");
        }

        if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
        {
            if (SDL_Init(SDL_INIT_VIDEO) != 0)
            {
                throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
            }
            m_impl->ownsSdl = true;
        }

        m_impl->window = SDL_CreateWindow(title.c_str(),
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          width,
                                          height,
                                          SDL_WINDOW_SHOWN);
        if (m_impl->window == nullptr)
        {
            const std::string error = SDL_GetError();
            if (m_impl->ownsSdl)
            {
                SDL_Quit();
            }
            throw std::runtime_error("SDL_CreateWindow failed: " + error);
        }

        SDL_ShowWindow(m_impl->window);
        SDL_RaiseWindow(m_impl->window);

        m_impl->renderer = SDL_CreateRenderer(m_impl->window, -1, SDL_RENDERER_ACCELERATED);
        if (m_impl->renderer == nullptr)
        {
            m_impl->renderer = SDL_CreateRenderer(m_impl->window, -1, SDL_RENDERER_SOFTWARE);
        }

        if (m_impl->renderer == nullptr)
        {
            const std::string error = SDL_GetError();
            SDL_DestroyWindow(m_impl->window);
            m_impl->window = nullptr;
            if (m_impl->ownsSdl)
            {
                SDL_Quit();
            }
            throw std::runtime_error("SDL_CreateRenderer failed: " + error);
        }

        m_impl->width = width;
        m_impl->height = height;
        m_impl->cellSize = cellSize;
    }

    SdlRenderBackend::~SdlRenderBackend()
    {
        if (m_impl->renderer != nullptr)
        {
            SDL_DestroyRenderer(m_impl->renderer);
        }

        if (m_impl->window != nullptr)
        {
            SDL_DestroyWindow(m_impl->window);
        }

        if (m_impl->ownsSdl)
        {
            SDL_Quit();
        }
    }

    void SdlRenderBackend::render(const RenderFrame& frame)
    {
        auto* renderer = m_impl->renderer;
        fillRect(renderer, 0, 0, m_impl->width, m_impl->height, SDL_Color{15, 23, 42, 255});
        fillRect(renderer,
                 0,
                 HeaderHeight,
                 m_impl->width,
                 m_impl->height - HeaderHeight - FooterHeight,
                 SDL_Color{30, 41, 59, 255});

        const int arenaWidth = ArenaColumns * m_impl->cellSize;
        const int arenaHeight = ArenaRows * m_impl->cellSize;
        const int arenaX = (m_impl->width - arenaWidth) / 2;
        const int arenaY = HeaderHeight + 24;

        fillRect(renderer, arenaX, arenaY, arenaWidth, arenaHeight, SDL_Color{51, 65, 85, 255});

        SDL_SetRenderDrawColor(renderer, 71, 85, 105, 255);
        for (int column = 0; column <= ArenaColumns; ++column)
        {
            const int x = arenaX + column * m_impl->cellSize;
            SDL_RenderDrawLine(renderer, x, arenaY, x, arenaY + arenaHeight);
        }
        for (int row = 0; row <= ArenaRows; ++row)
        {
            const int y = arenaY + row * m_impl->cellSize;
            SDL_RenderDrawLine(renderer, arenaX, y, arenaX + arenaWidth, y);
        }

        int healthY = arenaY + arenaHeight + 28;
        for (const auto& entity : frame.entities)
        {
            if (entity.alive && entity.hasPosition)
            {
                const int x = arenaX + std::clamp(entity.x, 0, ArenaColumns - 1) * m_impl->cellSize;
                const int y = arenaY + std::clamp(entity.y, 0, ArenaRows - 1) * m_impl->cellSize;
                const SDL_Color color = entityColor(entity.glyph);
                fillRect(renderer, x + 4, y + 4, m_impl->cellSize - 8, m_impl->cellSize - 8, color);
                drawRect(renderer,
                         x + 4,
                         y + 4,
                         m_impl->cellSize - 8,
                         m_impl->cellSize - 8,
                         SDL_Color{226, 232, 240, 255});
            }

            const int barX = 48;
            constexpr int barWidth = 260;
            constexpr int barHeight = 18;
            const SDL_Color color = entity.alive ? entityColor(entity.glyph) : SDL_Color{100, 116, 139, 255};
            fillRect(renderer, barX, healthY, barWidth, barHeight, SDL_Color{51, 65, 85, 255});
            if (entity.hasHealth)
            {
                fillRect(renderer,
                         barX,
                         healthY,
                         (barWidth * healthPercentage(entity.currentHealth, entity.maxHealth)) / 100,
                         barHeight,
                         color);
            }
            drawRect(renderer, barX, healthY, barWidth, barHeight, SDL_Color{148, 163, 184, 255});
            fillRect(renderer, barX - 28, healthY, 18, 18, color);
            healthY += 32;
        }

        SDL_RenderPresent(renderer);
    }

    bool SdlRenderBackend::shouldClose() const
    {
        return m_impl->shouldClose;
    }
}
