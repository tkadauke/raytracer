#include <gtest/gtest.h>

#include "render/GpuDiffusePathLoopSceneSupport.h"

#include <cstdint>

namespace GpuDiffusePathLoopSceneSupportTest {
  using namespace render;

  namespace {
    GpuDiffusePathLoopSceneSupportReasons reasons() {
      return {"max-depth", "geometry", "material", "texture", "light", "display-resolve"};
    }

    GpuDiffusePathLoopSettings settings() {
      GpuDiffusePathLoopSettings result;
      result.maxDepth = 1;
      return result;
    }

    GpuDiffusePathLoopBackendSupport supportFor(const GpuTracingSceneSections& sections) {
      return GpuDiffusePathLoopSceneSupport().support(sections, settings(), reasons());
    }

    GpuTracingMaterialRecord materialRecord(GpuTracingMaterialKind kind) {
      GpuTracingMaterialRecord record;
      record.kind = static_cast<std::uint32_t>(kind);
      return record;
    }

    GpuTracingTextureRecord textureRecord(GpuTracingTextureKind kind) {
      GpuTracingTextureRecord record;
      record.kind = static_cast<std::uint32_t>(kind);
      return record;
    }

    GpuTracingLightRecord lightRecord(GpuTracingLightKind kind) {
      GpuTracingLightRecord record;
      record.kind = static_cast<std::uint32_t>(kind);
      return record;
    }

    GpuIntersectionPrimitiveRecord primitiveRecord(GpuIntersectionPrimitiveKind kind) {
      GpuIntersectionPrimitiveRecord record;
      record.kind = static_cast<std::uint32_t>(kind);
      record.payloadCount = 1;
      return record;
    }

    GpuIntersectionBvhNode singlePrimitiveLeafNode() {
      GpuIntersectionBvhNode node;
      node.primitiveCount = 1;
      node.flags = gpuIntersectionLeafNodeFlag;
      return node;
    }

    void expectSupported(const GpuDiffusePathLoopBackendSupport& support) {
      EXPECT_TRUE(support.supported);
      EXPECT_TRUE(support.reason.empty());
    }

    void expectUnsupported(const GpuDiffusePathLoopBackendSupport& support,
                           const char* expectedReason) {
      EXPECT_FALSE(support.supported);
      EXPECT_EQ(expectedReason, support.reason);
    }
  }

  TEST(GpuDiffusePathLoopSceneSupport, AcceptsEmptyGeometryWithSupportedSceneRecords) {
    GpuTracingSceneSections sections;
    sections.materials.push_back(materialRecord(GpuTracingMaterialKind::Unsupported));
    sections.materials.push_back(materialRecord(GpuTracingMaterialKind::Matte));
    sections.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    sections.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    sections.lights.push_back(lightRecord(GpuTracingLightKind::Point));

    expectSupported(supportFor(sections));
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsZeroMaxDepthBeforeScenePolicyChecks) {
    GpuTracingSceneSections sections;
    sections.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Unsupported));
    sections.lights.push_back(lightRecord(GpuTracingLightKind::Unsupported));

    GpuDiffusePathLoopSettings zeroDepth = settings();
    zeroDepth.maxDepth = 0;

    expectUnsupported(GpuDiffusePathLoopSceneSupport().support(sections, zeroDepth, reasons()),
                      "max-depth");
  }

  TEST(GpuDiffusePathLoopSceneSupport,
       RejectsUnsupportedDisplayResolveTonemapOnlyWhenDisplayResolveRequested) {
    GpuDiffusePathLoopSettings unsupportedDisplayResolve = settings();
    unsupportedDisplayResolve.captureResolvedDisplay = true;
    unsupportedDisplayResolve.displayResolveTonemap = GpuDisplayResolveTonemap::Unsupported;

    expectUnsupported(GpuDiffusePathLoopSceneSupport().support(
                        GpuTracingSceneSections(), unsupportedDisplayResolve, reasons()),
                      "display-resolve");

    unsupportedDisplayResolve.captureResolvedDisplay = false;
    expectSupported(GpuDiffusePathLoopSceneSupport().support(GpuTracingSceneSections(),
                                                             unsupportedDisplayResolve, reasons()));
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsUnsupportedNonGeometryRecordsOnEmptyGeometry) {
    GpuTracingSceneSections unsupportedMaterial;
    unsupportedMaterial.materials.push_back(materialRecord(GpuTracingMaterialKind::Unsupported));
    unsupportedMaterial.materials.push_back(materialRecord(GpuTracingMaterialKind::Unsupported));
    expectUnsupported(supportFor(unsupportedMaterial), "material");

    GpuTracingSceneSections unsupportedTexture;
    unsupportedTexture.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    unsupportedTexture.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    expectUnsupported(supportFor(unsupportedTexture), "texture");

    GpuTracingSceneSections unsupportedLight;
    unsupportedLight.lights.push_back(lightRecord(GpuTracingLightKind::Unsupported));
    expectUnsupported(supportFor(unsupportedLight), "light");
  }

  TEST(GpuDiffusePathLoopSceneSupport, ValidatesSupportedPrimitivePayloads) {
    GpuTracingSceneSections sections;
    sections.geometry.bvh.push_back(singlePrimitiveLeafNode());
    sections.geometry.primitives.push_back(primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    sections.geometry.spheres.push_back(GpuIntersectionSpherePayload{});

    const GpuDiffusePathLoopSceneSupport supportPolicy;
    EXPECT_FALSE(supportPolicy.hasNoGeometry(sections));
    EXPECT_TRUE(supportPolicy.hasSupportedGeometry(sections));
    expectSupported(supportFor(sections));
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsUnsupportedOrMalformedGeometry) {
    GpuTracingSceneSections missingBvh;
    missingBvh.geometry.primitives.push_back(primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    missingBvh.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(missingBvh), "geometry");

    GpuTracingSceneSections unsupportedPrimitive;
    unsupportedPrimitive.geometry.bvh.push_back(singlePrimitiveLeafNode());
    unsupportedPrimitive.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Unsupported));
    expectUnsupported(supportFor(unsupportedPrimitive), "geometry");

    GpuTracingSceneSections missingPayload;
    missingPayload.geometry.bvh.push_back(singlePrimitiveLeafNode());
    missingPayload.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    expectUnsupported(supportFor(missingPayload), "geometry");

    GpuTracingSceneSections wrongPayloadCount;
    wrongPayloadCount.geometry.bvh.push_back(singlePrimitiveLeafNode());
    wrongPayloadCount.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    wrongPayloadCount.geometry.primitives.back().payloadCount = 2;
    wrongPayloadCount.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    wrongPayloadCount.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(wrongPayloadCount), "geometry");
  }

  TEST(GpuDiffusePathLoopSceneSupport, ValidatesSupportedTextureGraphs) {
    GpuTracingSceneSections sections;
    sections.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    sections.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    sections.textures.push_back(textureRecord(GpuTracingTextureKind::UVColor));
    GpuTracingTextureRecord checker = textureRecord(GpuTracingTextureKind::CheckerBoard);
    checker.payloadOffset = 1;
    checker.payloadCount = 2;
    checker.flags = static_cast<std::uint32_t>(GpuTracingTextureMappingKind::Planar);
    sections.textures.push_back(checker);

    const GpuDiffusePathLoopSceneSupport supportPolicy;
    EXPECT_TRUE(supportPolicy.hasSupportedTextures(sections));
    expectSupported(supportFor(sections));
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsUnsupportedTextureGraphShapes) {
    GpuTracingSceneSections unsupportedMapping;
    unsupportedMapping.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    unsupportedMapping.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    GpuTracingTextureRecord checker = textureRecord(GpuTracingTextureKind::CheckerBoard);
    checker.payloadOffset = 1;
    checker.payloadCount = 1;
    checker.flags = static_cast<std::uint32_t>(GpuTracingTextureMappingKind::None);
    unsupportedMapping.textures.push_back(checker);
    expectUnsupported(supportFor(unsupportedMapping), "texture");

    GpuTracingSceneSections missingChild;
    missingChild.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    GpuTracingTextureRecord tinted = textureRecord(GpuTracingTextureKind::Tinted);
    tinted.payloadOffset = 7;
    missingChild.textures.push_back(tinted);
    expectUnsupported(supportFor(missingChild), "texture");
  }
}
