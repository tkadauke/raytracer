#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>
#include <vector>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "core/math/Vector.h"
#include "world/animation/AnimationTrack.h"
#include "world/animation/Timeline.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

namespace TimelineTest {
  using ::testing::HasSubstr;

  QJsonValue vectorValue(double x, double y, double z) {
    return QJsonValue(QJsonArray({x, y, z}));
  }

  QJsonObject keyJson(int frame, const QJsonValue& value) {
    QJsonObject json;
    json["frame"] = frame;
    json["value"] = value;
    return json;
  }

  QJsonObject trackJson(const QString& target, const QString& property, const QJsonArray& keys,
                        const QString& interpolation = "linear") {
    QJsonObject json;
    json["target"] = target;
    json["property"] = property;
    json["interpolation"] = interpolation;
    json["keys"] = keys;
    return json;
  }

  QJsonObject timelineJson(const QJsonArray& tracks) {
    QJsonObject json;
    json["startFrame"] = 1;
    json["endFrame"] = 10;
    json["fps"] = 24.0;
    json["tracks"] = tracks;
    return json;
  }

  void expectInvalidArgumentWithSubstring(const std::function<void()>& action,
                                          const std::string& message) {
    try {
      action();
      FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr(message));
    }
  }

  TEST(Timeline, ReadsAndWritesSerializedTimelineJson) {
    const auto timeline = world::Timeline::read(
      timelineJson(QJsonArray({trackJson("camera-id", "position",
                                         QJsonArray({
                                           keyJson(1, vectorValue(0.0, 0.0, 0.0)),
                                           keyJson(10, vectorValue(10.0, 0.0, 0.0)),
                                         }))})));

    EXPECT_EQ(1, timeline.startFrame());
    EXPECT_EQ(10, timeline.endFrame());
    EXPECT_DOUBLE_EQ(24.0, timeline.fps());
    ASSERT_EQ(1u, timeline.tracks().size());

    QJsonObject written;
    timeline.write(written);

    EXPECT_EQ(1, written["startFrame"].toInt());
    EXPECT_EQ(10, written["endFrame"].toInt());
    EXPECT_DOUBLE_EQ(24.0, written["fps"].toDouble());
    ASSERT_TRUE(written["tracks"].isArray());
    EXPECT_EQ(1, written["tracks"].toArray().size());
  }

  TEST(Timeline, RejectsMalformedSerializedTimelineJson) {
    expectInvalidArgumentWithSubstring(
      [] {
        QJsonObject json = timelineJson(QJsonArray());
        json["startFrame"] = 1.25;
        world::Timeline::read(json);
      },
      "startFrame");

    expectInvalidArgumentWithSubstring(
      [] {
        QJsonObject json = timelineJson(QJsonArray());
        json["endFrame"] = "10";
        world::Timeline::read(json);
      },
      "endFrame");

    expectInvalidArgumentWithSubstring(
      [] {
        QJsonObject json = timelineJson(QJsonArray());
        json["fps"] = "24";
        world::Timeline::read(json);
      },
      "fps");

    expectInvalidArgumentWithSubstring(
      [] {
        QJsonObject json = timelineJson(QJsonArray());
        json["endFrame"] = 0;
        world::Timeline::read(json);
      },
      "end frame");

    expectInvalidArgumentWithSubstring(
      [] {
        QJsonObject json = timelineJson(QJsonArray());
        json["fps"] = 0.0;
        world::Timeline::read(json);
      },
      "fps");

    expectInvalidArgumentWithSubstring(
      [] {
        QJsonObject json = timelineJson(QJsonArray());
        json["tracks"] = "not-array";
        world::Timeline::read(json);
      },
      "tracks");

    expectInvalidArgumentWithSubstring(
      [] { world::Timeline::read(timelineJson(QJsonArray({QJsonValue(4.0)}))); },
      "tracks must be objects");
  }

  TEST(Timeline, AppliesAllTracksToScene) {
    Scene scene;
    auto* camera = new PinholeCamera;
    camera->setId("camera-id");
    scene.addChild(camera);

    const world::Timeline timeline(1, 11, 24.0,
                                   std::vector<world::AnimationTrack>({
                                     world::AnimationTrack("camera-id", "position",
                                                           {
                                                             {1, vectorValue(0.0, 0.0, 0.0)},
                                                             {11, vectorValue(10.0, 0.0, 0.0)},
                                                           }),
                                     world::AnimationTrack("camera-id", "target",
                                                           {
                                                             {1, vectorValue(0.0, 0.0, -1.0)},
                                                             {11, vectorValue(0.0, 0.0, -11.0)},
                                                           }),
                                   }));

    timeline.apply(scene, 6);

    EXPECT_EQ(Vector3d(5.0, 0.0, 0.0), camera->position());
    EXPECT_EQ(Vector3d(0.0, 0.0, -6.0), camera->target());
  }
}
