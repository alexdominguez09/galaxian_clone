#include "DevScene.hpp"

#include <string>

#include "DevArt.hpp"
#include "core/Constants.hpp"
#include "input/InputManager.hpp"

namespace galaxian {

bool DevScene::initialize(Renderer& renderer)
{
    if (!DevArt::createAll(renderer)) {
        return false;
    }

    // 10 enemies: two rows of five, cycling through the three types.
    const char* types[] = {DevArt::kEnemyCommander,
                           DevArt::kEnemyGuard,
                           DevArt::kEnemyScout};
    int index = 0;
    for (int row = 0; row < 2; ++row) {
        for (int col = 0; col < 5; ++col) {
            EnemySprite& enemy = enemies_[index++];
            enemy.texture = renderer.texture(types[(row + col) % 3]);
            enemy.position = {static_cast<float>(80 + col * 50),
                              static_cast<float>(60 + row * 40)};
        }
    }
    return true;
}

void DevScene::update(const InputManager& input)
{
    // Snapshot the action state for the on-screen table (drawn by draw()).
    // The Stage 4 demo player movement and fire flash were here; the real
    // Player is now simulated by Game in the fixed-timestep loop.
    for (int i = 0; i < kActionCount; ++i) {
        const Action action = static_cast<Action>(i);
        actionRows_[i].held = input.isHeld(action);
        actionRows_[i].pressed = input.wasPressed(action);
        actionRows_[i].released = input.wasReleased(action);
    }
}

void DevScene::draw(Renderer& renderer) const
{
    // Background + screen border.
    renderer.clear(colors::kBlack);
    renderer.drawRect(
        {0.0f, 0.0f, static_cast<float>(kLogicalWidth),
         static_cast<float>(kLogicalHeight)},
        colors::kBorder);

    // Enemy sprites.
    for (const EnemySprite& enemy : enemies_) {
        if (enemy.texture != nullptr) {
            renderer.drawSprite(*enemy.texture, enemy.position);
        }
    }

    // Projectile rectangles (Stage 3 static test shapes).
    renderer.drawFilledRect({100.0f, 400.0f, 4.0f, 10.0f}, colors::kBullet);
    renderer.drawFilledRect({222.0f, 300.0f, 4.0f, 10.0f}, colors::kBullet);
    renderer.drawFilledRect({340.0f, 450.0f, 4.0f, 10.0f}, colors::kBullet);

    // Stage 4: live action-state table (dev aid for manual verification).
    // Shows the held/pressed/released state of every named Action so the
    // input layer can be verified by eye.
    const float tableX = 16.0f;
    const float heldX = 190.0f;
    const float pressX = 240.0f;
    const float relX = 285.0f;
    renderer.drawText("ACTION", {tableX, 140.0f}, colors::kBorder, 14);
    renderer.drawText("HELD", {heldX, 140.0f}, colors::kBorder, 14);
    renderer.drawText("PRS", {pressX, 140.0f}, colors::kBorder, 14);
    renderer.drawText("REL", {relX, 140.0f}, colors::kBorder, 14);
    for (int i = 0; i < kActionCount; ++i) {
        const float y = 160.0f + static_cast<float>(i) * 18.0f;
        renderer.drawText(std::string(actionName(static_cast<Action>(i))),
                          {tableX, y}, colors::kWhite, 14);
        const ActionRow& row = actionRows_[i];
        renderer.drawText(row.held ? "Y" : "-", {heldX, y},
                          row.held ? colors::kGreen : colors::kBorder, 14);
        renderer.drawText(row.pressed ? "Y" : "-", {pressX, y},
                          row.pressed ? colors::kEnemyYellow : colors::kBorder,
                          14);
        renderer.drawText(row.released ? "Y" : "-", {relX, y},
                          row.released ? colors::kEnemyRed : colors::kBorder,
                          14);
    }

    // Text.
    renderer.drawText("GALAXIAN CLONE - STAGE 7 COLLISION", {16.0f, 16.0f},
                      colors::kWhite);
    renderer.drawText("PLAYER=TRIANGLE  ENEMIES=SQUARES", {16.0f, 40.0f},
                      colors::kWhite);
    renderer.drawText(
        "LEFT/RIGHT=MOVE  SPACE=FIRE  F1=BOXES  F2=STATS  ESC=QUIT",
        {16.0f, 552.0f}, colors::kWhite);
}

}  // namespace galaxian
