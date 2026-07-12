#include "gtest/gtest.h"
#include "render/brdf/PerfectTransmitter.h"
#include "core/math/HitPoint.h"
#include "core/math/Ray.h"

#include <cmath>

namespace PerfectTransmitterTest {
  using namespace render;

  // Construct a hit point at the origin with the given outward normal. The
  // primitive pointer is irrelevant for these tests; PerfectTransmitter only
  // reads .normal() and .point().
  static HitPoint hitPointWithNormal(const Vector3d& normal) {
    return HitPoint(nullptr, 1.0, Vector4d(0, 0, 0, 1), normal);
  }

  TEST(PerfectTransmitter, ShouldInitialize) {
    PerfectTransmitter btdf;
    ASSERT_EQ(1.0, btdf.transmissionCoefficient());
    // The default refraction index is 16 (effectively diamond-and-then-some);
    // most practical materials override it.
    ASSERT_EQ(16, btdf.refractionIndex());
  }

  TEST(PerfectTransmitter, ShouldClampTransmissionCoefficientToZeroOne) {
    PerfectTransmitter btdf;
    btdf.setTransmissionCoefficient(-0.5);
    ASSERT_EQ(0.0, btdf.transmissionCoefficient());
    btdf.setTransmissionCoefficient(2.0);
    ASSERT_EQ(1.0, btdf.transmissionCoefficient());
    btdf.setTransmissionCoefficient(0.7);
    ASSERT_EQ(0.7, btdf.transmissionCoefficient());
  }

  TEST(PerfectTransmitter, ShouldStoreRefractionIndex) {
    PerfectTransmitter btdf;
    btdf.setRefractionIndex(1.5); // typical glass.
    ASSERT_EQ(1.5, btdf.refractionIndex());
  }

  // Total internal reflection occurs when light travels from a denser medium
  // (high IOR) into a less dense medium (low IOR) at an angle past the
  // critical angle. For glass→air, the critical angle is asin(1/1.5) ≈ 41.8°.
  //
  // In the PerfectTransmitter geometry, "from inside" means the ray points
  // outward (out of the surface) and the hit point's normal points outward
  // — so wo = -ray.direction points inward, normal·wo is negative. The
  // implementation detects this and inverts eta to 1/refractionIndex.

  TEST(PerfectTransmitter, ShouldDetectTotalInternalReflectionPastCriticalAngle) {
    PerfectTransmitter btdf;
    btdf.setRefractionIndex(1.5);

    // 60° from normal — well past the ~41.8° critical angle.
    const double angle = 60.0 * M_PI / 180.0;
    Vector3d normal(0, 1, 0);
    Vector3d direction(std::sin(angle), std::cos(angle), 0); // outward + tilted.
    Rayd ray(Vector3d(0, -1, 0), direction);
    HitPoint hit = hitPointWithNormal(normal);

    ASSERT_TRUE(btdf.totalInternalReflection(ray, hit));
  }

  TEST(PerfectTransmitter, ShouldNotDetectTIRBelowCriticalAngle) {
    PerfectTransmitter btdf;
    btdf.setRefractionIndex(1.5);

    // 30° from normal — well below the ~41.8° critical angle.
    const double angle = 30.0 * M_PI / 180.0;
    Vector3d normal(0, 1, 0);
    Vector3d direction(std::sin(angle), std::cos(angle), 0);
    Rayd ray(Vector3d(0, -1, 0), direction);
    HitPoint hit = hitPointWithNormal(normal);

    ASSERT_FALSE(btdf.totalInternalReflection(ray, hit));
  }

  TEST(PerfectTransmitter, ShouldNotDetectTIRAtNormalIncidenceFromInside) {
    PerfectTransmitter btdf;
    btdf.setRefractionIndex(1.5);

    // Ray exactly along the outward normal — angle 0, sin² = 0, no TIR.
    Vector3d normal(0, 1, 0);
    Rayd ray(Vector3d(0, -1, 0), Vector3d(0, 1, 0));
    HitPoint hit = hitPointWithNormal(normal);

    ASSERT_FALSE(btdf.totalInternalReflection(ray, hit));
  }

  TEST(PerfectTransmitter, ShouldNeverDetectTIRWhenEnteringDenserMedium) {
    // Going from low IOR (e.g. air) into high IOR (glass), TIR is impossible
    // regardless of incidence angle — Snell's law always yields a real
    // transmitted angle.
    PerfectTransmitter btdf;
    btdf.setRefractionIndex(1.5);

    Vector3d normal(0, 1, 0); // outward (toward air) at the entry surface.
    HitPoint hit = hitPointWithNormal(normal);

    // Ray heading inward at a steep glancing angle (80° from normal) —
    // direction is mostly horizontal, with a small downward component.
    const double angle = 80.0 * M_PI / 180.0;
    Vector3d direction(std::sin(angle), -std::cos(angle), 0);
    Rayd ray(Vector3d(-1, 1, 0), direction);

    ASSERT_FALSE(btdf.totalInternalReflection(ray, hit));
  }

  TEST(PerfectTransmitter, ShouldComputeStraightTransmissionAtNormalIncidence) {
    PerfectTransmitter btdf;
    btdf.setRefractionIndex(1.5);

    Vector3d normal(0, 1, 0);
    HitPoint hit = hitPointWithNormal(normal);
    Vector3d out(0, 1, 0); // straight along normal.
    Vector3d transmitted;
    btdf.sample(hit, out, transmitted);

    // Normal-incidence transmission should not bend the ray (ignoring sign
    // conventions on the in/out vectors, which the implementation handles).
    // The transmitted ray should be collinear with the negative incident.
    ASSERT_NEAR(0.0, transmitted.x(), 1e-9);
    ASSERT_NEAR(0.0, transmitted.z(), 1e-9);
    // y-component direction depends on the eta-flip path; magnitude only.
    ASSERT_NEAR(1.0, std::fabs(transmitted.normalized().y()), 1e-9);
  }

  TEST(PerfectTransmitter, ShouldExposeDeltaTransmissionFlagsAndPdfContract) {
    PerfectTransmitter btdf;
    btdf.setRefractionIndex(1.5);
    btdf.setTransmissionCoefficient(0.75);

    Vector3d normal(0, 1, 0);
    HitPoint hit = hitPointWithNormal(normal);
    Vector3d transmitted;
    double sampledPdf = 0.0;
    Colord value = btdf.sample(hit, Vector3d(0, 1, 0), transmitted, sampledPdf);

    ASSERT_TRUE(btdf.isSpecular());
    ASSERT_TRUE(btdf.isTransmission());
    ASSERT_TRUE(btdf.isDelta());
    ASSERT_FALSE(btdf.isReflection());
    ASSERT_NEAR(1.0, sampledPdf, 1e-12);
    ASSERT_NEAR(0.0, btdf.pdf(hit, Vector3d(0, 1, 0), transmitted), 1e-12);
    ASSERT_GT(value.r(), 0.0);
    ASSERT_EQ(value.r(), value.g());
    ASSERT_EQ(value.g(), value.b());
  }

  TEST(PerfectTransmitter, ShouldRefractAccordingToSnellsLaw) {
    // n1 sin θ1 = n2 sin θ2. With air→glass (eta=1.5) and θ1=45°, the
    // transmitted angle should satisfy sin θ2 = sin 45° / 1.5.
    PerfectTransmitter btdf;
    const double n2 = 1.5;
    btdf.setRefractionIndex(n2);

    const double angleIn = 45.0 * M_PI / 180.0;
    Vector3d normal(0, 1, 0);
    HitPoint hit = hitPointWithNormal(normal);
    Vector3d out(std::sin(angleIn), std::cos(angleIn), 0); // pointing outward + tilted.

    Vector3d transmitted;
    btdf.sample(hit, out, transmitted);

    // Transmitted ray should travel below the surface (negative y) and have
    // reduced angle compared to the incident direction.
    ASSERT_LT(transmitted.y(), 0.0);

    // sin θ2 = |x-component of transmitted direction| / |transmitted|
    Vector3d dirT = transmitted.normalized();
    double sinThetaOut = std::fabs(dirT.x());
    double sinThetaIn = std::sin(angleIn);
    double expected = sinThetaIn / n2;
    ASSERT_NEAR(expected, sinThetaOut, 1e-6);
  }
}
