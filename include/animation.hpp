/*
 * Bonjour! - typewriter reveal timing.
 *
 * MIT licensed, see LICENSE. Part of the Bonjour! Nintendo Switch homebrew.
 */
#pragma once

#include <cstddef>
#include <vector>

namespace bonjour {

/// Timing of the typewriter effect, all values in seconds.
struct TypewriterTiming {
    /// Black screen shown before the very first letter appears.
    double startDelay = 0.60;
    /// Delay between two consecutive letters.
    double letterInterval = 0.12;
    /// Duration of a single letter's fade-in.
    double letterFade = 0.24;
};

/// Computes the visibility of every letter of a text over time.
///
/// The animation is purely a function of elapsed time, so it is deterministic
/// and can be evaluated at any moment (dropped frames never desynchronise it).
class Typewriter {
public:
    Typewriter(std::size_t letterCount, TypewriterTiming timing = TypewriterTiming{});

    /// Writes `letterCount()` opacity values in [0, 1] for the given elapsed
    /// time. Letters that have not started yet get 0.
    void update(double elapsedSeconds, float* opacities) const;

    /// Number of letters that have begun to appear.
    std::size_t startedLetters(double elapsedSeconds) const;

    /// True once every letter is fully opaque.
    bool finished(double elapsedSeconds) const;

    /// Moment at which the last letter becomes fully opaque.
    double finishedAt() const;

    /// Moment at which the first letter starts to appear.
    double startsAt() const { return timing_.startDelay; }

    std::size_t letterCount() const { return letterCount_; }
    const TypewriterTiming& timing() const { return timing_; }

    /// Opacity of letter `index` at `elapsedSeconds`.
    float opacityOf(std::size_t index, double elapsedSeconds) const;

private:
    std::size_t letterCount_;
    TypewriterTiming timing_;
};

} // namespace bonjour
