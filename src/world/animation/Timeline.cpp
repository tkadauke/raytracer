#include "world/animation/Timeline.h"

#include <cmath>
#include <stdexcept>
#include <utility>

#include <QJsonArray>
#include <QString>

#include "world/objects/Scene.h"

namespace {

  std::invalid_argument invalidTimeline(const QString& message) {
    return std::invalid_argument(message.toStdString());
  }

  int requiredInt(const QJsonObject& json, const QString& name) {
    const auto value = json[name];
    if (!value.isDouble())
      throw invalidTimeline(QString("animation field '%1' must be an integer").arg(name));

    const auto numeric = value.toDouble();
    if (std::floor(numeric) != numeric)
      throw invalidTimeline(QString("animation field '%1' must be an integer").arg(name));

    return static_cast<int>(numeric);
  }

  double requiredDouble(const QJsonObject& json, const QString& name) {
    const auto value = json[name];
    if (!value.isDouble())
      throw invalidTimeline(QString("animation field '%1' must be a number").arg(name));
    return value.toDouble();
  }

  std::vector<world::AnimationTrack> readTracks(const QJsonObject& json) {
    const auto tracksValue = json["tracks"];
    if (!tracksValue.isArray())
      throw invalidTimeline("animation field 'tracks' must be an array");

    std::vector<world::AnimationTrack> tracks;
    const auto trackArray = tracksValue.toArray();
    tracks.reserve(static_cast<size_t>(trackArray.size()));
    for (const auto& trackValue : trackArray) {
      if (!trackValue.isObject())
        throw invalidTimeline("animation tracks must be objects");
      tracks.push_back(world::AnimationTrack::read(trackValue.toObject()));
    }
    return tracks;
  }

} // namespace

namespace world {

  Timeline::Timeline(core::animation::Timeline timeline, std::vector<AnimationTrack> tracks)
      : m_timeline(timeline),
        m_tracks(std::move(tracks)) {
  }

  Timeline::Timeline(int startFrame, int endFrame, double fps, std::vector<AnimationTrack> tracks)
      : Timeline(core::animation::Timeline(startFrame, endFrame, fps), std::move(tracks)) {
  }

  Timeline Timeline::read(const QJsonObject& json) {
    return Timeline(requiredInt(json, "startFrame"), requiredInt(json, "endFrame"),
                    requiredDouble(json, "fps"), readTracks(json));
  }

  void Timeline::write(QJsonObject& json) const {
    json["startFrame"] = startFrame();
    json["endFrame"] = endFrame();
    json["fps"] = fps();

    QJsonArray tracks;
    for (const auto& track : m_tracks) {
      QJsonObject trackObject;
      track.write(trackObject);
      tracks.append(trackObject);
    }
    json["tracks"] = tracks;
  }

  const core::animation::Timeline& Timeline::coreTimeline() const noexcept {
    return m_timeline;
  }

  int Timeline::startFrame() const noexcept {
    return m_timeline.startFrame();
  }

  int Timeline::endFrame() const noexcept {
    return m_timeline.endFrame();
  }

  double Timeline::fps() const noexcept {
    return m_timeline.fps();
  }

  const std::vector<AnimationTrack>& Timeline::tracks() const noexcept {
    return m_tracks;
  }

  void Timeline::apply(Scene& scene, int frame) const {
    for (const auto& track : m_tracks) {
      track.apply(scene, frame);
    }
  }

} // namespace world
