#include "GameCore/Core/SdlInputBackend.h"

#include "GameCore/Core/InputManager.h"

#include <SDL.h>

#include <memory>
#include <stdexcept>
#include <string>

namespace GameCore::Core
{
    namespace
    {
        void setMappedKey(InputManager& input, const SDL_Keycode key, const bool isDown)
        {
            switch (key)
            {
            case SDLK_w:
            case SDLK_UP:
                input.setActionDown("MoveUp", isDown);
                break;
            case SDLK_s:
            case SDLK_DOWN:
                input.setActionDown("MoveDown", isDown);
                break;
            case SDLK_a:
            case SDLK_LEFT:
                input.setActionDown("MoveLeft", isDown);
                break;
            case SDLK_d:
            case SDLK_RIGHT:
                input.setActionDown("MoveRight", isDown);
                break;
            case SDLK_SPACE:
            case SDLK_RETURN:
                input.setActionDown("PrimaryAction", isDown);
                break;
            case SDLK_ESCAPE:
                input.setActionDown("Quit", isDown);
                break;
            default:
                break;
            }
        }
    }

    struct SdlInputBackend::Impl
    {
    };

    SdlInputBackend::SdlInputBackend()
        : m_impl(std::make_unique<Impl>())
    {
        if (SDL_WasInit(SDL_INIT_VIDEO) == 0)
        {
            if (SDL_Init(SDL_INIT_VIDEO) != 0)
            {
                throw std::runtime_error(std::string("SDL_Init failed: ") + SDL_GetError());
            }
        }
    }

    SdlInputBackend::~SdlInputBackend() = default;

    void SdlInputBackend::process(InputManager& input)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event) != 0)
        {
            switch (event.type)
            {
            case SDL_QUIT:
                input.setActionDown("Quit", true);
                break;
            case SDL_KEYDOWN:
                if (event.key.repeat == 0)
                {
                    setMappedKey(input, event.key.keysym.sym, true);
                }
                break;
            case SDL_KEYUP:
                setMappedKey(input, event.key.keysym.sym, false);
                break;
            default:
                break;
            }
        }
    }
}
