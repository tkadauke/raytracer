#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "engine/graph/RenderResourceStorage.h"

namespace RenderResourceStorageTest {
  using namespace engine::graph;

  RenderResourceDescriptor resource(const std::string& id, RenderResourceType type) {
    RenderResourceDescriptor descriptor;
    descriptor.id = id;
    descriptor.name = id;
    descriptor.type = type;
    descriptor.width = 4;
    descriptor.height = 3;
    descriptor.sampleCount = 1;
    return descriptor;
  }

  TEST(RenderResourceStorage, AllocatesSupportedCpuBuffers) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("color", RenderResourceType::Color),
      resource("depth", RenderResourceType::Depth),
      resource("stencil", RenderResourceType::Stencil),
      resource("object_id", RenderResourceType::ObjectId),
    });

    EXPECT_TRUE(storage.contains("color"));
    EXPECT_TRUE(storage.hasBuffer("color"));
    EXPECT_TRUE(storage.resource("color").colorBacked());
    EXPECT_FALSE(storage.resource("depth").colorBacked());
    EXPECT_EQ(4, storage.color("color").width());
    EXPECT_EQ(3, storage.color("color").height());
    EXPECT_EQ(4, storage.depth("depth").width());
    EXPECT_EQ(3, storage.stencil("stencil").height());
    EXPECT_EQ(4, storage.objectId("object_id").width());
  }

  TEST(RenderResourceStorage, AllocatesAovFamiliesToExpectedCpuBuffers) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("normal", RenderResourceType::Normal),
      resource("world_position", RenderResourceType::WorldPosition),
      resource("material_id", RenderResourceType::MaterialId),
      resource("shadow_map", RenderResourceType::ShadowMap),
    });

    EXPECT_TRUE(storage.resource("normal").colorBacked());
    EXPECT_TRUE(storage.resource("world_position").colorBacked());
    EXPECT_TRUE(storage.resource("material_id").objectIdBacked());
    EXPECT_TRUE(storage.resource("shadow_map").depthBacked());
  }

  TEST(RenderResourceStorage, KeepsDescriptorForGpuResourceWithoutCpuBuffer) {
    auto descriptor = resource("gpu_color", RenderResourceType::Color);
    descriptor.domain = RenderResourceDomain::GPU;

    RenderResourceStorage storage;
    storage.allocate({
      descriptor,
    });

    EXPECT_TRUE(storage.contains("gpu_color"));
    EXPECT_FALSE(storage.hasBuffer("gpu_color"));
    EXPECT_EQ(RenderResourceType::Color, storage.descriptor("gpu_color").type);
    EXPECT_EQ(RenderResourceDomain::GPU, storage.descriptor("gpu_color").domain);
  }

  TEST(RenderResourceStorage, ThrowsForWrongTypedAccess) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("color", RenderResourceType::Color),
    });

    EXPECT_THROW(storage.depth("color"), std::out_of_range);
    EXPECT_THROW(storage.descriptor("missing"), std::out_of_range);
  }

  TEST(RenderResourceStorage, BindsExternalColorBuffer) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("history_color", RenderResourceType::Color),
    });

    Buffer<Colord> external(4, 3);
    external.clear(Colord(0.25, 0.5, 0.75));

    storage.bindColor("history_color", external);

    EXPECT_EQ(Colord(0.25, 0.5, 0.75), storage.color("history_color")[1][2]);
    EXPECT_FALSE(storage.resource("history_color").substituteDefault());
  }

  TEST(RenderResourceStorage, RejectsMismatchedExternalColorBufferShape) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("history_color", RenderResourceType::Color),
    });

    Buffer<Colord> external(2, 3);

    EXPECT_THROW(storage.bindColor("history_color", external), std::runtime_error);
  }

  TEST(RenderResourceStorage, BindsExternalDepthBuffer) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("history_depth", RenderResourceType::Depth),
    });

    Buffer<double> external(4, 3);
    external.clear(0.75);

    storage.bindDepth("history_depth", external);

    EXPECT_EQ(0.75, storage.depth("history_depth")[1][2]);
    EXPECT_FALSE(storage.resource("history_depth").substituteDefault());
  }

  TEST(RenderResourceStorage, RejectsMismatchedExternalDepthBufferShape) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("history_depth", RenderResourceType::Depth),
    });

    Buffer<double> external(2, 3);

    EXPECT_THROW(storage.bindDepth("history_depth", external), std::runtime_error);
  }

  TEST(RenderResourceStorage, TracksSubstituteDefaultContents) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("color", RenderResourceType::Color),
    });

    EXPECT_FALSE(storage.resource("color").substituteDefault());

    storage.resource("color").clearSubstituteDefault(RenderPassKind::Beauty, Colord(0.1, 0.2, 0.3));

    EXPECT_TRUE(storage.resource("color").substituteDefault());
    EXPECT_EQ(Colord(0.1, 0.2, 0.3), storage.color("color")[0][0]);

    storage.resource("color").markProduced();

    EXPECT_FALSE(storage.resource("color").substituteDefault());
  }

  TEST(RenderResourceStorage, ClearDropsDescriptorsAndBuffers) {
    RenderResourceStorage storage;
    storage.allocate({
      resource("color", RenderResourceType::Color),
    });

    storage.clear();

    EXPECT_FALSE(storage.contains("color"));
    EXPECT_FALSE(storage.hasBuffer("color"));
  }
}
