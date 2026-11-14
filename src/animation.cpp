/*
 * Bonjour! - typewriter reveal timing.
 *
 * MIT licensed, see LICENSE.
 */
#include "animation.hpp"

#include <algorithm>
#include <cmath>

namespace bonjour {

namespace {

/// Smoothstep easing so letters fade in softly instead of popping.
float smoothStep(float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

} // namespace

Typewriter::Typewriter(std::size_t letterCount, TypewriterTiming timing)
    : letterCount_(letterCount), timing_(timing) {}

float Typewriter::opacityOf(std::size_t index, double elapsedSeconds) const {
    if (index >= letterCount_) {
        return 0.0f;
    }

    const double start =
        timing_.startDelay + static_cast<double>(index) * timing_.letterInterval;

    if (elapsedSeconds <= start) {
        return 0.0f;
    }
    if (timing_.letterFade <= 0.0) {
        return 1.0f;
    }

    const double progress = (elapsedSeconds - start) / timing_.letterFade;
    if (progress >= 1.0) {
        return 1.0f;
    }

    return smoothStep(static_cast<float>(progress));
}

void Typewriter::update(double elapsedSeconds, float* opacities) const {
    if (opacities == nullptr) {
        return;
    }
    for (std::size_t i = 0; i < letterCount_; ++i) {
        opacities[i] = opacityOf(i, elapsedSeconds);
    }
}

std::size_t Typewriter::startedLetters(double elapsedSeconds) const {
    std::size_t started = 0;
    while (started < letterCount_) {
        const double start =
            timing_.startDelay + static_cast<double>(started) * timing_.letterInterval;
        if (elapsedSeconds <= start) {
            break;
        }
        ++started;
    }
    return started;
}

bool Typewriter::finished(double elapsedSeconds) const {
    return elapsedSeconds >= finishedAt();
}

double Typewriter::finishedAt() const {
    if (letterCount_ == 0) {
        return timing_.startDelay;
    }
    return timing_.startDelay +
           static_cast<double>(letterCount_ - 1) * timing_.letterInterval +
           timing_.letterFade;
}

} // namespace bonjour
