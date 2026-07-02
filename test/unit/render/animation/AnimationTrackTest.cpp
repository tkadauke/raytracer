#include <gtest/gtest.h>

#include <limits>
#include <string>
#include <vector>

#include "core/animation/AnimationTrack.h"
#include "core/Color.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"
#include "core/math/interpolation/Interpolation.h"
#include "render/animation/AnimationTrack.h"
#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/MatrixTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

using core::math::interpolation::InterpolationMode;
using render::animation::AnimationTrack;
using render::animation::AnimationValue;
using render::animation::Keyframe;

TEST(RenderAnimationValueTest, ReportsExplicitSupportedTypes) {
  EXPECT_TRUE(AnimationValue::supports<double>());
  EXPECT_TRUE(AnimationValue::supports<int>());
  EXPECT_TRUE(AnimationValue::supports<Vector3d>());
  EXPECT_TRUE(AnimationValue::supports<Colord>());
  EXPECT_TRUE(AnimationValue::supports<Matrix4d>());
  EXPECT_TRUE(AnimationValue::supports<bool>());
  EXPECT_TRUE(AnimationValue::supports<std::string>());
  EXPECT_FALSE(AnimationValue::supports<std::vector<double>>());

  EXPECT_THROW(static_cast<void>(AnimationValue::from(std::vector<double>{1.0})),
               std::invalid_argument);
}

TEST(RenderAnimationTrackTest, RequiresAtLeastOneKeyframe) {
  EXPECT_THROW(AnimationTrack({}), std::invalid_argument);
}

TEST(RenderAnimationTrackTest, RejectsNonFiniteKeyTimes) {
  EXPECT_THROW(AnimationTrack({{std::numeric_limits<double>::infinity(), 1.0}}),
               std::invalid_argument);
}

TEST(RenderAnimationTrackTest, SortsKeyframesByContinuousTime) {
  const AnimationTrack track({{10.0, 10.0}, {0.0, 0.0}, {5.0, 5.0}});

  ASSERT_EQ(3u, track.keyframes().size());
  EXPECT_DOUBLE_EQ(0.0, track.keyframes()[0].time);
  EXPECT_DOUBLE_EQ(5.0, track.keyframes()[1].time);
  EXPECT_DOUBLE_EQ(10.0, track.keyframes()[2].time);
}

TEST(RenderAnimationTrackTest, RejectsDuplicateKeyTimes) {
  EXPECT_THROW(AnimationTrack({{0.0, 0.0}, {0.0, 1.0}}), std::invalid_argument);
}

TEST(RenderAnimationTrackTest, RejectsMixedKeyValueTypes) {
  EXPECT_THROW(AnimationTrack({{0.0, 0.0}, {1.0, Vector3d(1, 2, 3)}}), std::invalid_argument);
}

TEST(RenderAnimationTrackTest, SamplesExactKeys) {
  const AnimationTrack track({{0.0, 0.0}, {1.25, 10.0}});

  EXPECT_DOUBLE_EQ(0.0, track.sample(0.0).get<double>());
  EXPECT_DOUBLE_EQ(10.0, track.sample(1.25).get<double>());
}

TEST(RenderAnimationTrackTest, ClampsBeforeFirstAndAfterLastKey) {
  const AnimationTrack track({{1.0, 20.0}, {2.0, 40.0}});

  EXPECT_DOUBLE_EQ(20.0, track.sample(0.25).get<double>());
  EXPECT_DOUBLE_EQ(40.0, track.sample(4.0).get<double>());
}

TEST(RenderAnimationTrackTest, SamplesSingleKeyTracks) {
  const AnimationTrack track({{1.0, 20.0}});

  EXPECT_DOUBLE_EQ(20.0, track.sample(0.0).get<double>());
  EXPECT_DOUBLE_EQ(20.0, track.sample(1.0).get<double>());
  EXPECT_DOUBLE_EQ(20.0, track.sample(30.0).get<double>());
}

TEST(RenderAnimationTrackTest, RejectsNonFiniteSampleTimes) {
  const AnimationTrack track({{1.0, 20.0}});

  EXPECT_THROW(static_cast<void>(track.sample(std::numeric_limits<double>::quiet_NaN())),
               std::invalid_argument);
}

TEST(RenderAnimationTrackTest, InterpolatesDoubleValuesAtSubframeTime) {
  const AnimationTrack track({{0.0, 10.0}, {1.0, 30.0}});

  EXPECT_DOUBLE_EQ(15.0, track.sample(0.25).get<double>());
}

TEST(RenderAnimationTrackTest, AppliesSmoothStepWeightBetweenKeys) {
  const AnimationTrack track({{0.0, 0.0}, {1.0, 10.0}}, InterpolationMode::SmoothStep);

  EXPECT_DOUBLE_EQ(1.04, track.sample(0.2).get<double>());
  EXPECT_DOUBLE_EQ(5.0, track.sample(0.5).get<double>());
  EXPECT_DOUBLE_EQ(8.96, track.sample(0.8).get<double>());
}

TEST(RenderAnimationTrackTest, StepInterpolationHoldsPreviousKey) {
  const AnimationTrack track({{0.0, "open"}, {1.0, "closed"}}, InterpolationMode::Step);

  EXPECT_EQ("open", track.sample(0.999).get<std::string>());
  EXPECT_EQ("closed", track.sample(1.0).get<std::string>());
}

TEST(RenderAnimationTrackTest, NonInterpolatableLinearTracksFailClearlyBetweenKeys) {
  const AnimationTrack boolTrack({{0.0, false}, {1.0, true}});
  const AnimationTrack stringTrack({{0.0, "open"}, {1.0, "closed"}});

  EXPECT_THROW(static_cast<void>(boolTrack.sample(0.5)), std::logic_error);
  EXPECT_THROW(static_cast<void>(stringTrack.sample(0.5)), std::logic_error);
}

TEST(RenderAnimationTrackTest, InterpolatesVector3dValues) {
  const AnimationTrack track({{0.0, Vector3d(0, 10, 20)}, {0.5, Vector3d(10, 20, 40)}});

  EXPECT_VECTOR_NEAR(Vector3d(5, 15, 30), track.sample(0.25).get<Vector3d>(), 1e-12);
}

TEST(RenderAnimationTrackTest, InterpolatesColordValues) {
  const AnimationTrack track({{0.0, Colord(0.0, 0.2, 0.4)}, {0.5, Colord(1.0, 0.4, 0.8)}});

  EXPECT_COLOR_NEAR(Colord(0.5, 0.3, 0.6), track.sample(0.25).get<Colord>(), 1e-12);
}

TEST(RenderAnimationTrackTest, InterpolatesMatrix4dValues) {
  const AnimationTrack track(
    {{0.0, Matrix4d::translate(0.0, 10.0, 20.0)}, {2.0, Matrix4d::translate(10.0, 20.0, 40.0)}});

  ASSERT_MATRIX_NEAR(Matrix4d::translate(2.5, 12.5, 25.0), track.sample(0.5).get<Matrix4d>(),
                     1e-12);
}

TEST(RenderAnimationTrackTest, MatchesCoreAnimationInterpolationForFrameTimes) {
  const core::animation::AnimationTrack<Vector3d> coreTrack(
    {{0, Vector3d(0, 10, 20)}, {10, Vector3d(10, 20, 40)}}, InterpolationMode::SmoothStep);
  const AnimationTrack renderTrack({{0.0, Vector3d(0, 10, 20)}, {10.0, Vector3d(10, 20, 40)}},
                                   InterpolationMode::SmoothStep);

  EXPECT_VECTOR_NEAR(coreTrack.sample(2), renderTrack.sample(2.0).get<Vector3d>(), 1e-12);
  EXPECT_VECTOR_NEAR(coreTrack.sample(5), renderTrack.sample(5.0).get<Vector3d>(), 1e-12);
  EXPECT_VECTOR_NEAR(coreTrack.sample(8), renderTrack.sample(8.0).get<Vector3d>(), 1e-12);
}
