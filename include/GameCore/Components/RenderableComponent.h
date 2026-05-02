#pragma once

namespace GameCore::Components
{
    struct RenderableComponent
    {
        char glyph{'?'};
        int layer{0};
        bool visible{true};
    };
}
