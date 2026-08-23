#include "Animation.hpp"

#include <cmath>

namespace galaxian {
namespace animation {

void Animator::play(const AnimationClip& clip)
{
    clip_ = &clip;
    time_ = 0.0;
}

void Animator::update(double dt)
{
    if (!valid() || dt <= 0.0 || clip_->frameCount() <= 0) {
        return;
    }
    time_ += dt;

    const double total = clip_->duration();
    if (total <= 0.0) {
        return;
    }
    if (clip_->loops()) {
        // Wrap modulo one pass: the clock stays bounded forever.
        time_ = std::fmod(time_, total);
    } else if (time_ > total) {
        time_ = total;  // one-shot clamps to the final frame
    }
}

int Animator::frame() const
{
    if (!valid() || clip_->frameCount() <= 0 ||
        clip_->frameSeconds() <= 0.0) {
        return 0;
    }
    int index =
        static_cast<int>(time_ / clip_->frameSeconds());
    const int count = clip_->frameCount();
    if (clip_->loops()) {
        index %= count;
    } else if (index >= count) {
        index = count - 1;
    }
    return index;
}

bool Animator::finished() const
{
    if (!valid() || clip_->loops() || clip_->frameCount() <= 0) {
        return false;
    }
    return time_ >= clip_->duration();
}

void Animator::draw(Renderer& renderer, Vector2 position) const
{
    if (!valid()) {
        return;
    }
    const Texture* texture =
        renderer.texture(clip_->textureId(frame()));
    if (texture != nullptr) {
        renderer.drawSprite(*texture, position);
    }
}

}  // namespace animation
}  // namespace galaxian
