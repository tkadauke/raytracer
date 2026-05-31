#include <gtest/gtest.h>

#include "engine/graph/RaytracerPassState.h"
#include "engine/graph/RenderPlan.h"
#include "engine/raytracer/Raytracer.h"
#include "render/PathTracingIntegrator.h"
#include "render/WhittedIntegrator.h"
#include "render/cameras/Camera.h"
#include "render/samplers/JitteredSampler.h"
#include "render/viewplanes/TiledViewPlane.h"

#include <QJsonObject>

namespace RaytracerPassStateTest {
  using namespace engine::graph;

  TEST(RaytracerBeautyPassState, SerializesFocusedSubstates) {
    RaytracerBeautyPassState state;
    state.setMaximumRecursionDepth(7);
    state.setMaximumThreads(3);
    state.setQueueSize(11);
    state.setIntegrator("path_tracer");
    state.setSampler("Jittered");
    state.setSamplesPerPixel(16);
    state.setViewPlane("TiledViewPlane");

    const QJsonObject json = state.toJson();

    EXPECT_EQ(7, json.value("execution").toObject().value("maxRecursionDepth").toInt());
    EXPECT_EQ(3, json.value("execution").toObject().value("threads").toInt());
    EXPECT_EQ(11, json.value("execution").toObject().value("queueSize").toInt());
    EXPECT_EQ("pathtracer",
              json.value("execution").toObject().value("integrator").toString().toStdString());
    EXPECT_EQ("Jittered",
              json.value("sampling").toObject().value("sampler").toString().toStdString());
    EXPECT_EQ(16, json.value("sampling").toObject().value("samplesPerPixel").toInt());
    EXPECT_EQ("TiledViewPlane",
              json.value("viewPlane").toObject().value("type").toString().toStdString());

    const RaytracerBeautyPassState decoded = RaytracerBeautyPassState::fromJson(json);
    ASSERT_TRUE(decoded.maximumRecursionDepth().has_value());
    ASSERT_TRUE(decoded.maximumThreads().has_value());
    ASSERT_TRUE(decoded.queueSize().has_value());
    ASSERT_TRUE(decoded.integrator().has_value());
    ASSERT_TRUE(decoded.sampler().has_value());
    ASSERT_TRUE(decoded.samplesPerPixel().has_value());
    ASSERT_TRUE(decoded.viewPlane().has_value());
    EXPECT_EQ(7, *decoded.maximumRecursionDepth());
    EXPECT_EQ(3, *decoded.maximumThreads());
    EXPECT_EQ(11, *decoded.queueSize());
    EXPECT_EQ("pathtracer", *decoded.integrator());
    EXPECT_EQ("Jittered", *decoded.sampler());
    EXPECT_EQ(16, *decoded.samplesPerPixel());
    EXPECT_EQ("TiledViewPlane", *decoded.viewPlane());
  }

  TEST(RaytracerBeautyPassState, AppliesPathTracingIntegratorToRaytracer) {
    RaytracerBeautyPassState state;
    state.setIntegrator("pt");
    state.setMaximumRecursionDepth(5);

    engine::raytracer::Raytracer raytracer(nullptr);
    ASSERT_NE(nullptr, dynamic_cast<const render::WhittedIntegrator*>(&raytracer.integrator()));

    state.applyTo(raytracer);

    const auto* integrator =
      dynamic_cast<const render::PathTracingIntegrator*>(&raytracer.integrator());
    ASSERT_NE(nullptr, integrator);
    EXPECT_EQ(5, integrator->maximumRecursionDepth());
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
