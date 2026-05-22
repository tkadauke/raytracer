#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "core/animation/AnimationTrack.h"
#include "core/animation/Timeline.h"
#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/interpolation/Interpolation.h"

using namespace core::animation;
using namespace core::math::interpolation;

namespace {

void expectVectorNear(const Vector3d& expected, const Vector3d& actual, double epsilon) {
  EXPECT_NEAR(expected.x(), actual.x(), epsilon);
  EXPECT_NEAR(expected.y(), actual.y(), epsilon);
  EXPECT_NEAR(expected.z(), actual.z(), epsilon);
}

void expectColorNear(const Colord& expected, const Colord& actual, double epsilon) {
  EXPECT_NEAR(expected.r(), actual.r(), epsilon);
  EXPECT_NEAR(expected.g(), actual.g(), epsilon);
  EXPECT_NEAR(expected.b(), actual.b(), epsilon);
}

}  // namespace

TEST(TimelineTest, RejectsInvalidFrameRange) {
  EXPECT_THROW(Timeline(10, 9, 24.0), std::invalid_argument);
}

TEST(TimelineTest, RejectsInvalidFps) {
  EXPECT_THROW(Timeline(1, 10, 0.0), std::invalid_argument);
  EXPECT_THROW(Timeline(1, 10, -24.0), std::invalid_argument);
}

TEST(TimelineTest, ConvertsFramesToSecondsFromTimelineStart) {
  const Timeline timeline(10, 34, 24.0);

  EXPECT_TRUE(timeline.containsFrame(10));
  EXPECT_TRUE(timeline.containsFrame(34));
  EXPECT_FALSE(timeline.containsFrame(9));
  EXPECT_FALSE(timeline.containsFrame(35));
  EXPECT_NEAR(0.0, timeline.secondsForFrame(10), 1e-12);
  EXPECT_NEAR(0.5, timeline.secondsForFrame(22), 1e-12);

  const auto time = timeline.timeAtFrame(22);
  EXPECT_EQ(22, time.frame());
  EXPECT_NEAR(0.5, time.secondsFromStart(), 1e-12);
}

TEST(AnimationTrackTest, RequiresAtLeastOneKeyframe) {
  EXPECT_THROW(AnimationTrack<double>({}), std::invalid_argument);
}

TEST(AnimationTrackTest, SortsKeyframesByFrame) {
  const AnimationTrack<double> track({{10, 10.0}, {0, 0.0}, {5, 5.0}});

  ASSERT_EQ(3u, track.keyframes().size());
  EXPECT_EQ(0, track.keyframes()[0].frame);
  EXPECT_EQ(5, track.keyframes()[1].frame);
  EXPECT_EQ(10, track.keyframes()[2].frame);
}

TEST(AnimationTrackTest, RejectsDuplicateKeyframeFrames) {
  EXPECT_THROW((AnimationTrack<double>({{0, 0.0}, {0, 1.0}})), std::invalid_argument);
}

TEST(AnimationTrackTest, SamplesExactKeys) {
  const AnimationTrack<double> track({{0, 0.0}, {10, 20.0}});

  EXPECT_DOUBLE_EQ(0.0, track.sample(0));
  EXPECT_DOUBLE_EQ(20.0, track.sample(10));
}

TEST(AnimationTrackTest, ClampsBeforeFirstAndAfterLastKey) {
  const AnimationTrack<double> track({{10, 20.0}, {20, 40.0}});

  EXPECT_DOUBLE_EQ(20.0, track.sample(0));
  EXPECT_DOUBLE_EQ(40.0, track.sample(30));
}

TEST(AnimationTrackTest, SamplesSingleKeyTracks) {
  const AnimationTrack<double> track({{10, 20.0}});

  EXPECT_DOUBLE_EQ(20.0, track.sample(0));
  EXPECT_DOUBLE_EQ(20.0, track.sample(10));
  EXPECT_DOUBLE_EQ(20.0, track.sample(30));
}

TEST(AnimationTrackTest, InterpolatesLinearlyBetweenKeys) {
  const AnimationTrack<double> track({{0, 10.0}, {10, 30.0}});

  EXPECT_DOUBLE_EQ(20.0, track.sample(5));
}

TEST(AnimationTrackTest, AppliesSmoothStepWeightBetweenKeys) {
  const AnimationTrack<double> track({{0, 0.0}, {10, 10.0}}, InterpolationMode::SmoothStep);

  EXPECT_DOUBLE_EQ(1.04, track.sample(2));
  EXPECT_DOUBLE_EQ(5.0, track.sample(5));
  EXPECT_DOUBLE_EQ(8.96, track.sample(8));
}

TEST(AnimationTrackTest, StepInterpolationHoldsPreviousKey) {
  const AnimationTrack<double> track({{0, 10.0}, {10, 30.0}, {20, 50.0}}, InterpolationMode::Step);

  EXPECT_DOUBLE_EQ(10.0, track.sample(9));
  EXPECT_DOUBLE_EQ(30.0, track.sample(10));
  EXPECT_DOUBLE_EQ(30.0, track.sample(19));
}

TEST(AnimationTrackTest, StepInterpolationSupportsNonInterpolatableValues) {
  const AnimationTrack<std::string> track({{0, "open"}, {10, "closed"}}, InterpolationMode::Step);

  EXPECT_EQ("open", track.sample(5));
  EXPECT_EQ("closed", track.sample(10));
}

TEST(AnimationTrackTest, NonInterpolatableLinearTrackFailsClearlyAtBetweenKeyFrames) {
  const AnimationTrack<std::string> track({{0, "open"}, {10, "closed"}}, InterpolationMode::Linear);

  EXPECT_THROW(static_cast<void>(track.sample(5)), std::logic_error);
}

TEST(AnimationTrackTest, InterpolatesVector3dValues) {
  const AnimationTrack<Vector3d> track({{0, Vector3d(0, 10, 20)}, {10, Vector3d(10, 20, 40)}});

  expectVectorNear(Vector3d(5, 15, 30), track.sample(5), 1e-12);
}

TEST(AnimationTrackTest, InterpolatesColordValues) {
  const AnimationTrack<Colord> track({{0, Colord(0.0, 0.2, 0.4)}, {10, Colord(1.0, 0.4, 0.8)}});

  expectColorNear(Colord(0.5, 0.3, 0.6), track.sample(5), 1e-12);
}
