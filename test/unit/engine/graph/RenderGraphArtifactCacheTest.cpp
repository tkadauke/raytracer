#include <gtest/gtest.h>

#include "engine/graph/GraphRenderEngine.h"
#include "engine/graph/PostProcessPassState.h"
#include "engine/graph/RenderGraphArtifactCache.h"
#include "render/cameras/PinholeCamera.h"
#include "render/primitives/Scene.h"

#include <limits>

namespace RenderGraphArtifactCacheTest {
  using namespace engine::graph;

  RenderResourceDescriptor colorResource(const std::string& id) {
    RenderResourceDescriptor descriptor;
    descriptor.id = id;
    descriptor.name = "Color";
    descriptor.type = RenderResourceType::Color;
    descriptor.format = RenderResourceFormat::RGBDouble;
    descriptor.width = 16;
    descriptor.height = 8;
    descriptor.sampleCount = 1;
    descriptor.domain = RenderResourceDomain::CPU;
    descriptor.lifetime = RenderResourceLifetime::PersistentCache;
    return descriptor;
  }

  RenderPassNode pass(const std::string& id) {
    RenderPassNode node;
    node.id = id;
    node.kind = RenderPassKind::PostProcess;
    node.executor = RenderExecutorKind::PostProcess;
    return node;
  }

  TEST(RenderGraphArtifactCache, StoresAndFindsArtifactsByTypedKey) {
    const RenderPassNode producer = pass("post_fxaa");
    const auto descriptor = colorResource("cached_color");
    const auto key = RenderGraphCacheKey::forPassOutput(producer, descriptor, "inputs:a");

    RenderGraphArtifactCache cache;
    auto artifact = std::make_shared<RenderGraphCachedArtifact>(key, "cached color artifact");
    cache.store(artifact);

    EXPECT_EQ(1u, cache.size());
    EXPECT_TRUE(cache.contains(key));
    ASSERT_EQ(artifact, cache.find(key));
    EXPECT_EQ("cached_color", cache.find(key)->descriptor().id);
    EXPECT_EQ("cached color artifact", cache.find(key)->description());
  }

  TEST(RenderGraphArtifactCache, DepthArtifactOwnsImmutableBufferCopy) {
    Buffer<double> source(2, 2);
    source.clear(std::numeric_limits<double>::infinity());
    source[1][0] = 0.25;

    const auto key =
      RenderGraphCacheKey::forPassOutput(pass("shadow"), colorResource("depth"), "inputs");
    RenderGraphDepthArtifact artifact(key, source, "depth artifact");
    source[1][0] = 0.75;

    Buffer<double> restored(2, 2);
    artifact.copyTo(restored);

    EXPECT_EQ(0.25, restored[1][0]);
    EXPECT_EQ("depth artifact", artifact.description());
  }

  TEST(RenderGraphArtifactCache, SeparatesInputFingerprintsAndPassState) {
    RenderPassNode producer = pass("post_aa");
    const auto descriptor = colorResource("cached_color");

    const auto baseKey = RenderGraphCacheKey::forPassOutput(producer, descriptor, "inputs:a");
    producer.state = std::make_shared<FxaaPostProcessAAState>();
    const auto fxaaKey = RenderGraphCacheKey::forPassOutput(producer, descriptor, "inputs:a");
    const auto movedCameraKey =
      RenderGraphCacheKey::forPassOutput(producer, descriptor, "inputs:b");

    EXPECT_FALSE(baseKey == fxaaKey);
    EXPECT_FALSE(fxaaKey == movedCameraKey);

    RenderGraphArtifactCache cache;
    cache.store(std::make_shared<RenderGraphCachedArtifact>(fxaaKey));

    EXPECT_EQ(nullptr, cache.find(baseKey));
    EXPECT_EQ(nullptr, cache.find(movedCameraKey));
    EXPECT_NE(nullptr, cache.find(fxaaKey));
  }

  TEST(RenderGraphArtifactCache, ErasesArtifactsByProducerOrResource) {
    const auto fxaaColor =
      RenderGraphCacheKey::forPassOutput(pass("post_fxaa"), colorResource("post_color"), "a");
    const auto fxaaPreview =
      RenderGraphCacheKey::forPassOutput(pass("post_fxaa"), colorResource("preview_color"), "a");
    const auto tonemapColor =
      RenderGraphCacheKey::forPassOutput(pass("tonemap"), colorResource("post_color"), "a");

    RenderGraphArtifactCache cache;
    cache.store(std::make_shared<RenderGraphCachedArtifact>(fxaaColor));
    cache.store(std::make_shared<RenderGraphCachedArtifact>(fxaaPreview));
    cache.store(std::make_shared<RenderGraphCachedArtifact>(tonemapColor));

    EXPECT_EQ(2u, cache.eraseProducerOutputs("post_fxaa"));
    EXPECT_FALSE(cache.contains(fxaaColor));
    EXPECT_FALSE(cache.contains(fxaaPreview));
    EXPECT_TRUE(cache.contains(tonemapColor));

    cache.store(std::make_shared<RenderGraphCachedArtifact>(fxaaColor));
    EXPECT_EQ(2u, cache.eraseResource("post_color"));
    EXPECT_FALSE(cache.contains(fxaaColor));
    EXPECT_FALSE(cache.contains(tonemapColor));
    EXPECT_EQ(0u, cache.eraseResource("missing"));
  }

  TEST(RenderGraphArtifactCache, GraphRenderEngineClonesShareCache) {
    auto scene = std::make_shared<render::Scene>();
    auto camera = std::make_shared<render::PinholeCamera>(Vector3d(0, 0, -5), Vector3d::null);
    GraphRenderEngine engine(camera, scene);
    auto clone = std::dynamic_pointer_cast<GraphRenderEngine>(engine.cloneForRender());
    ASSERT_TRUE(clone);

    const auto key = RenderGraphCacheKey::forPassOutput(
      pass("producer"), colorResource("cached_color"), engine.executionInputFingerprint());
    engine.artifactCache()->store(std::make_shared<RenderGraphCachedArtifact>(key));

    EXPECT_EQ(1u, clone->artifactCache()->size());
    EXPECT_NE(nullptr, clone->artifactCache()->find(key));
  }
}
