#pragma once

#include <vector>

#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <QVariant>

#include "core/math/interpolation/Interpolation.h"
#include "render/animation/AnimationTrack.h"

class Element;

namespace world {

  /**
  * How a world animation track can be consumed by render engines.
  */
  enum class AnimationTrackClass { RuntimeContinuous, FrameBaked, StepOnly, Rejected };

  /**
  * Classification result for one world animation track.
  */
  struct AnimationTrackClassification {
    AnimationTrackClass trackClass{AnimationTrackClass::Rejected};
    QString diagnostic;
  };

  /**
  * One serialized keyframe on a world animation track.
  *
  * Values stay in JSON form until evaluation, because the target property's
  * `Q_PROPERTY` type determines how the value should be decoded.
  */
  struct AnimationKeyframe {
    /**
    * Exact integer frame where @p value is keyed.
    */
    int frame;

    /**
    * Serialized key value.
    */
    QJsonValue value;
  };

  /**
  * Animation track targeting one editable property on one world element.
  *
  * The target is addressed by stable element id and a direct property name. Most
  * tracks target direct `Q_PROPERTY`s; source-backed assets can also expose
  * importer-defined dynamic properties through `Element::animationPropertyInfo`.
  * Nested property paths are intentionally not accepted by this first world
  * timeline layer.
  */
  class AnimationTrack {
  public:
    /**
    * Creates a world animation track.
    *
    * @param targetId id of the world element whose property is animated.
    * @param propertyName direct animatable property name.
    * @param keyframes keyed JSON values, sorted by frame during construction.
    * @param interpolationMode mode used between adjacent keyframes.
    * @throws std::invalid_argument if the target, property, or keyframes are
    *   invalid.
    */
    AnimationTrack(QString targetId, QString propertyName, std::vector<AnimationKeyframe> keyframes,
                   core::math::interpolation::InterpolationMode interpolationMode =
                     core::math::interpolation::InterpolationMode::Linear);

    /**
    * Reads a track from its scene-file JSON representation.
    *
    * @throws std::invalid_argument if required fields are missing or invalid.
    */
    static AnimationTrack read(const QJsonObject& json);

    /**
    * Writes the track to its scene-file JSON representation.
    */
    void write(QJsonObject& json) const;

    /**
    * @returns the target element id.
    */
    [[nodiscard]] const QString& targetId() const noexcept;

    /**
    * @returns the direct target property name.
    */
    [[nodiscard]] const QString& propertyName() const noexcept;

    /**
    * @returns the interpolation mode used between keyframes.
    */
    [[nodiscard]] core::math::interpolation::InterpolationMode interpolationMode() const noexcept;

    /**
    * @returns sorted keyframes owned by the track.
    */
    [[nodiscard]] const std::vector<AnimationKeyframe>& keyframes() const noexcept;

    /**
    * Classifies this track for runtime compilation against @p target.
    *
    * Runtime-continuous tracks can be converted to render-side continuous-time
    * tracks. Frame-baked tracks must still be evaluated on ordinary integer
    * frames before world-to-render conversion. Step-only tracks can be sampled
    * only at key boundaries or as discrete switches. Rejected tracks have clear
    * diagnostics and should not be compiled.
    */
    [[nodiscard]] AnimationTrackClassification classify(const Element& target) const;

    /**
    * Compiles this world track into a Qt-free render animation track.
    *
    * The target property metadata selects the same value decoder used by
    * `sample()`, but key times remain continuous doubles so render code can
    * sample subframes without going back through the editable world scene.
    *
    * @throws std::runtime_error if the property is missing, unsupported, or
    *   cannot be represented by render-time animation values.
    */
    [[nodiscard]] render::animation::AnimationTrack toRenderTrack(const Element& target) const;

    /**
    * Samples this track for @p target at @p frame.
    *
    * The target property's animation metadata selects the supported value
    * decoder. Linear and smoothstep sampling are supported for `double`, `int`,
    * `Vector3d`, and `Colord`. `bool` and string-like source parameters are
    * step-only.
    *
    * @throws std::runtime_error if the property is missing, unsupported, or
    *   cannot be sampled with this track's interpolation mode.
    */
    [[nodiscard]] QVariant sample(const Element& target, int frame) const;

    /**
    * Finds the target element under @p root, samples the track, and writes the
    * sampled value back to the target property.
    *
    * @throws std::runtime_error if the target cannot be found or the sampled
    *   value cannot be applied.
    */
    void apply(Element& root, int frame) const;

  private:
    QString m_targetId;
    QString m_propertyName;
    std::vector<AnimationKeyframe> m_keyframes;
    core::math::interpolation::InterpolationMode m_interpolationMode;
  };

} // namespace world
