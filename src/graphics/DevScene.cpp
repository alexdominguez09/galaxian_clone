#include "DevScene.hpp"

#include "DevArt.hpp"
#include "core/Constants.hpp"

namespace galaxian {

bool DevScene::initialize(Renderer& renderer)
{
    return DevArt::createAll(renderer);
}

void DevScene::draw(Renderer& renderer) const
{
    // Background + screen border.
    renderer.clear(colors::kBlack);
    renderer.drawRect(
        {0.0f, 0.0f, static_cast<float>(kLogicalWidth),
         static_cast<float>(kLogicalHeight)},
        colors::kBorder);

    // The Stage 3/4 scaffolding (static bullets, action table, per-stage
    // titles, and the controls help line) is all gone: the top bar is the
    // real HUD (Stage 18, graphics/Hud), the enemies/player/bullets are the
    // gameplay objects, and v1.0 ships without the dev controls line
    // (SPACE=FIRE / F1=BOXES / F2=STATS / F3=DIVE) on the play screen.
}

}  // namespace galaxian
