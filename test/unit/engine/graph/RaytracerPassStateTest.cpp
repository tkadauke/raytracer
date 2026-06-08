#include <gtest/gtest.h>

#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/RenderPlan.h"
#include "engine/raytracer/Raytracer.h"
#include "engine/wavefront/WavefrontRaytracer.h"
#include "render/PathTracingIntegrator.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/denoise/BilateralDenoiser.h"
#include "render/samplers/JitteredSampler.h"
#include "render/viewplanes/TiledViewPlane.h"

#include <QJsonObject>

#include <stdexcept>
#include <string>

namespace RaytracerPassStateTest {
  using namespace engine::graph;

  TEST(RaytracerBeautyPassState, SerializesFocusedSubstates) {
    RaytracerBeautyPassState state;
    state.setMaximumRecursionDepth(7);
    state.setMaximumThreads(3);
    state.setQueueSize(11);
    state.setIntegrator("path_tracer");
    state.setIntersectionBackend("gpu");
    state.setRussianRouletteDepth(4);
    state.setDirectLightSamples(6);
    state.setSampler("Jittered");
    state.setSamplesPerPixel(16);
    state.setSamplingSeed(12345);
    state.setViewPlane("TiledViewPlane");
    state.setConvergenceEnabled(true);
    state.setConvergenceActiveSampleFractionThreshold(0.125);
    state.setConvergenceRadianceDeltaRmsThreshold(0.0025);
    state.setAdaptiveSamplingEnabled(true);
    state.setAdaptiveMinimumSamples(4);
    state.setAdaptiveStddevThreshold(0.05);
    state.setDenoiser("bilateral");
    state.setDenoiseRadius(3);
    state.setDenoiseColorSigma(0.2);

    const QJsonObject json = state.toJson();

    EXPECT_EQ(7, json.value("execution").toObject().value("maxRecursionDepth").toInt());
    EXPECT_EQ(3, json.value("execution").toObject().value("threads").toInt());
    EXPECT_EQ(11, json.value("execution").toObject().value("queueSize").toInt());
    EXPECT_EQ("pathtracer",
              json.value("execution").toObject().value("integrator").toString().toStdString());
    EXPECT_EQ(
      "gpu",
      json.value("execution").toObject().value("intersectionBackend").toString().toStdString());
    EXPECT_EQ(4, json.value("execution").toObject().value("russianRouletteDepth").toInt());
    EXPECT_EQ(6, json.value("execution").toObject().value("directLightSamples").toInt());
    EXPECT_EQ("Jittered",
              json.value("sampling").toObject().value("sampler").toString().toStdString());
    EXPECT_EQ(16, json.value("sampling").toObject().value("samplesPerPixel").toInt());
    EXPECT_EQ(12345.0, json.value("sampling").toObject().value("seed").toDouble());
    EXPECT_EQ("TiledViewPlane",
              json.value("viewPlane").toObject().value("type").toString().toStdString());
    EXPECT_TRUE(json.value("convergence").toObject().value("enabled").toBool());
    EXPECT_DOUBLE_EQ(
      0.125,
      json.value("convergence").toObject().value("activeSampleFractionThreshold").toDouble());
    EXPECT_DOUBLE_EQ(
      0.0025, json.value("convergence").toObject().value("radianceDeltaRmsThreshold").toDouble());
    EXPECT_TRUE(json.value("adaptiveSampling").toObject().value("enabled").toBool());
    EXPECT_EQ(4, json.value("adaptiveSampling").toObject().value("minimumSamples").toInt());
    EXPECT_DOUBLE_EQ(0.05,
                     json.value("adaptiveSampling").toObject().value("stddevThreshold").toDouble());
    EXPECT_EQ("bilateral", json.value("denoise").toObject().value("type").toString().toStdString());
    EXPECT_EQ(3, json.value("denoise").toObject().value("radius").toInt());
    EXPECT_DOUBLE_EQ(0.2, json.value("denoise").toObject().value("colorSigma").toDouble());

    const RaytracerBeautyPassState decoded = RaytracerBeautyPassState::fromJson(json);
    ASSERT_TRUE(decoded.maximumRecursionDepth().has_value());
    ASSERT_TRUE(decoded.maximumThreads().has_value());
    ASSERT_TRUE(decoded.queueSize().has_value());
    ASSERT_TRUE(decoded.integrator().has_value());
    ASSERT_TRUE(decoded.intersectionBackend().has_value());
    ASSERT_TRUE(decoded.russianRouletteDepth().has_value());
    ASSERT_TRUE(decoded.directLightSamples().has_value());
    ASSERT_TRUE(decoded.sampler().has_value());
    ASSERT_TRUE(decoded.samplesPerPixel().has_value());
    ASSERT_TRUE(decoded.samplingSeed().has_value());
    ASSERT_TRUE(decoded.viewPlane().has_value());
    ASSERT_TRUE(decoded.convergenceEnabled().has_value());
    ASSERT_TRUE(decoded.convergenceActiveSampleFractionThreshold().has_value());
    ASSERT_TRUE(decoded.convergenceRadianceDeltaRmsThreshold().has_value());
    ASSERT_TRUE(decoded.adaptiveSamplingEnabled().has_value());
    ASSERT_TRUE(decoded.adaptiveMinimumSamples().has_value());
    ASSERT_TRUE(decoded.adaptiveStddevThreshold().has_value());
    ASSERT_TRUE(decoded.denoiser().has_value());
    ASSERT_TRUE(decoded.denoiseRadius().has_value());
    ASSERT_TRUE(decoded.denoiseColorSigma().has_value());
    EXPECT_EQ(7, *decoded.maximumRecursionDepth());
    EXPECT_EQ(3, *decoded.maximumThreads());
    EXPECT_EQ(11, *decoded.queueSize());
    EXPECT_EQ("pathtracer", *decoded.integrator());
    EXPECT_STREQ("gpu", decoded.intersectionBackend()->id());
    EXPECT_EQ(4, *decoded.russianRouletteDepth());
    EXPECT_EQ(6, *decoded.directLightSamples());
    EXPECT_EQ("Jittered", *decoded.sampler());
    EXPECT_EQ(16, *decoded.samplesPerPixel());
    EXPECT_EQ(12345u, *decoded.samplingSeed());
    EXPECT_EQ("TiledViewPlane", *decoded.viewPlane());
    EXPECT_TRUE(*decoded.convergenceEnabled());
    EXPECT_DOUBLE_EQ(0.125, *decoded.convergenceActiveSampleFractionThreshold());
    EXPECT_DOUBLE_EQ(0.0025, *decoded.convergenceRadianceDeltaRmsThreshold());
    EXPECT_TRUE(*decoded.adaptiveSamplingEnabled());
    EXPECT_EQ(4, *decoded.adaptiveMinimumSamples());
    EXPECT_DOUBLE_EQ(0.05, *decoded.adaptiveStddevThreshold());
    EXPECT_EQ("bilateral", *decoded.denoiser());
    EXPECT_EQ(3, *decoded.denoiseRadius());
    EXPECT_DOUBLE_EQ(0.2, *decoded.denoiseColorSigma());
  }

  TEST(RaytracerBeautyPassState, RejectsSamplingSeedOutsideExactJsonIntegerRange) {
    QJsonObject sampling;
    sampling["seed"] = 9007199254740992.0;
    QJsonObject json;
    json["sampling"] = sampling;

    EXPECT_THROW(RaytracerBeautyPassState::fromJson(json), std::runtime_error);
  }

  TEST(RaytracerBeautyPassState, AppliesPathTracingIntegratorToRaytracer) {
    RaytracerBeautyPassState state;
    state.setIntegrator("pt");
    state.setMaximumRecursionDepth(5);
    state.setRussianRouletteDepth(2);
    state.setDirectLightSamples(3);

    engine::raytracer::Raytracer raytracer(nullptr);
    ASSERT_NE(nullptr, dynamic_cast<const render::WhittedIntegrator*>(&raytracer.integrator()));

    state.applyTo(raytracer);

    const auto* integrator =
      dynamic_cast<const render::PathTracingIntegrator*>(&raytracer.integrator());
    ASSERT_NE(nullptr, integrator);
    EXPECT_EQ(5, integrator->maximumRecursionDepth());
    EXPECT_EQ(2, integrator->russianRouletteDepth());
    EXPECT_EQ(3, integrator->directLightSamples());
  }

  TEST(RaytracerBeautyPassState, AppliesSamplingAndViewPlaneToRaytracer) {
    RaytracerBeautyPassState state;
    state.setSampler("Jittered");
    state.setSamplesPerPixel(9);
    state.setViewPlane("TiledViewPlane");

    engine::raytracer::Raytracer raytracer(nullptr);
    state.applyTo(raytracer);

    ASSERT_NE(nullptr, raytracer.camera());
    ASSERT_NE(nullptr, raytracer.camera()->viewPlane());
    EXPECT_NE(nullptr,
              dynamic_cast<render::TiledViewPlane*>(raytracer.camera()->viewPlane().get()));
    ASSERT_NE(nullptr, raytracer.camera()->viewPlane()->sampler());
    EXPECT_NE(nullptr, dynamic_cast<render::JitteredSampler*>(
                         raytracer.camera()->viewPlane()->sampler().get()));
    EXPECT_EQ(9, raytracer.camera()->viewPlane()->sampler()->numSamples());
  }

  TEST(RaytracerBeautyPassState, AppliesSamplingSeedToGeneratedSamplerSets) {
    RaytracerBeautyPassState state;
    state.setSampler("Jittered");
    state.setSamplesPerPixel(4);
    state.setSamplingSeed(1234);

    engine::raytracer::Raytracer first(nullptr);
    engine::raytracer::Raytracer second(nullptr);
    state.applyTo(first);
    state.applyTo(second);

    ASSERT_NE(nullptr, first.camera());
    ASSERT_NE(nullptr, second.camera());
    const auto firstSampler = first.camera()->viewPlane()->sampler();
    const auto secondSampler = second.camera()->viewPlane()->sampler();
    ASSERT_NE(nullptr, firstSampler);
    ASSERT_NE(nullptr, secondSampler);
    ASSERT_FALSE(firstSampler->setAt(0).empty());
    ASSERT_FALSE(secondSampler->setAt(0).empty());
    EXPECT_EQ(firstSampler->setAt(0), secondSampler->setAt(0));
  }

  TEST(RaytracerBeautyPassState, AppliesDenoiserToWavefront) {
    RaytracerBeautyPassState state;
    state.setDenoiser("bilateral");
    state.setDenoiseRadius(4);
    state.setDenoiseColorSigma(0.25);

    engine::wavefront::WavefrontRaytracer wavefront{std::shared_ptr<render::Scene>()};

    state.applyTo(wavefront);

    const auto* bilateral = dynamic_cast<const render::BilateralDenoiser*>(wavefront.denoiser());
    ASSERT_NE(nullptr, bilateral);
    EXPECT_EQ(4, bilateral->radius());
    EXPECT_DOUBLE_EQ(0.25, bilateral->colorSigma());

    state.setDenoiser("none");
    state.applyTo(wavefront);

    EXPECT_EQ(nullptr, wavefront.denoiser());
  }

  TEST(RaytracerBeautyPassState, AppliesAdaptiveSamplingToWavefront) {
    RaytracerBeautyPassState state;
    state.setAdaptiveSamplingEnabled(true);
    state.setAdaptiveMinimumSamples(3);
    state.setAdaptiveStddevThreshold(0.125);

    engine::wavefront::WavefrontRaytracer wavefront{std::shared_ptr<render::Scene>()};

    state.applyTo(wavefront);

    EXPECT_TRUE(wavefront.adaptiveSamplingEnabled());
    EXPECT_EQ(3, wavefront.adaptiveMinimumSamples());
    EXPECT_DOUBLE_EQ(0.125, wavefront.adaptiveStddevThreshold());
  }

  TEST(RaytracerBeautyPassState, AppliesIntersectionBackendToWavefront) {
    RaytracerBeautyPassState state;
    state.setIntersectionBackend("gpu");

    engine::wavefront::WavefrontRaytracer wavefront{std::shared_ptr<render::Scene>()};

    state.applyTo(wavefront);

    const auto& backend = wavefront.intersectionBackend().resolvedBackend();
    EXPECT_STREQ("gpu", backend.requestedName());
    EXPECT_STREQ("cpu", backend.name());
    EXPECT_STREQ("fallback", backend.availability());
    EXPECT_FALSE(std::string(backend.fallbackReason()).empty());
  }

  TEST(RaytracerBeautyPassState, AppliesSamplingSeedToRayFamilyEngines) {
    RaytracerBeautyPassState state;
    state.setSamplingSeed(6789);

    engine::raytracer::Raytracer raytracer(nullptr);
    engine::wavefront::WavefrontRaytracer wavefront{std::shared_ptr<render::Scene>()};

    state.applyTo(raytracer);
    state.applyTo(wavefront);

    ASSERT_TRUE(raytracer.samplingSeed().has_value());
    ASSERT_TRUE(wavefront.samplingSeed().has_value());
    EXPECT_EQ(6789u, *raytracer.samplingSeed());
    EXPECT_EQ(6789u, *wavefront.samplingSeed());
  }

  TEST(RaytracerBeautyPassState, WritesOnlyToRayFamilyBeautyPasses) {
    RenderPlan plan;
    RenderPassNode raytracer;
    raytracer.id = "raytrace_beauty";
    raytracer.kind = RenderPassKind::Beauty;
    raytracer.executor = RenderExecutorKind::Raytracer;
    plan.addPass(raytracer);

    RenderPassNode wavefront;
    wavefront.id = "wavefront_beauty";
    wavefront.kind = RenderPassKind::Beauty;
    wavefront.executor = RenderExecutorKind::Wavefront;
    plan.addPass(wavefront);

    RenderPassNode rasterizer;
    rasterizer.id = "raster_beauty";
    rasterizer.kind = RenderPassKind::Beauty;
    rasterizer.executor = RenderExecutorKind::Rasterizer;
    plan.addPass(rasterizer);

    RaytracerBeautyPassState state;
    state.setSamplesPerPixel(4);

    EXPECT_EQ(2u, state.writeToRaytracerBeautyPasses(plan));
    ASSERT_NE(nullptr, plan.passes()[0].state);
    ASSERT_NE(nullptr, plan.passes()[1].state);
    EXPECT_NE(nullptr, RaytracerBeautyPassState::fromPass(plan.passes()[0]));
    EXPECT_NE(nullptr, RaytracerBeautyPassState::fromPass(plan.passes()[1]));
    EXPECT_EQ(nullptr, plan.passes()[2].state);
  }
}
