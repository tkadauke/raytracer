#include <gtest/gtest.h>

#include "render/GpuDiffusePathLoopSceneSupport.h"

#include <cstdint>
#include <limits>

namespace GpuDiffusePathLoopSceneSupportTest {
  using namespace render;

  namespace {
    GpuDiffusePathLoopSceneSupportReasons reasons() {
      return {"max-depth", "geometry",    "material",        "texture",
              "light",     "environment", "display-resolve", "convergence"};
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

    void addSupportedMatteMaterial(GpuTracingSceneSections& sections) {
      sections.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
      sections.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
      sections.materials.push_back(materialRecord(GpuTracingMaterialKind::Unsupported));
      GpuTracingMaterialRecord matte = materialRecord(GpuTracingMaterialKind::Matte);
      matte.albedoTexture = 1;
      sections.materials.push_back(matte);
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
    GpuTracingMaterialRecord matte = materialRecord(GpuTracingMaterialKind::Matte);
    matte.albedoTexture = 1;
    sections.materials.push_back(matte);
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

  TEST(GpuDiffusePathLoopSceneSupport, RejectsConvergenceBeforeScenePolicyChecks) {
    GpuTracingSceneSections sections;
    sections.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Unsupported));
    sections.lights.push_back(lightRecord(GpuTracingLightKind::Unsupported));

    GpuDiffusePathLoopSettings convergence = settings();
    convergence.convergenceEnabled = true;

    expectUnsupported(GpuDiffusePathLoopSceneSupport().support(sections, convergence, reasons()),
                      "convergence");
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

  TEST(GpuDiffusePathLoopSceneSupport, RejectsNonFiniteLightRecords) {
    const float nan = std::numeric_limits<float>::quiet_NaN();

    GpuTracingSceneSections nonFinitePoint;
    nonFinitePoint.lights.push_back(lightRecord(GpuTracingLightKind::Point));
    nonFinitePoint.lights.back().positionOrDirection[0] = nan;
    expectUnsupported(supportFor(nonFinitePoint), "light");

    GpuTracingSceneSections nonFiniteDirectional;
    nonFiniteDirectional.lights.push_back(lightRecord(GpuTracingLightKind::Directional));
    nonFiniteDirectional.lights.back().parameters[1] = nan;
    expectUnsupported(supportFor(nonFiniteDirectional), "light");

    GpuTracingSceneSections nonFiniteArea;
    nonFiniteArea.lights.push_back(lightRecord(GpuTracingLightKind::RectangularArea));
    nonFiniteArea.lights.back().u[2] = nan;
    expectUnsupported(supportFor(nonFiniteArea), "light");
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsNonFiniteShaderRecords) {
    const float nan = std::numeric_limits<float>::quiet_NaN();

    GpuTracingSceneSections nonFiniteMaterial;
    nonFiniteMaterial.materials.push_back(materialRecord(GpuTracingMaterialKind::Unsupported));
    nonFiniteMaterial.materials.push_back(materialRecord(GpuTracingMaterialKind::Matte));
    nonFiniteMaterial.materials.back().parameters[1] = nan;
    expectUnsupported(supportFor(nonFiniteMaterial), "material");

    GpuTracingSceneSections nonFinitePortalMaterial;
    nonFinitePortalMaterial.materials.push_back(materialRecord(GpuTracingMaterialKind::Portal));
    nonFinitePortalMaterial.materials.back().portalOriginMatrix2[3] = nan;
    expectUnsupported(supportFor(nonFinitePortalMaterial), "material");

    GpuTracingSceneSections nonFiniteTexture;
    nonFiniteTexture.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    nonFiniteTexture.textures.back().parameters[0] = nan;
    expectUnsupported(supportFor(nonFiniteTexture), "texture");

    GpuTracingSceneSections nonFiniteEnvironment;
    nonFiniteEnvironment.environment.push_back(GpuTracingEnvironmentRecord{});
    nonFiniteEnvironment.environment.back().color[2] = nan;
    expectUnsupported(supportFor(nonFiniteEnvironment), "environment");
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsNonFiniteTintedTextureParameters) {
    const float nan = std::numeric_limits<float>::quiet_NaN();

    GpuTracingSceneSections nonFiniteTinted;
    nonFiniteTinted.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    nonFiniteTinted.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    GpuTracingTextureRecord tinted = textureRecord(GpuTracingTextureKind::Tinted);
    tinted.payloadOffset = 1;
    tinted.parameters[0] = nan;
    nonFiniteTinted.textures.push_back(tinted);
    expectUnsupported(supportFor(nonFiniteTinted), "texture");
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsSupportedMaterialsWithMalformedTextureReferences) {
    GpuTracingSceneSections matteWithMissingAlbedo;
    matteWithMissingAlbedo.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    matteWithMissingAlbedo.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    GpuTracingMaterialRecord matte = materialRecord(GpuTracingMaterialKind::Matte);
    matte.albedoTexture = 7;
    matteWithMissingAlbedo.materials.push_back(matte);
    expectUnsupported(supportFor(matteWithMissingAlbedo), "texture");

    GpuTracingSceneSections emissiveWithMissingEmission;
    emissiveWithMissingEmission.textures.push_back(
      textureRecord(GpuTracingTextureKind::Unsupported));
    emissiveWithMissingEmission.textures.push_back(
      textureRecord(GpuTracingTextureKind::ConstantColor));
    GpuTracingMaterialRecord emissive = materialRecord(GpuTracingMaterialKind::Emissive);
    emissive.emissionTexture = 7;
    emissiveWithMissingEmission.materials.push_back(emissive);
    expectUnsupported(supportFor(emissiveWithMissingEmission), "texture");
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsMaterialsThatReferenceUnsupportedTextureSentinel) {
    GpuTracingSceneSections sentinelAlbedo;
    sentinelAlbedo.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    sentinelAlbedo.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    GpuTracingMaterialRecord matte = materialRecord(GpuTracingMaterialKind::Matte);
    matte.albedoTexture = 0;
    sentinelAlbedo.materials.push_back(matte);
    expectUnsupported(supportFor(sentinelAlbedo), "texture");

    GpuTracingSceneSections sentinelEmission;
    sentinelEmission.textures.push_back(textureRecord(GpuTracingTextureKind::Unsupported));
    sentinelEmission.textures.push_back(textureRecord(GpuTracingTextureKind::ConstantColor));
    GpuTracingMaterialRecord emissive = materialRecord(GpuTracingMaterialKind::Emissive);
    emissive.emissionTexture = 0;
    sentinelEmission.materials.push_back(emissive);
    expectUnsupported(supportFor(sentinelEmission), "texture");
  }

  TEST(GpuDiffusePathLoopSceneSupport, ValidatesSupportedPrimitivePayloads) {
    GpuTracingSceneSections sections;
    addSupportedMatteMaterial(sections);
    sections.geometry.bvh.push_back(singlePrimitiveLeafNode());
    sections.geometry.primitives.push_back(primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    sections.geometry.primitives.back().material = 1;
    sections.geometry.spheres.push_back(GpuIntersectionSpherePayload{});

    const GpuDiffusePathLoopSceneSupport supportPolicy;
    EXPECT_FALSE(supportPolicy.hasNoGeometry(sections));
    EXPECT_TRUE(supportPolicy.hasSupportedGeometry(sections));
    expectSupported(supportFor(sections));
  }

  TEST(GpuDiffusePathLoopSceneSupport, ValidatesSupportedBranchBvh) {
    GpuTracingSceneSections sections;
    addSupportedMatteMaterial(sections);
    GpuIntersectionBvhNode branch;
    branch.leftOrFirstPrimitive = 1;
    branch.primitiveCount = 2;
    sections.geometry.bvh.push_back(branch);
    sections.geometry.bvh.push_back(singlePrimitiveLeafNode());
    sections.geometry.bvh.push_back(singlePrimitiveLeafNode());
    sections.geometry.bvh.back().leftOrFirstPrimitive = 1;
    sections.geometry.primitives.push_back(primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    sections.geometry.primitives.back().material = 1;
    sections.geometry.primitives.push_back(primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    sections.geometry.primitives.back().material = 1;
    sections.geometry.primitives.back().payloadOffset = 1;
    sections.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    sections.geometry.spheres.push_back(GpuIntersectionSpherePayload{});

    const GpuDiffusePathLoopSceneSupport supportPolicy;
    EXPECT_TRUE(supportPolicy.hasSupportedGeometry(sections));
    expectSupported(supportFor(sections));
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsSupportedGeometryWithInvalidMaterialReferences) {
    GpuTracingSceneSections missingMaterial;
    missingMaterial.geometry.bvh.push_back(singlePrimitiveLeafNode());
    missingMaterial.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    missingMaterial.geometry.primitives.back().material = 7;
    missingMaterial.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(missingMaterial), "geometry");

    GpuTracingSceneSections sentinelMaterial;
    addSupportedMatteMaterial(sentinelMaterial);
    sentinelMaterial.geometry.bvh.push_back(singlePrimitiveLeafNode());
    sentinelMaterial.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    sentinelMaterial.geometry.primitives.back().material = 0;
    sentinelMaterial.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(sentinelMaterial), "geometry");
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsSupportedGeometryWithMalformedBvhRanges) {
    GpuTracingSceneSections missingLeafPrimitive;
    addSupportedMatteMaterial(missingLeafPrimitive);
    missingLeafPrimitive.geometry.bvh.push_back(singlePrimitiveLeafNode());
    missingLeafPrimitive.geometry.bvh.back().leftOrFirstPrimitive = 1;
    missingLeafPrimitive.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    missingLeafPrimitive.geometry.primitives.back().material = 1;
    missingLeafPrimitive.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(missingLeafPrimitive), "geometry");

    GpuTracingSceneSections emptyLeaf;
    addSupportedMatteMaterial(emptyLeaf);
    emptyLeaf.geometry.bvh.push_back(singlePrimitiveLeafNode());
    emptyLeaf.geometry.bvh.back().primitiveCount = 0;
    emptyLeaf.geometry.primitives.push_back(primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    emptyLeaf.geometry.primitives.back().material = 1;
    emptyLeaf.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(emptyLeaf), "geometry");

    GpuTracingSceneSections missingBranchChild;
    addSupportedMatteMaterial(missingBranchChild);
    GpuIntersectionBvhNode branch;
    branch.leftOrFirstPrimitive = 1;
    branch.primitiveCount = 7;
    missingBranchChild.geometry.bvh.push_back(branch);
    missingBranchChild.geometry.bvh.push_back(singlePrimitiveLeafNode());
    missingBranchChild.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    missingBranchChild.geometry.primitives.back().material = 1;
    missingBranchChild.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(missingBranchChild), "geometry");

    GpuTracingSceneSections selfReferencingBranch;
    addSupportedMatteMaterial(selfReferencingBranch);
    GpuIntersectionBvhNode selfBranch;
    selfBranch.leftOrFirstPrimitive = 0;
    selfBranch.primitiveCount = 1;
    selfReferencingBranch.geometry.bvh.push_back(selfBranch);
    selfReferencingBranch.geometry.bvh.push_back(singlePrimitiveLeafNode());
    selfReferencingBranch.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    selfReferencingBranch.geometry.primitives.back().material = 1;
    selfReferencingBranch.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(selfReferencingBranch), "geometry");
  }

  TEST(GpuDiffusePathLoopSceneSupport, RejectsUnsupportedOrMalformedGeometry) {
    GpuTracingSceneSections missingBvh;
    addSupportedMatteMaterial(missingBvh);
    missingBvh.geometry.primitives.push_back(primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    missingBvh.geometry.primitives.back().material = 1;
    missingBvh.geometry.spheres.push_back(GpuIntersectionSpherePayload{});
    expectUnsupported(supportFor(missingBvh), "geometry");

    GpuTracingSceneSections unsupportedPrimitive;
    addSupportedMatteMaterial(unsupportedPrimitive);
    unsupportedPrimitive.geometry.bvh.push_back(singlePrimitiveLeafNode());
    unsupportedPrimitive.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Unsupported));
    unsupportedPrimitive.geometry.primitives.back().material = 1;
    expectUnsupported(supportFor(unsupportedPrimitive), "geometry");

    GpuTracingSceneSections missingPayload;
    addSupportedMatteMaterial(missingPayload);
    missingPayload.geometry.bvh.push_back(singlePrimitiveLeafNode());
    missingPayload.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    missingPayload.geometry.primitives.back().material = 1;
    expectUnsupported(supportFor(missingPayload), "geometry");

    GpuTracingSceneSections wrongPayloadCount;
    addSupportedMatteMaterial(wrongPayloadCount);
    wrongPayloadCount.geometry.bvh.push_back(singlePrimitiveLeafNode());
    wrongPayloadCount.geometry.primitives.push_back(
      primitiveRecord(GpuIntersectionPrimitiveKind::Sphere));
    wrongPayloadCount.geometry.primitives.back().material = 1;
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
