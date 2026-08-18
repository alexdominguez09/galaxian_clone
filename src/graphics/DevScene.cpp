#include "DevScene.hpp"

#include "DevArt.hpp"
#include "core/Constants.hpp"

namespace galaxian {

bool DevScene::initialize(Renderer& renderer)
{
    if (!DevArt::createAll(renderer)) {
        return false;
    }

    player_ = renderer.texture(DevArt::kPlayer);
    bullet_ = renderer.texture(DevArt::kBullet);
    if (player_ == nullptr || bullet_ == nullptr) {
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

    // Player sprite.
    renderer.drawSprite(*player_, {212.0f, 520.0f});

    // Projectile rectangles.
    renderer.drawFilledRect({100.0f, 400.0f, 4.0f, 10.0f}, colors::kBullet);
    renderer.drawFilledRect({222.0f, 300.0f, 4.0f, 10.0f}, colors::kBullet);
    renderer.drawFilledRect({340.0f, 450.0f, 4.0f, 10.0f}, colors::kBullet);

    // Text.
    renderer.drawText("GALAXIAN CLONE - STAGE 3 TEST SCENE", {16.0f, 16.0f},
                      colors::kWhite);
    renderer.drawText("PLAYER=TRIANGLE  ENEMIES=SQUARES", {16.0f, 40.0f},
                      colors::kWhite);
    renderer.drawText("F2=STATS  ESC=QUIT", {16.0f, 552.0f}, colors::kWhite);
}

}  // namespace galaxian
