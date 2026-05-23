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
