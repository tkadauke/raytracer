#include <gtest/gtest.h>

#include <string>

#include "core/Color.h"
#include "core/math/Vector.h"
#include "core/math/interpolation/Interpolation.h"

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

TEST(InterpolationModesTest, ParsesSupportedNames) {
  EXPECT_EQ(InterpolationMode::Step, InterpolationModes::fromName("step"));
  EXPECT_EQ(InterpolationMode::Linear, InterpolationModes::fromName("linear"));
  EXPECT_EQ(InterpolationMode::SmoothStep, InterpolationModes::fromName("smoothstep"));

  EXPECT_EQ("step", InterpolationModes::name(InterpolationMode::Step));
  EXPECT_EQ("linear", InterpolationModes::name(InterpolationMode::Linear));
  EXPECT_EQ("smoothstep", InterpolationModes::name(InterpolationMode::SmoothStep));
}

TEST(InterpolationModesTest, RejectsUnsupportedName) {
  EXPECT_THROW(InterpolationModes::fromName("bezier"), std::invalid_argument);
}

TEST(StepInterpolatorTest, HoldsFirstValue) {
  EXPECT_EQ("open", StepInterpolator<std::string>::interpolate("open", "closed", 0.75));
  EXPECT_EQ("closed", StepInterpolator<std::string>::interpolate("open", "closed", 1.0));
}

TEST(LinearInterpolatorTest, InterpolatesScalarValues) {
  EXPECT_DOUBLE_EQ(2.5, LinearInterpolator<double>::interpolate(0.0, 10.0, 0.25));
}

TEST(LinearInterpolatorTest, InterpolatesVectorValues) {
  const auto actual = LinearInterpolator<Vector3d>::interpolate(Vector3d(0, 10, 20),
                                                                Vector3d(10, 20, 40), 0.25);

  expectVectorNear(Vector3d(2.5, 12.5, 25), actual, 1e-12);
}

TEST(LinearInterpolatorTest, InterpolatesColorValues) {
  const auto actual = LinearInterpolator<Colord>::interpolate(Colord(0.0, 0.2, 0.4),
                                                              Colord(1.0, 0.4, 0.8), 0.25);

  expectColorNear(Colord(0.25, 0.25, 0.5), actual, 1e-12);
}

TEST(SmoothStepInterpolatorTest, RemapsInterpolationWeight) {
  EXPECT_DOUBLE_EQ(1.04, SmoothStepInterpolator<double>::interpolate(0.0, 10.0, 0.2));
  EXPECT_DOUBLE_EQ(5.0, SmoothStepInterpolator<double>::interpolate(0.0, 10.0, 0.5));
  EXPECT_DOUBLE_EQ(8.96, SmoothStepInterpolator<double>::interpolate(0.0, 10.0, 0.8));
}

TEST(InterpolatorTest, DispatchesInterpolationMode) {
  EXPECT_DOUBLE_EQ(0.0, Interpolator<double>::interpolate(InterpolationMode::Step, 0.0, 10.0, 0.5));
  EXPECT_DOUBLE_EQ(5.0, Interpolator<double>::interpolate(InterpolationMode::Linear, 0.0, 10.0, 0.5));
  EXPECT_DOUBLE_EQ(5.0, Interpolator<double>::interpolate(InterpolationMode::SmoothStep, 0.0, 10.0, 0.5));
}

TEST(InterpolatorTest, NonLinearModesRequireInterpolatableValues) {
  EXPECT_THROW(Interpolator<std::string>::interpolate(InterpolationMode::Linear, "open", "closed", 0.5),
               std::logic_error);
}
