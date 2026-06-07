#pragma once

#include <vector>

#include <QJsonObject>

#include "core/animation/Timeline.h"
#include "world/animation/AnimationTrack.h"

class Scene;

namespace world {

  /**
  * World-scene animation timeline.
  *
  * The world timeline owns the scene frame range and a set of property tracks
  * that can be evaluated against editable `world` objects before conversion to
  * runtime render objects.
  */
  class Timeline {
  public:
    /**
    * Creates a world timeline from a core frame timeline and property tracks.
    *
    * @param timeline frame range and fps used by the animation.
    * @param tracks property tracks evaluated for each frame.
    */
    explicit Timeline(core::animation::Timeline timeline, std::vector<AnimationTrack> tracks = {});

    /**
    * Creates a world timeline from explicit frame range values.
    *
    * @param startFrame first frame in the animation range.
    * @param endFrame last frame in the animation range.
    * @param fps number of frames per second.
    * @param tracks property tracks evaluated for each frame.
    */
    Timeline(int startFrame, int endFrame, double fps, std::vector<AnimationTrack> tracks = {});

    /**
    * Reads a world timeline from a top-level scene `animation` block.
    *
    * @throws std::invalid_argument if required fields are missing or invalid.
    */
    static Timeline read(const QJsonObject& json);

    /**
    * Writes this timeline as a top-level scene `animation` block.
    */
    void write(QJsonObject& json) const;

    /**
    * @returns the shared core timeline.
    */
    [[nodiscard]] const core::animation::Timeline& coreTimeline() const noexcept;

    /**
    * @returns the first frame in the inclusive animation range.
    */
    [[nodiscard]] int startFrame() const noexcept;

    /**
    * @returns the last frame in the inclusive animation range.
    */
    [[nodiscard]] int endFrame() const noexcept;

    /**
    * @returns the number of frames per second.
    */
    [[nodiscard]] double fps() const noexcept;

    /**
    * @returns property tracks owned by this timeline.
    */
    [[nodiscard]] const std::vector<AnimationTrack>& tracks() const noexcept;

    /**
    * Classifies every track against @p scene.
    *
    * Tracks whose target id cannot be resolved are returned as rejected with a
    * diagnostic. Result order matches `tracks()`.
    */
    [[nodiscard]] std::vector<AnimationTrackClassification>
    classifyTracks(const Scene& scene) const;

    /**
    * Applies every track to @p scene at @p frame.
    *
    * Tracks clamp outside their own keyframe ranges. The timeline frame range
    * is preserved for serialization and CLI defaults; it does not prevent
    * explicit evaluation outside that range.
    */
    void apply(Scene& scene, int frame) const;

  private:
    core::animation::Timeline m_timeline;
    std::vector<AnimationTrack> m_tracks;
  };

} // namespace world
