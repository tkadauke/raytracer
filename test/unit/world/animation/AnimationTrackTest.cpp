#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <functional>
#include <stdexcept>

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>

#include "core/Color.h"
#include "core/math/Angle.h"
#include "core/math/Vector.h"
#include "world/animation/AnimationTrack.h"
#include "world/objects/Material.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"
#include "world/objects/Sphere.h"
#include "world/objects/Texture.h"

Q_DECLARE_METATYPE(Vector3d);
Q_DECLARE_METATYPE(Angled);
Q_DECLARE_METATYPE(Colord);

namespace AnimationTrackTest {
  using core::math::interpolation::InterpolationMode;
  using ::testing::HasSubstr;

  struct MetaTypeRegistrar {
    MetaTypeRegistrar() {
      qRegisterMetaType<Vector3d>();
      qRegisterMetaType<Angled>();
      qRegisterMetaType<Colord>();
      qRegisterMetaType<Material*>();
      qRegisterMetaType<Texture*>();
    }
  };
  static const MetaTypeRegistrar s_registrar;

  QJsonValue vectorValue(double x, double y, double z) {
    return QJsonValue(QJsonArray({ x, y, z }));
  }

  QJsonValue colorValue(double r, double g, double b) {
    return QJsonValue(QJsonArray({ r, g, b }));
  }

  QJsonObject keyJson(int frame, const QJsonValue& value) {
    QJsonObject json;
    json["frame"] = frame;
    json["value"] = value;
    return json;
  }

  QJsonObject trackJson(const QString& target,
                        const QString& property,
                        const QJsonArray& keys,
                        const QString& interpolation = "linear") {
    QJsonObject json;
    json["target"] = target;
    json["property"] = property;
    json["interpolation"] = interpolation;
    json["keys"] = keys;
    return json;
  }

  void expectInvalidArgumentWithSubstring(const std::function<void()>& action, const std::string& message) {
    try {
      action();
      FAIL() << "expected std::invalid_argument";
    } catch (const std::invalid_argument& error) {
      EXPECT_THAT(error.what(), HasSubstr(message));
    }
  }

  void expectRuntimeErrorWithSubstring(const std::function<void()>& action, const std::string& message) {
    try {
      action();
      FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& error) {
      EXPECT_THAT(error.what(), HasSubstr(message));
    }
  }

  TEST(AnimationTrack, SortsKeyframesByFrame) {
    world::AnimationTrack track(
      "target-id",
      "position",
      {
        { 10, vectorValue(10.0, 0.0, 0.0) },
        { 1, vectorValue(1.0, 0.0, 0.0) },
      });

    ASSERT_EQ(2u, track.keyframes().size());
    EXPECT_EQ(1, track.keyframes()[0].frame);
    EXPECT_EQ(10, track.keyframes()[1].frame);
  }

  TEST(AnimationTrack, RejectsInvalidConstructionArguments) {
    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack("", "position", { { 1, vectorValue(0.0, 0.0, 0.0) } });
    }, "target must not be empty");

    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack("target-id", "", { { 1, vectorValue(0.0, 0.0, 0.0) } });
    }, "property must not be empty");

    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack("target-id", "material.color", { { 1, colorValue(1.0, 0.0, 0.0) } });
    }, "direct property name");

    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack("target-id", "position", {});
    }, "at least one keyframe");

    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack(
        "target-id",
        "position",
        {
          { 1, vectorValue(0.0, 0.0, 0.0) },
          { 1, vectorValue(1.0, 0.0, 0.0) },
        });
    }, "duplicate");
  }

  TEST(AnimationTrack, ReadsAndWritesSerializedTrackJson) {
    const auto track = world::AnimationTrack::read(trackJson(
      "camera-id",
      "target",
      QJsonArray({
        keyJson(1, vectorValue(0.0, 0.0, 0.0)),
        keyJson(10, vectorValue(0.0, 0.0, -1.0)),
      }),
      "smoothstep"));

    EXPECT_EQ(QString("camera-id"), track.targetId());
    EXPECT_EQ(QString("target"), track.propertyName());
    EXPECT_EQ(InterpolationMode::SmoothStep, track.interpolationMode());

    QJsonObject written;
    track.write(written);

    EXPECT_EQ(QString("camera-id"), written["target"].toString());
    EXPECT_EQ(QString("target"), written["property"].toString());
    EXPECT_EQ(QString("smoothstep"), written["interpolation"].toString());
    ASSERT_TRUE(written["keys"].isArray());
    EXPECT_EQ(2, written["keys"].toArray().size());
  }

  TEST(AnimationTrack, DefaultsMissingInterpolationToLinear) {
    QJsonObject json;
    json["target"] = "sphere-id";
    json["property"] = "radius";
    json["keys"] = QJsonArray({
      keyJson(1, QJsonValue(1.0)),
      keyJson(10, QJsonValue(10.0)),
    });

    const auto track = world::AnimationTrack::read(json);

    EXPECT_EQ(InterpolationMode::Linear, track.interpolationMode());
  }

  TEST(AnimationTrack, RejectsMalformedSerializedTrackJson) {
    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack::read(trackJson("", "position", QJsonArray({ keyJson(1, vectorValue(0.0, 0.0, 0.0)) })));
    }, "target");

    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack::read(trackJson("target-id", "", QJsonArray({ keyJson(1, vectorValue(0.0, 0.0, 0.0)) })));
    }, "property");

    expectInvalidArgumentWithSubstring([] {
      QJsonObject json = trackJson("target-id", "position", QJsonArray({ keyJson(1, vectorValue(0.0, 0.0, 0.0)) }));
      json["interpolation"] = "not-a-mode";
      world::AnimationTrack::read(json);
    }, "unsupported interpolation mode");

    expectInvalidArgumentWithSubstring([] {
      QJsonObject json = trackJson("target-id", "position", QJsonArray());
      json["keys"] = "not-array";
      world::AnimationTrack::read(json);
    }, "keys");

    expectInvalidArgumentWithSubstring([] {
      world::AnimationTrack::read(trackJson("target-id", "position", QJsonArray({ QJsonValue(3.0) })));
    }, "keys must be objects");

    expectInvalidArgumentWithSubstring([] {
      QJsonObject key;
      key["frame"] = 1;
      world::AnimationTrack::read(trackJson("target-id", "position", QJsonArray({ key })));
    }, "missing field 'value'");

    expectInvalidArgumentWithSubstring([] {
      QJsonObject key = keyJson(1, vectorValue(0.0, 0.0, 0.0));
      key["frame"] = 1.5;
      world::AnimationTrack::read(trackJson("target-id", "position", QJsonArray({ key })));
    }, "frame");
  }

  TEST(AnimationTrack, SamplesDoubleProperties) {
    Sphere sphere;
    const world::AnimationTrack track(
      "sphere-id",
      "radius",
      {
        { 1, QJsonValue(1.0) },
        { 11, QJsonValue(11.0) },
      });

    EXPECT_DOUBLE_EQ(6.0, track.sample(sphere, 6).toDouble());
  }

  TEST(AnimationTrack, SamplesVectorProperties) {
    PinholeCamera camera;
    const world::AnimationTrack track(
      "camera-id",
      "position",
      {
        { 1, vectorValue(0.0, 0.0, 0.0) },
        { 11, vectorValue(10.0, 0.0, 0.0) },
      });

    EXPECT_EQ(Vector3d(5.0, 0.0, 0.0), track.sample(camera, 6).value<Vector3d>());
  }

  TEST(AnimationTrack, SamplesColorProperties) {
    Scene scene;
    scene.setId("scene-id");
    const world::AnimationTrack track(
      "scene-id",
      "background",
      {
        { 1, colorValue(0.0, 0.0, 0.0) },
        { 11, colorValue(1.0, 0.5, 0.0) },
      });

    EXPECT_EQ(Colord(0.5, 0.25, 0.0), track.sample(scene, 6).value<Colord>());
  }

  TEST(AnimationTrack, SamplesStepOnlyBoolProperties) {
    Sphere sphere;
    const world::AnimationTrack track(
      "sphere-id",
      "visible",
      {
        { 1, QJsonValue(true) },
        { 10, QJsonValue(false) },
      },
      InterpolationMode::Step);

    EXPECT_TRUE(track.sample(sphere, 9).toBool());
    EXPECT_FALSE(track.sample(sphere, 10).toBool());
  }

  TEST(AnimationTrack, ClampsBeforeFirstAndAfterLastKey) {
    Sphere sphere;
    const world::AnimationTrack track(
      "sphere-id",
      "radius",
      {
        { 5, QJsonValue(5.0) },
        { 10, QJsonValue(10.0) },
      });

    EXPECT_DOUBLE_EQ(5.0, track.sample(sphere, 1).toDouble());
    EXPECT_DOUBLE_EQ(10.0, track.sample(sphere, 20).toDouble());
  }

  TEST(AnimationTrack, AppliesSmoothStepInterpolation) {
    Sphere sphere;
    const world::AnimationTrack track(
      "sphere-id",
      "radius",
      {
        { 1, QJsonValue(0.0) },
        { 5, QJsonValue(10.0) },
      },
      InterpolationMode::SmoothStep);

    EXPECT_DOUBLE_EQ(1.5625, track.sample(sphere, 2).toDouble());
  }

  TEST(AnimationTrack, AppliesSampledValueToTargetElement) {
    Scene scene;
    auto* sphere = new Sphere;
    sphere->setId("sphere-id");
    scene.addChild(sphere);
    const world::AnimationTrack track(
      "sphere-id",
      "radius",
      {
        { 1, QJsonValue(1.0) },
        { 11, QJsonValue(11.0) },
      });

    track.apply(scene, 6);

    EXPECT_DOUBLE_EQ(6.0, sphere->radius());
  }

  TEST(AnimationTrack, MissingTargetFailsClearly) {
    Scene scene;
    const world::AnimationTrack track(
      "missing-id",
      "position",
      {
        { 1, vectorValue(0.0, 0.0, 0.0) },
        { 10, vectorValue(1.0, 0.0, 0.0) },
      });

    expectRuntimeErrorWithSubstring([&] {
      track.apply(scene, 1);
    }, "target element was not found");
  }

  TEST(AnimationTrack, MissingPropertyFailsClearly) {
    PinholeCamera camera;
    const world::AnimationTrack track(
      "camera-id",
      "notAProperty",
      {
        { 1, vectorValue(0.0, 0.0, 0.0) },
        { 10, vectorValue(1.0, 0.0, 0.0) },
      });

    expectRuntimeErrorWithSubstring([&] {
      static_cast<void>(track.sample(camera, 1));
    }, "target property does not exist");
  }

  TEST(AnimationTrack, UnsupportedPropertyTypeFailsClearly) {
    Scene scene;
    const world::AnimationTrack track(
      "scene-id",
      "name",
      {
        { 1, QJsonValue("a") },
        { 10, QJsonValue("b") },
      },
      InterpolationMode::Step);

    expectRuntimeErrorWithSubstring([&] {
      static_cast<void>(track.sample(scene, 1));
    }, "unsupported property type 'QString'");
  }

  TEST(AnimationTrack, MalformedDoubleKeyValueFailsClearly) {
    Sphere sphere;
    const world::AnimationTrack track(
      "sphere-id",
      "radius",
      {
        { 1, QJsonValue(1.0) },
        { 10, QJsonValue("not-number") },
      });

    expectRuntimeErrorWithSubstring([&] {
      static_cast<void>(track.sample(sphere, 5));
    }, "double key values must be numbers");
  }

  TEST(AnimationTrack, MalformedVectorKeyValueFailsClearly) {
    PinholeCamera camera;
    const world::AnimationTrack track(
      "camera-id",
      "position",
      {
        { 1, vectorValue(0.0, 0.0, 0.0) },
        { 10, QJsonValue(QJsonArray({ 1.0, 2.0 })) },
      });

    expectRuntimeErrorWithSubstring([&] {
      static_cast<void>(track.sample(camera, 5));
    }, "exactly three elements");
  }

  TEST(AnimationTrack, MalformedBoolKeyValueFailsClearly) {
    Sphere sphere;
    const world::AnimationTrack track(
      "sphere-id",
      "visible",
      {
        { 1, QJsonValue(true) },
        { 10, QJsonValue("false") },
      },
      InterpolationMode::Step);

    expectRuntimeErrorWithSubstring([&] {
      static_cast<void>(track.sample(sphere, 5));
    }, "bool key values must be booleans");
  }

  TEST(AnimationTrack, LinearBoolTrackFailsClearly) {
    Sphere sphere;
    const world::AnimationTrack track(
      "sphere-id",
      "visible",
      {
        { 1, QJsonValue(true) },
        { 10, QJsonValue(false) },
      },
      InterpolationMode::Linear);

    expectRuntimeErrorWithSubstring([&] {
      static_cast<void>(track.sample(sphere, 5));
    }, "bool properties support only step interpolation");
  }
}
