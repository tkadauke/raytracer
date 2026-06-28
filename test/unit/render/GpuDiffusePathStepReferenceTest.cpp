#include <gtest/gtest.h>

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/geometry/Mesh.h"
#include "core/geometry/Polyline.h"
#include "core/math/Constants.h"
#include "core/math/Matrix.h"
#include "test/helpers/ColorTestHelper.h"
#include "test/helpers/VectorTestHelper.h"

#include "render/GpuDiffusePathLoopBackend.h"
#include "render/GpuDiffusePathLoopLaunch.h"
#include "render/GpuDiffusePathStepReference.h"
#include "render/IntersectionSceneCompiler.h"
#include "render/MIS.h"
#include "render/MetalGpuDiffusePathLoopBackend.h"
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
#include "render/MetalGpuDiffusePathFrontierCompactionBackend.h"
#include "render/MetalGpuDiffusePathLoopKernel.h"
#endif
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
#include "render/VulkanGpuDiffusePathFrontierCompactionBackend.h"
#include "render/VulkanGpuDiffusePathLoopKernel.h"
#endif
#include "render/VulkanGpuDiffusePathLoopBackend.h"
#include "render/animation/AnimationTrack.h"
#include "render/cameras/EquirectangularCamera.h"
#include "render/cameras/FishEyeCamera.h"
#include "render/cameras/OrthographicCamera.h"
#include "render/cameras/PinholeCamera.h"
#include "render/cameras/SphericalCamera.h"
#include "render/cameras/ThinLensCamera.h"
#include "render/cameras/TiltShiftCamera.h"
#include "render/lights/DirectionalLight.h"
#include "render/lights/PointLight.h"
#include "render/lights/RectangularAreaLight.h"
#include "render/materials/EmissiveMaterial.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/PortalMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/primitives/Box.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Disk.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/primitives/OpenCylinder.h"
#include "render/primitives/Plane.h"
#include "render/primitives/Rectangle.h"
#include "render/primitives/Scene.h"
#include "render/primitives/Sphere.h"
#include "render/primitives/Torus.h"
#include "render/primitives/Triangle.h"
#include "render/samplers/GpuSampleStream.h"
#include "render/samplers/Sampler.h"
#include "render/textures/CheckerBoardTexture.h"
#include "render/textures/ConstantColorTexture.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/TintedTexture.h"
#include "render/textures/UVColorTexture.h"
#include "render/textures/mappings/PlanarMapping2D.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

namespace GpuDiffusePathStepReferenceTest {
  using namespace render;

  namespace {
    class UnsupportedGpuTracingMaterial final : public Material {
    public:
      Colord shade(const RayCaster*, const Scene&, const Rayd&, const HitPoint&,
                   State&) const override {
        return Colord::black();
      }
    };

    class UnsupportedGpuTracingTexture final : public Texturec {
    public:
      Colord evaluate(const Rayd&, const HitPoint&) const override {
        return Colord::white();
      }
    };

    class UnsupportedGpuTracingPrimitive final : public Primitive {
    public:
      const Primitive* intersect(const Rayd&, HitPointInterval&, State&) const override {
        return nullptr;
      }

    protected:
      BoundingBoxd calculateBoundingBox() const override {
        return BoundingBoxd(Vector3d(-1.0, -1.0, -1.0), Vector3d(1.0, 1.0, 1.0));
      }
    };

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    std::vector<std::uint32_t> sortedRetainedPathIndices(std::vector<std::uint32_t> indices) {
      std::sort(indices.begin(), indices.end());
      return indices;
    }
#endif

    GpuDiffusePathStateRecord activePath(std::uint32_t rayIndex = 7) {
      GpuDiffusePathStateRecord path = makeActiveGpuDiffusePathState();
      path.ray = GpuIntersectionScenePacker().packRay(
        Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), rayIndex);
      path.pixelIndex = 3;
      path.primarySampleIndex = 2;
      path.sampleSeed = 12345;
      path.sampleDimensionBase = static_cast<std::uint32_t>(SampleDimension::BSDF);
      path.sampleDimensionStride = 4;
      return path;
    }

    GpuDiffusePathStateRecord activePath(const Rayd& ray, std::uint32_t rayIndex) {
      GpuDiffusePathStateRecord path = activePath(rayIndex);
      path.ray = GpuIntersectionScenePacker().packRay(ray, rayIndex);
      return path;
    }

    Vector3d equirectangularLocalDirection(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                                           double x, double y) {
      const double viewWidth = descriptor.lensParameters[0];
      const double viewHeight = descriptor.lensParameters[1];
      const double lon = (2.0 * x / viewWidth - 1.0) * PI;
      const double lat = (1.0 - 2.0 * y / viewHeight) * (PI / 2.0);
      const double cosLat = std::cos(lat);
      return Vector3d(cosLat * std::sin(lon), -std::sin(lat), cosLat * std::cos(lon));
    }

    Vector3d
    equirectangularDescriptorDirection(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                                       double x, double y) {
      const Vector3d local = equirectangularLocalDirection(descriptor, x, y);
      const Vector3d right(descriptor.right);
      const Vector3d down(descriptor.down);
      const Vector3d forward(descriptor.forward);
      return (right * local.x() + down * local.y() + forward * local.z()).normalized();
    }

    Vector3d sphericalLocalDirection(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                                     double x, double y) {
      const double viewWidth = descriptor.lensParameters[0];
      const double viewHeight = descriptor.lensParameters[1];
      const double horizontalFov = descriptor.lensParameters[2];
      const double verticalFov = descriptor.lensParameters[3];
      const double pointX = 2.0 / viewWidth * x + 1.0;
      const double pointY = 2.0 / viewHeight * y - 1.0;
      const double lambda = pointX * 0.5 * horizontalFov;
      const double psi = pointY * 0.5 * verticalFov;
      const double phi = PI - lambda;
      const double theta = 0.5 * PI - psi;
      return Vector3d(std::sin(theta) * std::sin(phi), std::cos(theta),
                      std::sin(theta) * std::cos(phi));
    }

    Vector3d sphericalDescriptorDirection(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                                          double x, double y) {
      const Vector3d local = sphericalLocalDirection(descriptor, x, y);
      const Vector3d right(descriptor.right);
      const Vector3d down(descriptor.down);
      const Vector3d forward(descriptor.forward);
      return (right * local.x() + down * local.y() + forward * local.z()).normalized();
    }

    std::optional<Vector3d>
    fishEyeLocalDirection(const GpuRectilinearPrimaryPathDescriptor& descriptor, double x,
                          double y) {
      const double viewWidth = descriptor.lensParameters[0];
      const double viewHeight = descriptor.lensParameters[1];
      const double fieldOfView = descriptor.lensParameters[2];
      const Vector2d point(2.0 / viewWidth * x - 1.0, 2.0 / viewHeight * y - 1.0);
      const double r2 = point * point;
      if (r2 > 1.0 || r2 <= std::numeric_limits<double>::epsilon()) {
        return std::nullopt;
      }
      const double r = std::sqrt(r2);
      const double psi = r * fieldOfView / 2.0;
      const double sinPsi = std::sin(psi);
      const double cosPsi = std::cos(psi);
      const double sinAlpha = point.y() / r;
      const double cosAlpha = point.x() / r;
      return Vector3d(sinPsi * cosAlpha, sinPsi * sinAlpha, cosPsi);
    }

    void fillEchoedLaunchParameters(GpuDiffusePathLoopPlatformResult& platform,
                                    std::uint32_t initialPathCount,
                                    const GpuDiffusePathLoopSettings& settings,
                                    std::uint32_t width = 1, std::uint32_t height = 1) {
      platform.echoedParameters.layoutVersion = gpuDiffusePathLoopLaunchLayoutVersion;
      platform.echoedParameters.maxDepth = settings.maxDepth;
      platform.echoedParameters.russianRouletteDepth = settings.russianRouletteDepth;
      platform.echoedParameters.directLightSamples = settings.directLightSamples;
      platform.echoedParameters.captureDiagnostics = settings.captureDiagnostics ? 1u : 0u;
      platform.echoedParameters.captureMetrics = settings.captureMetrics ? 1u : 0u;
      platform.echoedParameters.captureDenoiserFeatures =
        settings.captureDenoiserFeatures ? 1u : 0u;
      platform.echoedParameters.displayResolveTonemap =
        static_cast<std::uint32_t>(settings.displayResolveTonemap);
      platform.echoedParameters.initialPathCount = initialPathCount;
      platform.echoedParameters.imageWidth = width;
      platform.echoedParameters.imageHeight = height;
    }

    class RecordingFrontierCompactionBackend final
        : public GpuDiffusePathFrontierCompactionBackend {
    public:
      const char* name() const override {
        return "recording_diffuse_frontier_compaction";
      }

      const char* pathStateResidency() const override {
        return "recording_path_state";
      }

      GpuDiffusePathFrontierCompactionResult
      compact(const std::vector<GpuDiffusePathStateRecord>& sourceRecords,
              const std::vector<std::uint32_t>& retainedPathIndices) const override {
        ++calls;
        inputCounts.push_back(sourceRecords.size());
        retainedIndices.push_back(retainedPathIndices);

        GpuDiffusePathFrontierCompactionResult result;
        result.executionPath = name();
        result.pathStateResidency = pathStateResidency();
        result.inputPathCount = sourceRecords.size();
        result.retainedPathIndices = retainedPathIndices;
        result.uploadWorkerSeconds = uploadWorkerSeconds;
        result.kernelWorkerSeconds = kernelWorkerSeconds;
        result.readbackWorkerSeconds = readbackWorkerSeconds;
        for (const std::uint32_t index : retainedPathIndices) {
          result.retainedRecords.push_back(sourceRecords[index]);
        }
        return result;
      }

      mutable int calls{0};
      mutable std::vector<std::size_t> inputCounts;
      mutable std::vector<std::vector<std::uint32_t>> retainedIndices;
      double uploadWorkerSeconds{0.01};
      double kernelWorkerSeconds{0.02};
      double readbackWorkerSeconds{0.03};
    };

    class AvailableFullGpuPathLoopBackend : public GpuDiffusePathLoopBackend {
    public:
      const char* name() const override {
        return "available_full_gpu_path_loop";
      }

      bool fullGpuPathLoopAvailable() const override {
        return true;
      }

      GpuDiffusePathLoopResult run(const GpuTracingSceneSections&,
                                   const std::vector<GpuDiffusePathStateRecord>&,
                                   const GpuDiffusePathLoopSettings&) const override {
        return {};
      }
    };

    class SceneRejectingFullGpuPathLoopBackend final : public AvailableFullGpuPathLoopBackend {
    public:
      GpuDiffusePathLoopBackendSupport
      fullGpuPathLoopSupport(const GpuTracingSceneSections&) const override {
        return {false, "test backend supports only a narrower scene subset"};
      }

      GpuDiffusePathLoopBackendSupport
      fullGpuPathLoopSupport(const GpuTracingSceneSections&,
                             const GpuDiffusePathLoopSettings&) const override {
        return {false, "test backend supports only a narrower scene subset"};
      }
    };

    class UnavailableFullGpuPathLoopBackend final : public AvailableFullGpuPathLoopBackend {
    public:
      bool fullGpuPathLoopAvailable() const override {
        return false;
      }

      const char* fullGpuPathLoopUnavailableReason() const override {
        return "test backend is offline";
      }
    };

    GpuIntersectionHitRecord hitRecord(std::uint32_t rayIndex, std::uint32_t material) {
      GpuIntersectionHitRecord hit;
      hit.hit = 1;
      hit.rayIndex = rayIndex;
      hit.material = material;
      hit.object = 9;
      hit.primitiveRecord = 5;
      hit.distance = 4.0f;
      hit.point = {0.0f, 0.0f, 0.0f, 1.0f};
      hit.normal = {0.0f, 0.0f, -1.0f, 0.0f};
      return hit;
    }

    GpuTracingSceneSections sectionsFor(Scene& scene) {
      const CompiledIntersectionScene intersection = IntersectionSceneCompiler().compile(scene);
      GpuTracingSceneSections sections;
      sections.geometry = GpuIntersectionScenePacker().packScene(intersection);
      const GpuTracingMaterialCompilation materials = compileGpuTracingMaterials(intersection);
      sections.materials = materials.records;
      sections.textures = materials.textures.records;
      sections.lights = compileGpuTracingLights(scene).records;
      sections.environment.push_back(makeGpuTracingConstantEnvironment(scene.background()));
      if (scene.environmentRadiance() != scene.background()) {
        sections.environment.push_back(
          makeGpuTracingConstantEnvironment(scene.environmentRadiance()));
      }
      return sections;
    }

    [[maybe_unused]] std::shared_ptr<Mesh> triangleMesh() {
      auto mesh = std::make_shared<Mesh>();
      mesh->addVertex(Vector3d(0.0, 0.0, 0.0), Vector3d(0.0, 0.0, 1.0), Vector2d(0.0, 0.0));
      mesh->addVertex(Vector3d(1.0, 0.0, 0.0), Vector3d(0.0, 0.0, 1.0), Vector2d(1.0, 0.0));
      mesh->addVertex(Vector3d(0.0, 1.0, 0.0), Vector3d(0.0, 0.0, 1.0), Vector2d(0.0, 1.0));
      mesh->addFace({0, 1, 2});
      return mesh;
    }

    void expectFloat4Near(const std::array<float, 4>& actual, const std::array<float, 4>& expected,
                          double tolerance);
    void expectPathStateNear(const GpuDiffusePathStateRecord& actual,
                             const GpuDiffusePathStateRecord& expected, double tolerance);

    struct GpuPathLoopCase {
      GpuTracingSceneSections sections;
      std::vector<GpuDiffusePathStateRecord> paths;
      GpuDiffusePathLoopSettings settings;
    };

    std::shared_ptr<MatteMaterial> gpuPathLoopMatte(const Colord& color = Colord(0.25, 0.5, 0.75),
                                                    double diffuseCoefficient = 0.8) {
      auto material =
        std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(color));
      material->setDiffuseCoefficient(diffuseCoefficient);
      return material;
    }

    GpuDiffusePathLoopSettings singleBounceGpuPathLoopSettings() {
      GpuDiffusePathLoopSettings settings;
      settings.maxDepth = 1;
      settings.russianRouletteDepth = 10;
      settings.directLightSamples = 1;
      return settings;
    }

    [[maybe_unused]] GpuPathLoopCase meshPrimitiveGpuPathLoopCase() {
      Scene scene;
      auto meshPrimitive =
        std::make_shared<MeshPrimitive>(triangleMesh(), MeshPrimitive::NormalMode::Smooth);
      meshPrimitive->setMaterial(gpuPathLoopMatte(Colord(0.6, 0.5, 0.4), 0.9));
      scene.add(meshPrimitive);

      GpuDiffusePathStateRecord path =
        activePath(Rayd(Vector4d(0.25, 0.25, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 22);
      path.pixelIndex = 0;
      path.sampleSeed = 12347;

      return {sectionsFor(scene), {path}, singleBounceGpuPathLoopSettings()};
    }

    [[maybe_unused]] GpuPathLoopCase boxGpuPathLoopCase() {
      Scene scene;
      auto box = std::make_shared<Box>(Vector3d(0.0, 0.0, 3.0), Vector3d(1.0, 1.0, 1.0));
      box->setMaterial(gpuPathLoopMatte(Colord(0.25, 0.5, 0.75), 0.8));
      scene.add(box);

      GpuDiffusePathStateRecord path =
        activePath(Rayd(Vector4d(0.25, 0.5, 0.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 23);
      path.pixelIndex = 0;
      path.sampleSeed = 12347;

      return {sectionsFor(scene), {path}, singleBounceGpuPathLoopSettings()};
    }

    [[maybe_unused]] GpuPathLoopCase curveGpuPathLoopCase() {
      Scene scene;
      auto curve =
        std::make_shared<Curve>(core::Polyline({Vector3d(0.0, 0.0, 3.0), Vector3d(1.0, 0.0, 3.0)}),
                                0.5, Curve::TessellationMode::Ribbon);
      curve->setMaterial(gpuPathLoopMatte(Colord(0.7, 0.35, 0.2), 0.85));
      scene.add(curve);

      GpuDiffusePathStateRecord path =
        activePath(Rayd(Vector4d(0.5, 1.0, 3.0, 1.0), Vector3d(0.0, -1.0, 0.0)), 24);
      path.pixelIndex = 0;
      path.sampleSeed = 12347;

      return {sectionsFor(scene), {path}, singleBounceGpuPathLoopSettings()};
    }

    std::shared_ptr<MatteMaterial> directLightTestMatte() {
      auto matte =
        std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
      matte->setDiffuseCoefficient(1.0);
      return matte;
    }

    GpuDiffusePathStateRecord directLightTestPath() {
      GpuDiffusePathStateRecord path = activePath();
      path.pixelIndex = 0;
      path.sampleSeed = 12347;
      path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};
      return path;
    }

    GpuDiffusePathLoopSettings directLightTestSettings(std::uint32_t directLightSamples = 1u) {
      GpuDiffusePathLoopSettings settings;
      settings.maxDepth = 1;
      settings.russianRouletteDepth = 10;
      settings.directLightSamples = directLightSamples;
      return settings;
    }

    GpuPathLoopCase directionalLightGpuPathLoopCase() {
      Scene scene;
      auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
      receiver->setMaterial(directLightTestMatte());
      scene.add(receiver);
      scene.addLight(
        std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord(0.8, 0.6, 0.4)));
      return {sectionsFor(scene), {directLightTestPath()}, directLightTestSettings()};
    }

    GpuPathLoopCase rectangularAreaLightGpuPathLoopCase() {
      Scene scene;
      auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
      receiver->setMaterial(directLightTestMatte());
      scene.add(receiver);
      scene.addLight(
        std::make_shared<RectangularAreaLight>(Vector3d(0.0, 2.0, -3.0), Vector3d(2.0, 0.0, 0.0),
                                               Vector3d(0.0, 2.0, 0.0), Colord(0.8, 0.6, 0.4)));
      return {sectionsFor(scene), {directLightTestPath()}, directLightTestSettings()};
    }

    GpuPathLoopCase multipleLightGpuPathLoopCase() {
      Scene scene;
      auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
      receiver->setMaterial(directLightTestMatte());
      scene.add(receiver);
      scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.2, 0.1)));
      scene.addLight(
        std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord(0.1, 0.7, 0.2)));
      scene.addLight(
        std::make_shared<RectangularAreaLight>(Vector3d(0.0, 2.0, -3.0), Vector3d(2.0, 0.0, 0.0),
                                               Vector3d(0.0, 2.0, 0.0), Colord(0.3, 0.4, 0.9)));
      return {sectionsFor(scene), {directLightTestPath()}, directLightTestSettings(4u)};
    }

    [[maybe_unused]] void expectBackendPathLoopMatchesReference(
      const GpuDiffusePathLoopBackend& backend, const GpuPathLoopCase& testCase) {
      const GpuDiffusePathLoopResult expected =
        GpuDiffusePathLoop().run(testCase.sections, testCase.paths, testCase.settings);
      const GpuDiffusePathLoopResult result =
        backend.run(testCase.sections, testCase.paths, testCase.settings);

      EXPECT_TRUE(result.fullGpuPathLoopSupported());
      EXPECT_EQ(expected.depthCount, result.depthCount);
      EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
      ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
      expectFloat4Near(result.stepRecords[0].continuationThroughput,
                       expected.stepRecords[0].continuationThroughput, 1e-4);
      ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
      expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    }

    void expectBackendDirectLightPathLoopMatchesReference(const GpuDiffusePathLoopBackend& backend,
                                                          const GpuPathLoopCase& testCase) {
      const GpuDiffusePathLoopResult expected =
        GpuDiffusePathLoop().run(testCase.sections, testCase.paths, testCase.settings);
      const GpuDiffusePathLoopResult result =
        backend.run(testCase.sections, testCase.paths, testCase.settings);

      EXPECT_TRUE(result.fullGpuPathLoopSupported());
      ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
      expectFloat4Near(result.stepRecords[0].directLightRadiance,
                       expected.stepRecords[0].directLightRadiance, 1e-4);
      ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
      expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    }

    std::uint32_t firstMaterialId(const GpuTracingSceneSections& sections,
                                  GpuTracingMaterialKind kind) {
      for (std::uint32_t index = 0; index != sections.materials.size(); ++index) {
        if (sections.materials[index].kind == static_cast<std::uint32_t>(kind)) {
          return index;
        }
      }
      return 0;
    }

    std::shared_ptr<Texturec> nestedTintedConstantTexture() {
      return std::make_shared<TintedTexture>(
        std::make_shared<TintedTexture>(
          std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)),
          Colord(0.5, 0.25, 0.125)),
        Colord(0.5, 0.5, 0.5));
    }

    Colord nestedTintedConstantTextureColor() {
      return Colord(0.0625, 0.0625, 0.046875);
    }

    Colord checkerTextureGraphDarkColor() {
      return Colord(0.1875, 0.25, 0.1875);
    }

    std::shared_ptr<Texturec> checkerTextureGraph(TextureMapping2D* mapping) {
      auto darkTexture = std::make_shared<TintedTexture>(
        std::make_shared<ConstantColorTexture>(Colord(0.75, 0.5, 0.25)), Colord(0.25, 0.5, 0.75));
      return std::make_shared<CheckerBoardTexture>(mapping, nestedTintedConstantTexture(),
                                                   darkTexture);
    }

    void expectFloat4Near(const std::array<float, 4>& actual, const std::array<float, 4>& expected,
                          double tolerance = 1e-5) {
      for (std::size_t index = 0; index != actual.size(); ++index) {
        EXPECT_NEAR(expected[index], actual[index], tolerance);
      }
    }

    void expectHitRecordNear(const GpuIntersectionHitRecord& actual,
                             const GpuIntersectionHitRecord& expected, double tolerance = 1e-5) {
      EXPECT_EQ(expected.hit, actual.hit);
      EXPECT_EQ(expected.material, actual.material);
      EXPECT_EQ(expected.object, actual.object);
      EXPECT_EQ(expected.primitiveRecord, actual.primitiveRecord);
      EXPECT_EQ(expected.rayIndex, actual.rayIndex);
      if (std::isinf(expected.distance) || std::isinf(actual.distance)) {
        EXPECT_EQ(expected.distance, actual.distance);
      } else {
        EXPECT_NEAR(expected.distance, actual.distance, tolerance);
      }
      expectFloat4Near(actual.point, expected.point, tolerance);
      expectFloat4Near(actual.normal, expected.normal, tolerance);
      expectFloat4Near(actual.uv, expected.uv, tolerance);
      expectFloat4Near(actual.barycentric, expected.barycentric, tolerance);
    }

    void expectStepRecordEqual(const GpuDiffusePathStepRecord& actual,
                               const GpuDiffusePathStepRecord& expected) {
      EXPECT_EQ(expected.event, actual.event);
      EXPECT_EQ(expected.pathIndex, actual.pathIndex);
      EXPECT_EQ(expected.pixelIndex, actual.pixelIndex);
      EXPECT_EQ(expected.primarySampleIndex, actual.primarySampleIndex);
      EXPECT_EQ(expected.depth, actual.depth);
      EXPECT_EQ(expected.material, actual.material);
      EXPECT_EQ(expected.object, actual.object);
      EXPECT_EQ(expected.flags, actual.flags);
      EXPECT_EQ(expected.emittedRadiance, actual.emittedRadiance);
      EXPECT_EQ(expected.directLightRadiance, actual.directLightRadiance);
      EXPECT_EQ(expected.missRadiance, actual.missRadiance);
      EXPECT_EQ(expected.continuationThroughput, actual.continuationThroughput);
      EXPECT_EQ(expected.directLightSampleCount, actual.directLightSampleCount);
      EXPECT_EQ(expected.directLightVisibilityRayCount, actual.directLightVisibilityRayCount);
      EXPECT_EQ(expected.directLightContributingSampleCount,
                actual.directLightContributingSampleCount);
      EXPECT_EQ(expected.directLightOccludedSampleCount, actual.directLightOccludedSampleCount);
    }

    void expectGpuRayNear(const GpuIntersectionRay& actual, const GpuIntersectionRay& expected,
                          double tolerance = 1e-5) {
      expectFloat4Near(actual.origin, expected.origin, tolerance);
      expectFloat4Near(actual.direction, expected.direction, tolerance);
      EXPECT_NEAR(expected.minDistance, actual.minDistance, tolerance);
      if (std::isinf(expected.maxDistance) || std::isinf(actual.maxDistance)) {
        EXPECT_EQ(expected.maxDistance, actual.maxDistance);
      } else {
        EXPECT_NEAR(expected.maxDistance, actual.maxDistance, tolerance);
      }
      EXPECT_NEAR(expected.timeSample, actual.timeSample, tolerance);
      EXPECT_EQ(expected.flags, actual.flags);
      EXPECT_EQ(expected.rayIndex, actual.rayIndex);
    }

    void expectPathStateNear(const GpuDiffusePathStateRecord& actual,
                             const GpuDiffusePathStateRecord& expected, double tolerance = 1e-5) {
      expectGpuRayNear(actual.ray, expected.ray, tolerance);
      expectFloat4Near(actual.throughput, expected.throughput, tolerance);
      expectFloat4Near(actual.accumulatedRadiance, expected.accumulatedRadiance, tolerance);
      EXPECT_EQ(expected.pixelIndex, actual.pixelIndex);
      EXPECT_EQ(expected.primarySampleIndex, actual.primarySampleIndex);
      EXPECT_EQ(expected.depth, actual.depth);
      EXPECT_EQ(expected.sampleSeed, actual.sampleSeed);
      EXPECT_EQ(expected.sampleDimensionBase, actual.sampleDimensionBase);
      EXPECT_EQ(expected.sampleDimensionStride, actual.sampleDimensionStride);
      EXPECT_EQ(expected.flags, actual.flags);
      EXPECT_NEAR(expected.previousBsdfPdf, actual.previousBsdfPdf, tolerance);
      EXPECT_NEAR(expected.previousLightPdf, actual.previousLightPdf, tolerance);
      EXPECT_EQ(expected.previousMaterial, actual.previousMaterial);
      EXPECT_EQ(expected.previousEventFlags, actual.previousEventFlags);
    }

    void expectOcclusionRecordEqual(const GpuIntersectionOcclusionRecord& actual,
                                    const GpuIntersectionOcclusionRecord& expected) {
      EXPECT_EQ(expected.occluded, actual.occluded);
      EXPECT_EQ(expected.rayIndex, actual.rayIndex);
    }

    void expectStepResultParity(const GpuDiffusePathStepResult& actual,
                                const GpuDiffusePathStepResult& expected) {
      ASSERT_EQ(expected.closestHitRecords.size(), actual.closestHitRecords.size());
      for (std::size_t index = 0; index != actual.closestHitRecords.size(); ++index) {
        expectHitRecordNear(actual.closestHitRecords[index], expected.closestHitRecords[index]);
      }

      ASSERT_EQ(expected.stepRecords.size(), actual.stepRecords.size());
      for (std::size_t index = 0; index != actual.stepRecords.size(); ++index) {
        expectStepRecordEqual(actual.stepRecords[index], expected.stepRecords[index]);
      }

      ASSERT_EQ(expected.pathStates.size(), actual.pathStates.size());
      for (std::size_t index = 0; index != actual.pathStates.size(); ++index) {
        expectPathStateNear(actual.pathStates[index], expected.pathStates[index]);
      }

      ASSERT_EQ(expected.terminatedPathStates.size(), actual.terminatedPathStates.size());
      for (std::size_t index = 0; index != actual.terminatedPathStates.size(); ++index) {
        expectPathStateNear(actual.terminatedPathStates[index],
                            expected.terminatedPathStates[index]);
      }

      ASSERT_EQ(expected.directLightShadowRays.size(), actual.directLightShadowRays.size());
      for (std::size_t index = 0; index != actual.directLightShadowRays.size(); ++index) {
        expectGpuRayNear(actual.directLightShadowRays[index],
                         expected.directLightShadowRays[index]);
      }

      ASSERT_EQ(expected.directLightOcclusionRecords.size(),
                actual.directLightOcclusionRecords.size());
      for (std::size_t index = 0; index != actual.directLightOcclusionRecords.size(); ++index) {
        expectOcclusionRecordEqual(actual.directLightOcclusionRecords[index],
                                   expected.directLightOcclusionRecords[index]);
      }
    }

    std::vector<GpuIntersectionHitRecord>
    closestHitsFor(const GpuTracingSceneSections& sections,
                   const std::vector<GpuDiffusePathStateRecord>& paths) {
      std::vector<GpuIntersectionRay> rays;
      for (const GpuDiffusePathStateRecord& path : paths) {
        if (gpuDiffusePathStateIsActive(path)) {
          rays.push_back(path.ray);
        }
      }
      return GpuIntersectionIntersector().intersectClosest(sections.geometry, rays);
    }

    Vector3d expectedCosineHemisphereDirection(const Vector3d& normal, const Vector2d& sample) {
      const double r = std::sqrt(sample.x());
      const double phi = TAU * sample.y();
      const Vector3d helper = std::abs(normal.y()) < 0.999 ? Vector3d::up() : Vector3d::right();
      const Vector3d tangent = (helper ^ normal).normalized();
      const Vector3d bitangent = normal ^ tangent;
      return (tangent * (r * std::cos(phi)) + bitangent * (r * std::sin(phi)) +
              normal * std::sqrt(std::max(0.0, 1.0 - sample.x())))
        .normalized();
    }

    Vector2d expectedConcentricDiscSample(const Vector2d& sample) {
      const double a = 2.0 * sample.x() - 1.0;
      const double b = 2.0 * sample.y() - 1.0;

      if (a == 0.0 && b == 0.0) {
        return Vector2d(0.0, 0.0);
      }

      double r = 0.0;
      double phi = 0.0;
      if (a * a > b * b) {
        r = a;
        phi = PI_OVER_4 * (b / a);
      } else {
        r = b;
        phi = PI_OVER_2 - PI_OVER_4 * (a / b);
      }
      return Vector2d(r * std::cos(phi), r * std::sin(phi));
    }

    struct DescriptorRayBasis {
      Vector3d origin;
      Vector3d right;
      Vector3d down;
      Vector3d forward;
    };

    DescriptorRayBasis descriptorRayBasisAt(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                                            double timeSample) {
      if (descriptor.motionMode != gpuPrimaryPathMotionModeLookAt) {
        return DescriptorRayBasis{Vector3d(descriptor.originOrDirection) +
                                    Vector3d(descriptor.motionOriginDelta) * timeSample,
                                  Vector3d(descriptor.right), Vector3d(descriptor.down),
                                  Vector3d(descriptor.forward)};
      }

      const Vector3d position = Vector3d(descriptor.originOrDirection) +
                                Vector3d(descriptor.motionOriginDelta) * timeSample;
      const Vector3d target =
        Vector3d(descriptor.motionTarget) + Vector3d(descriptor.motionTargetDelta) * timeSample;
      const Matrix4d matrix = Matrix4d::lookAt(position, target, Vector3d::up());
      return DescriptorRayBasis{
        matrix.transformPoint(Vector3d(0.0, 0.0, -descriptor.motionParameters[0])),
        matrix.transformDirection(Vector3d(1.0, 0.0, 0.0)),
        matrix.transformDirection(Vector3d(0.0, 1.0, 0.0)),
        matrix.transformDirection(Vector3d(0.0, 0.0, 1.0))};
    }

    Vector3d descriptorRectilinearPixelPoint(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                                             const DescriptorRayBasis& basis,
                                             const Vector2d& pixelSample) {
      const Vector3d pixelPoint = Vector3d(descriptor.topLeft) +
                                  Vector3d(descriptor.right) * pixelSample.x() +
                                  Vector3d(descriptor.down) * pixelSample.y();
      if (descriptor.motionMode != gpuPrimaryPathMotionModeLookAt) {
        return pixelPoint;
      }
      return basis.origin + basis.forward * descriptor.motionParameters[0] +
             basis.right * pixelPoint.x() + basis.down * pixelPoint.y() +
             basis.forward * pixelPoint.z();
    }

    Rayd thinLensDescriptorRay(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                               const Vector2d& pixelSample, const Vector2d& lensSample,
                               double timeSample) {
      const DescriptorRayBasis basis = descriptorRayBasisAt(descriptor, timeSample);
      const Vector3d origin = basis.origin;
      const Vector3d pixelPoint = descriptorRectilinearPixelPoint(descriptor, basis, pixelSample);
      const Vector3d focalForward = basis.forward.normalized();
      const Vector3d pinholeDirection = (pixelPoint - origin).normalized();
      const double denominator = pinholeDirection * focalForward;
      const Vector3d focalPoint =
        origin + pinholeDirection * (descriptor.lensParameters[0] / denominator);
      const double apertureRadius = Vector3d(descriptor.lensRight).length();
      const Vector3d lensRight = descriptor.motionMode == gpuPrimaryPathMotionModeLookAt
                                   ? basis.right * apertureRadius
                                   : Vector3d(descriptor.lensRight);
      const Vector3d lensUp = descriptor.motionMode == gpuPrimaryPathMotionModeLookAt
                                ? basis.down * apertureRadius
                                : Vector3d(descriptor.lensUp);
      const Vector3d lensOrigin = origin + lensRight * lensSample.x() + lensUp * lensSample.y();
      return Rayd(lensOrigin, (focalPoint - lensOrigin).normalized());
    }

    Rayd tiltShiftDescriptorRay(const GpuRectilinearPrimaryPathDescriptor& descriptor,
                                const Vector2d& pixelSample, const Vector2d& lensSample,
                                double timeSample) {
      const DescriptorRayBasis basis = descriptorRayBasisAt(descriptor, timeSample);
      const Vector3d origin = basis.origin;
      const Vector3d pixelPoint = descriptorRectilinearPixelPoint(descriptor, basis, pixelSample);
      const Vector3d focalForward = basis.forward.normalized();
      const Vector3d rightBasis = basis.right.normalized();
      const Vector3d upBasis = basis.down.normalized();
      const double shiftX = descriptor.lensParameters[1];
      const double shiftY = descriptor.lensParameters[2];
      const double tiltRadians = descriptor.lensParameters[3];
      const Vector3d shiftedPixelPoint = pixelPoint + rightBasis * shiftX + upBasis * shiftY;
      const Vector3d pinholeDirection = (shiftedPixelPoint - origin).normalized();
      const Vector3d tiltedNormal =
        focalForward * std::cos(tiltRadians) + (rightBasis ^ focalForward) * std::sin(tiltRadians);
      const double denominator = pinholeDirection * tiltedNormal;
      const Vector3d focalPoint =
        origin + pinholeDirection *
                   (descriptor.lensParameters[0] * (focalForward * tiltedNormal) / denominator);
      const double apertureRadius = Vector3d(descriptor.lensRight).length();
      const Vector3d lensRight = descriptor.motionMode == gpuPrimaryPathMotionModeLookAt
                                   ? basis.right * apertureRadius
                                   : Vector3d(descriptor.lensRight);
      const Vector3d lensUp = descriptor.motionMode == gpuPrimaryPathMotionModeLookAt
                                ? basis.down * apertureRadius
                                : Vector3d(descriptor.lensUp);
      const Vector3d lensOrigin = origin + lensRight * lensSample.x() + lensUp * lensSample.y();
      return Rayd(lensOrigin, (focalPoint - lensOrigin).normalized());
    }
  }

  TEST(GpuDiffusePathStep, RunsClosestHitAndMaterialLookupForActivePaths) {
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto matteSphere = std::make_shared<Sphere>(Vector3d(-1.0, 0.0, 0.0), 0.5);
    matteSphere->setMaterial(matte);

    auto emissive = std::make_shared<EmissiveMaterial>(Colord(0.25, 0.5, 0.75));
    auto emissiveSphere = std::make_shared<Sphere>(Vector3d(1.0, 0.0, 0.0), 0.5);
    emissiveSphere->setMaterial(emissive);

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    scene.add(matteSphere);
    scene.add(emissiveSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);

    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(-1.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 11),
      activePath(Rayd(Vector4d(1.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 12),
      activePath(Rayd(Vector4d(3.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 13),
      makeTerminatedGpuDiffusePathState(),
    };
    paths[3].ray.rayIndex = 99;

    const std::vector<GpuIntersectionHitRecord> cpuClosestHits =
      GpuIntersectionIntersector().intersectClosest(sections.geometry,
                                                    {paths[0].ray, paths[1].ray, paths[2].ray});
    const GpuDiffusePathStepResult cpuReference =
      GpuDiffusePathStepReference().step(sections, paths, cpuClosestHits);

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, paths);

    ASSERT_EQ(cpuClosestHits.size(), result.closestHitRecords.size());
    for (std::size_t index = 0; index != cpuClosestHits.size(); ++index) {
      expectHitRecordNear(result.closestHitRecords[index], cpuClosestHits[index]);
    }
    ASSERT_EQ(cpuReference.stepRecords.size(), result.stepRecords.size());
    for (std::size_t index = 0; index != cpuReference.stepRecords.size(); ++index) {
      expectStepRecordEqual(result.stepRecords[index], cpuReference.stepRecords[index]);
    }

    ASSERT_TRUE(result.closestHitRecords[0].hit);
    ASSERT_TRUE(result.closestHitRecords[1].hit);
    EXPECT_FALSE(result.closestHitRecords[2].hit);
    EXPECT_EQ(firstMaterialId(sections, GpuTracingMaterialKind::Matte),
              result.closestHitRecords[0].material);
    EXPECT_EQ(firstMaterialId(sections, GpuTracingMaterialKind::Emissive),
              result.closestHitRecords[1].material);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[1].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[2].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[3].event);
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ(3u, result.metrics.closestHitRays);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesOneActivePathPerPixelSample) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    EXPECT_EQ(0, generation.requestedRect.left());
    EXPECT_EQ(0, generation.requestedRect.top());
    EXPECT_EQ(3, generation.requestedRect.width());
    EXPECT_EQ(2, generation.requestedRect.height());
    EXPECT_EQ(0, generation.actualRect.left());
    EXPECT_EQ(0, generation.actualRect.top());
    EXPECT_EQ(3, generation.actualRect.width());
    EXPECT_EQ(2, generation.actualRect.height());
    EXPECT_EQ(24u, generation.generatedPrimarySamples);
    EXPECT_EQ(0u, generation.skippedPrimarySamples);
    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_pinhole_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    ASSERT_EQ(24u, generation.pathStates.size());

    std::size_t pathIndex = 0;
    for (std::uint32_t pixelIndex = 0; pixelIndex != 6; ++pixelIndex) {
      for (std::uint32_t sampleIndex = 0; sampleIndex != 4; ++sampleIndex) {
        const GpuDiffusePathStateRecord& path = generation.pathStates[pathIndex];
        EXPECT_TRUE(gpuDiffusePathStateIsActive(path));
        EXPECT_FALSE(gpuDiffusePathStateIsTerminated(path));
        EXPECT_EQ(static_cast<std::uint32_t>(pathIndex), path.ray.rayIndex);
        EXPECT_EQ(pixelIndex, path.pixelIndex);
        EXPECT_EQ(sampleIndex, path.primarySampleIndex);
        EXPECT_EQ(0u, path.depth);
        EXPECT_EQ(1234u, path.sampleSeed);
        EXPECT_EQ(static_cast<std::uint32_t>(SampleDimension::BSDF), path.sampleDimensionBase);
        EXPECT_EQ(static_cast<std::uint32_t>(kPathSampleDimensionStride),
                  path.sampleDimensionStride);
        EXPECT_FLOAT_EQ(1.0f, path.throughput[0]);
        EXPECT_FLOAT_EQ(1.0f, path.throughput[1]);
        EXPECT_FLOAT_EQ(1.0f, path.throughput[2]);
        EXPECT_FLOAT_EQ(0.0f, path.accumulatedRadiance[0]);
        EXPECT_FLOAT_EQ(0.0f, path.accumulatedRadiance[1]);
        EXPECT_FLOAT_EQ(0.0f, path.accumulatedRadiance[2]);
        EXPECT_NEAR(1.0, Vector3d(path.ray.direction).length(), 1e-5);
        EXPECT_GE(path.ray.timeSample, 0.0f);
        EXPECT_LT(path.ray.timeSample, 1.0f);
        ++pathIndex;
      }
    }
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeavePinholePrimaryPathsDescriptorOnly) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_pinhole_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(24u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanGeneratePinholePrimarySampleSubrange) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.sampleOffset = 2u;
    options.sampleCount = 1u;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(2u, descriptor.sampleOffset);
    EXPECT_EQ(1u, descriptor.samplesPerPixel);
    EXPECT_EQ(6u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(6u, generation.generatedPrimarySamples);
    EXPECT_EQ(0u, generation.skippedPrimarySamples);
    ASSERT_EQ(6u, generation.pathStates.size());

    for (std::uint32_t pixelIndex = 0; pixelIndex != 6u; ++pixelIndex) {
      const GpuDiffusePathStateRecord& path = generation.pathStates[pixelIndex];
      EXPECT_EQ(pixelIndex, path.pixelIndex);
      EXPECT_EQ(2u, path.primarySampleIndex);
      EXPECT_EQ(pixelIndex, path.ray.rayIndex);
    }

    const Vector3d origin(descriptor.originOrDirection);
    const Vector3d motionOriginDelta(descriptor.motionOriginDelta);
    const Vector3d topLeft(descriptor.topLeft);
    const Vector3d right(descriptor.right);
    const Vector3d down(descriptor.down);
    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/2,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/2, /*dimension=*/1,
      /*component=*/0});
    const Vector3d pixelPoint = topLeft + right * pixelSample.x() + down * pixelSample.y();
    const Vector3d rayOrigin = origin + motionOriginDelta * timeSample;
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      Rayd(rayOrigin, (pixelPoint - rayOrigin).normalized()), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeavePinholePrimarySampleSubrangeDescriptorOnly) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    options.sampleOffset = 1u;
    options.sampleCount = 2u;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(1u, descriptor.sampleOffset);
    EXPECT_EQ(2u, descriptor.samplesPerPixel);
    EXPECT_EQ(12u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(12u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuPrimaryPathDescriptor, CanRestrictActualRectInsideRequestedRect) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 6, 4));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 6, 4), 99, 1234, options);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());

    const GpuPrimaryPathDescriptor descriptor =
      generation.primaryPathDescriptor->withActualRect(Recti(2, 1, 3, 2));

    EXPECT_EQ(0, descriptor.requestedRect().left());
    EXPECT_EQ(0, descriptor.requestedRect().top());
    EXPECT_EQ(6, descriptor.requestedRect().width());
    EXPECT_EQ(4, descriptor.requestedRect().height());
    EXPECT_EQ(2, descriptor.actualRect().left());
    EXPECT_EQ(1, descriptor.actualRect().top());
    EXPECT_EQ(3, descriptor.actualRect().width());
    EXPECT_EQ(2, descriptor.actualRect().height());
    EXPECT_EQ(24u, descriptor.pathCount());
    EXPECT_THROW((void)generation.primaryPathDescriptor->withActualRect(Recti(5, 1, 2, 1)),
                 std::out_of_range);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, PinholeDescriptorUsesGpuSampleStreamForPrimaryRay) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(0, descriptor.requestedLeft);
    EXPECT_EQ(0, descriptor.requestedTop);
    EXPECT_EQ(3u, descriptor.requestedWidth);
    EXPECT_EQ(2u, descriptor.requestedHeight);
    EXPECT_EQ(0, descriptor.actualLeft);
    EXPECT_EQ(0, descriptor.actualTop);
    EXPECT_EQ(3u, descriptor.actualWidth);
    EXPECT_EQ(2u, descriptor.actualHeight);
    EXPECT_EQ(4u, descriptor.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor.sampleSeed);

    ASSERT_FALSE(generation.pathStates.empty());
    const Vector3d origin(descriptor.originOrDirection);
    const Vector3d motionOriginDelta(descriptor.motionOriginDelta);
    const Vector3d topLeft(descriptor.topLeft);
    const Vector3d right(descriptor.right);
    const Vector3d down(descriptor.down);
    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d pixelPoint = topLeft + right * pixelSample.x() + down * pixelSample.y();
    const Vector3d rayOrigin = origin + motionOriginDelta * timeSample;
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      Rayd(rayOrigin, (pixelPoint - rayOrigin).normalized()), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       PinholeDescriptorAppliesSampledShutterMotionOriginDelta) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 0.0, 2.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -10.0), Vector3d(descriptor.originOrDirection), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), Vector3d(descriptor.motionOriginDelta), 1e-6);

    ASSERT_FALSE(generation.pathStates.empty());
    const Vector3d origin(descriptor.originOrDirection);
    const Vector3d motionOriginDelta(descriptor.motionOriginDelta);
    const Vector3d topLeft(descriptor.topLeft);
    const Vector3d right(descriptor.right);
    const Vector3d down(descriptor.down);
    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d pixelPoint = topLeft + right * pixelSample.x() + down * pixelSample.y();
    const Vector3d rayOrigin = origin + motionOriginDelta * timeSample;
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      Rayd(rayOrigin, (pixelPoint - rayOrigin).normalized()), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, PinholeDescriptorAppliesSampledShutterLookAtMotion) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 2.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0), Vector3d(descriptor.originOrDirection), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), Vector3d(descriptor.motionOriginDelta), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 0.0), Vector3d(descriptor.motionTarget), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(1.0, 0.0, 2.0), Vector3d(descriptor.motionTargetDelta), 1e-6);

    ASSERT_FALSE(generation.pathStates.empty());
    const Vector3d positionAtOpen(descriptor.originOrDirection);
    const Vector3d positionDelta(descriptor.motionOriginDelta);
    const Vector3d targetAtOpen(descriptor.motionTarget);
    const Vector3d targetDelta(descriptor.motionTargetDelta);
    const Vector3d topLeft(descriptor.topLeft);
    const Vector3d right(descriptor.right);
    const Vector3d down(descriptor.down);
    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d pixelPoint = topLeft + right * pixelSample.x() + down * pixelSample.y();
    const Vector3d position = positionAtOpen + positionDelta * timeSample;
    const Vector3d target = targetAtOpen + targetDelta * timeSample;
    const Vector3d rayOrigin =
      position - (target - position).normalized() * descriptor.motionParameters[0];
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      Rayd(rayOrigin, (pixelPoint - rayOrigin).normalized()), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesOrthographicPrimaryPathDescriptor) {
    OrthographicCamera camera(Vector3d(1.0, 2.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_orthographic_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeOrthographic, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    ASSERT_EQ(24u, generation.pathStates.size());

    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(4u, descriptor.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor.sampleSeed);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Rayd cameraRay = camera.rayForPixel(pixelSample.x(), pixelSample.y());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(cameraRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       OrthographicDescriptorAppliesSampledShutterMotionOriginDelta) {
    OrthographicCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 1.0, -5.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 1.0, 0.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 1.0, 0.0), Vector3d(descriptor.motionOriginDelta), 1e-6);

    ASSERT_FALSE(generation.pathStates.empty());
    const Vector3d direction(descriptor.originOrDirection);
    const Vector3d motionOriginDelta(descriptor.motionOriginDelta);
    const Vector3d topLeft(descriptor.topLeft);
    const Vector3d right(descriptor.right);
    const Vector3d down(descriptor.down);
    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d pixelPoint = topLeft + right * pixelSample.x() + down * pixelSample.y();
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      Rayd(pixelPoint + motionOriginDelta * timeSample, direction.normalized()), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       OrthographicDescriptorAppliesSampledShutterLookAtMotion) {
    OrthographicCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 0.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0), Vector3d(descriptor.originOrDirection), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(1.0, 0.0, 0.0), Vector3d(descriptor.motionTargetDelta), 1e-6);

    ASSERT_FALSE(generation.pathStates.empty());
    const Vector3d positionAtOpen(descriptor.originOrDirection);
    const Vector3d positionDelta(descriptor.motionOriginDelta);
    const Vector3d targetAtOpen(descriptor.motionTarget);
    const Vector3d targetDelta(descriptor.motionTargetDelta);
    const Vector3d topLeft(descriptor.topLeft);
    const Vector3d right(descriptor.right);
    const Vector3d down(descriptor.down);
    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d localPixelPoint = topLeft + right * pixelSample.x() + down * pixelSample.y();
    const Vector3d position = positionAtOpen + positionDelta * timeSample;
    const Vector3d target = targetAtOpen + targetDelta * timeSample;
    const Matrix4d matrix = Matrix4d::lookAt(position, target, Vector3d::up());
    const Rayd expectedRay(matrix.transformPoint(localPixelPoint),
                           matrix.transformDirection(Vector3d::forward()).normalized());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(expectedRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeaveOrthographicPrimaryPathsDescriptorOnly) {
    OrthographicCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_orthographic_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeOrthographic, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(24u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesThinLensPrimaryPathDescriptor) {
    ThinLensCamera camera(Vector3d(0.25, 0.5, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.35);
    camera.setFocalDistance(7.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_thin_lens_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeThinLens, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    ASSERT_EQ(24u, generation.pathStates.size());

    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(4u, descriptor.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor.sampleSeed);
    EXPECT_NEAR(12.0, descriptor.lensParameters[0], 1e-5);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const Vector2d lensSample = expectedConcentricDiscSample(
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/2));
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Rayd cameraRay =
      camera.rayForPixelWithLens(pixelSample.x(), pixelSample.y(), lensSample.x(), lensSample.y());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(cameraRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       ThinLensDescriptorAppliesSampledShutterMotionOriginDelta) {
    ThinLensCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.35);
    camera.setFocalDistance(7.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 0.0, 2.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), Vector3d(descriptor.motionOriginDelta), 1e-6);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const Vector2d lensSample = expectedConcentricDiscSample(
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/2));
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      thinLensDescriptorRay(descriptor, pixelSample, lensSample, timeSample), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, ThinLensDescriptorAppliesSampledShutterLookAtMotion) {
    ThinLensCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.35);
    camera.setFocalDistance(7.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 2.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0), Vector3d(descriptor.originOrDirection), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), Vector3d(descriptor.motionOriginDelta), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d::null, Vector3d(descriptor.motionTarget), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(1.0, 0.0, 2.0), Vector3d(descriptor.motionTargetDelta), 1e-6);
    EXPECT_FLOAT_EQ(5.0f, descriptor.motionParameters[0]);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const Vector2d lensSample = expectedConcentricDiscSample(
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/2));
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      thinLensDescriptorRay(descriptor, pixelSample, lensSample, timeSample), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeaveThinLensPrimaryPathsDescriptorOnly) {
    ThinLensCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.25);
    camera.setFocalDistance(6.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_thin_lens_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeThinLens, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(24u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesTiltShiftPrimaryPathDescriptor) {
    TiltShiftCamera camera(Vector3d(0.25, 0.5, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.35);
    camera.setFocalDistance(7.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_tilt_shift_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    ASSERT_EQ(24u, generation.pathStates.size());

    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(4u, descriptor.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor.sampleSeed);
    EXPECT_NEAR(12.0, descriptor.lensParameters[0], 1e-5);
    EXPECT_FLOAT_EQ(0.2f, descriptor.lensParameters[1]);
    EXPECT_FLOAT_EQ(-0.1f, descriptor.lensParameters[2]);
    EXPECT_FLOAT_EQ(static_cast<float>((20_degrees).radians()), descriptor.lensParameters[3]);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const Vector2d lensSample = expectedConcentricDiscSample(
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/2));
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Rayd cameraRay =
      camera.rayForPixelWithLens(pixelSample.x(), pixelSample.y(), lensSample.x(), lensSample.y());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(cameraRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       TiltShiftDescriptorAppliesSampledShutterMotionOriginDelta) {
    TiltShiftCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.35);
    camera.setFocalDistance(7.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 0.0, 2.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), Vector3d(descriptor.motionOriginDelta), 1e-6);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const Vector2d lensSample = expectedConcentricDiscSample(
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/2));
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      tiltShiftDescriptorRay(descriptor, pixelSample, lensSample, timeSample), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, TiltShiftDescriptorAppliesSampledShutterLookAtMotion) {
    TiltShiftCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.35);
    camera.setFocalDistance(7.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 2.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor.motionMode);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -5.0), Vector3d(descriptor.originOrDirection), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 2.0), Vector3d(descriptor.motionOriginDelta), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d::null, Vector3d(descriptor.motionTarget), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(1.0, 0.0, 2.0), Vector3d(descriptor.motionTargetDelta), 1e-6);
    EXPECT_FLOAT_EQ(5.0f, descriptor.motionParameters[0]);
    EXPECT_FLOAT_EQ(0.35f, descriptor.motionParameters[1]);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const Vector2d lensSample = expectedConcentricDiscSample(
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/2));
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const GpuIntersectionRay expected = GpuIntersectionScenePacker().packRay(
      tiltShiftDescriptorRay(descriptor, pixelSample, lensSample, timeSample), /*rayIndex=*/0,
      /*minDistance=*/0.0, std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeaveTiltShiftPrimaryPathsDescriptorOnly) {
    TiltShiftCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.25);
    camera.setFocalDistance(6.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_tilt_shift_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(24u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(24u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesEquirectangularPrimaryPathDescriptor) {
    EquirectangularCamera camera(Vector3d(1.0, 2.0, -5.0), Vector3d(1.0, 2.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_equirectangular_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeEquirectangular, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(32u, generation.primaryPathDescriptor->pathCount());
    ASSERT_EQ(32u, generation.pathStates.size());

    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(4u, descriptor.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor.sampleSeed);
    EXPECT_FLOAT_EQ(4.0f, descriptor.lensParameters[0]);
    EXPECT_FLOAT_EQ(2.0f, descriptor.lensParameters[1]);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Rayd cameraRay = camera.rayForPixel(pixelSample.x(), pixelSample.y());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(cameraRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       EquirectangularDescriptorAppliesSampledShutterMotionOriginDelta) {
    EquirectangularCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 1.0, -5.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -4.0)}, {1.0, Vector3d(0.0, 1.0, -4.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 1.0, 0.0), Vector3d(descriptor.motionOriginDelta), 1e-6);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d direction =
      equirectangularDescriptorDirection(descriptor, pixelSample.x(), pixelSample.y());
    const Rayd expectedRay(Vector4d(Vector3d(descriptor.originOrDirection) +
                                    Vector3d(descriptor.motionOriginDelta) * timeSample),
                           direction);
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(expectedRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       EquirectangularDescriptorAppliesSampledShutterLookAtMotion) {
    EquirectangularCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -4.0)}, {1.0, Vector3d(1.0, 0.0, -4.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor.motionMode);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d position =
      Vector3d(descriptor.originOrDirection) + Vector3d(descriptor.motionOriginDelta) * timeSample;
    const Vector3d target =
      Vector3d(descriptor.motionTarget) + Vector3d(descriptor.motionTargetDelta) * timeSample;
    const Matrix4d matrix = Matrix4d::lookAt(position, target, Vector3d::up());
    const Vector3d local =
      equirectangularLocalDirection(descriptor, pixelSample.x(), pixelSample.y());
    const Rayd expectedRay(matrix.translationVector(),
                           matrix.transformDirection(local).normalized());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(expectedRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeaveEquirectangularPrimaryPathsDescriptorOnly) {
    EquirectangularCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_equirectangular_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeEquirectangular, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(32u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(32u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesSphericalPrimaryPathDescriptor) {
    SphericalCamera camera(Vector3d(1.0, 2.0, -5.0), Vector3d(1.0, 2.0, -4.0));
    camera.setFieldOfView(200_degrees, 90_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_spherical_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeSpherical, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(32u, generation.primaryPathDescriptor->pathCount());
    ASSERT_EQ(32u, generation.pathStates.size());

    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(4u, descriptor.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor.sampleSeed);
    EXPECT_FLOAT_EQ(4.0f, descriptor.lensParameters[0]);
    EXPECT_FLOAT_EQ(2.0f, descriptor.lensParameters[1]);
    EXPECT_FLOAT_EQ(static_cast<float>((200_degrees).radians()), descriptor.lensParameters[2]);
    EXPECT_FLOAT_EQ(static_cast<float>((90_degrees).radians()), descriptor.lensParameters[3]);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Rayd cameraRay = camera.rayForPixel(pixelSample.x(), pixelSample.y());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(cameraRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator,
       SphericalDescriptorAppliesSampledShutterMotionOriginDelta) {
    SphericalCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(200_degrees, 90_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 1.0, -5.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -4.0)}, {1.0, Vector3d(0.0, 1.0, -4.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 1.0, 0.0), Vector3d(descriptor.motionOriginDelta), 1e-6);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d direction =
      sphericalDescriptorDirection(descriptor, pixelSample.x(), pixelSample.y());
    const Rayd expectedRay(Vector4d(Vector3d(descriptor.originOrDirection) +
                                    Vector3d(descriptor.motionOriginDelta) * timeSample),
                           direction);
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(expectedRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, SphericalDescriptorAppliesSampledShutterLookAtMotion) {
    SphericalCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(200_degrees, 90_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -4.0)}, {1.0, Vector3d(1.0, 0.0, -4.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor.motionMode);
    EXPECT_FLOAT_EQ(5.0f, descriptor.motionParameters[0]);

    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0,
                                /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, /*pixelIndex=*/0, /*primarySampleIndex=*/0, /*dimension=*/1,
      /*component=*/0});
    const Vector3d position =
      Vector3d(descriptor.originOrDirection) + Vector3d(descriptor.motionOriginDelta) * timeSample;
    const Vector3d target =
      Vector3d(descriptor.motionTarget) + Vector3d(descriptor.motionTargetDelta) * timeSample;
    const Matrix4d matrix = Matrix4d::lookAt(position, target, Vector3d::up());
    const Vector3d local = sphericalLocalDirection(descriptor, pixelSample.x(), pixelSample.y());
    const Rayd expectedRay(matrix.transformPoint(Vector3d(0.0, 0.0, -5.0)),
                           matrix.transformDirection(local).normalized());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(expectedRay, /*rayIndex=*/0, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(generation.pathStates.front().ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeaveSphericalPrimaryPathsDescriptorOnly) {
    SphericalCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_spherical_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeSpherical, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(32u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(32u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, GeneratesFishEyePrimaryPathDescriptor) {
    FishEyeCamera camera(Vector3d(1.0, 2.0, -5.0), Vector3d(1.0, 2.0, -4.0));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 4), 99, 1234);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_fisheye_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeFishEye, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(64u, generation.primaryPathDescriptor->pathCount());
    EXPECT_GT(generation.generatedPrimarySamples, 0u);
    EXPECT_GT(generation.skippedPrimarySamples, 0u);
    EXPECT_EQ(64u, generation.generatedPrimarySamples + generation.skippedPrimarySamples);
    ASSERT_EQ(generation.generatedPrimarySamples, generation.pathStates.size());

    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(4u, descriptor.samplesPerPixel);
    EXPECT_EQ(1234u, descriptor.sampleSeed);
    EXPECT_FLOAT_EQ(4.0f, descriptor.lensParameters[0]);
    EXPECT_FLOAT_EQ(4.0f, descriptor.lensParameters[1]);
    EXPECT_FLOAT_EQ(static_cast<float>((180_degrees).radians()), descriptor.lensParameters[2]);
    EXPECT_FLOAT_EQ(0.0f, descriptor.lensParameters[3]);

    for (const GpuDiffusePathStateRecord& path : generation.pathStates) {
      EXPECT_TRUE(gpuDiffusePathStateIsActive(path));
      EXPECT_FALSE(gpuDiffusePathStateIsTerminated(path));
      const float directionLengthSquared = path.ray.direction[0] * path.ray.direction[0] +
                                           path.ray.direction[1] * path.ray.direction[1] +
                                           path.ray.direction[2] * path.ray.direction[2];
      EXPECT_NEAR(1.0f, directionLengthSquared, 1e-5f);
    }
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, FishEyeDescriptorAppliesSampledShutterLookAtMotion) {
    FishEyeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -4.0)}, {1.0, Vector3d(1.0, 0.0, -4.0)}}));

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 4), 99, 1234);

    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    ASSERT_FALSE(generation.pathStates.empty());
    const GpuRectilinearPrimaryPathDescriptor& descriptor =
      generation.primaryPathDescriptor->rectilinear;
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, descriptor.motionMode);

    const GpuDiffusePathStateRecord& path = generation.pathStates.front();
    const std::uint32_t pixelIndex = path.pixelIndex;
    const std::uint32_t sampleIndex = path.primarySampleIndex;
    const std::uint32_t column = pixelIndex % descriptor.requestedWidth;
    const std::uint32_t row = pixelIndex / descriptor.requestedWidth;
    const Vector2d pixelSample =
      GpuSampleStream::sample2D(/*seed=*/1234, pixelIndex, sampleIndex, /*dimension=*/0);
    const double timeSample = GpuSampleStream::sample1D(GpuSampleCoordinate{
      /*seed=*/1234, pixelIndex, sampleIndex, /*dimension=*/1, /*component=*/0});
    const std::optional<Vector3d> local =
      fishEyeLocalDirection(descriptor, static_cast<double>(column) + pixelSample.x(),
                            static_cast<double>(row) + pixelSample.y());
    ASSERT_TRUE(local.has_value());

    const Vector3d position =
      Vector3d(descriptor.originOrDirection) + Vector3d(descriptor.motionOriginDelta) * timeSample;
    const Vector3d target =
      Vector3d(descriptor.motionTarget) + Vector3d(descriptor.motionTargetDelta) * timeSample;
    const Matrix4d matrix = Matrix4d::lookAt(position, target, Vector3d::up());
    const Rayd expectedRay(matrix.translationVector(),
                           matrix.transformDirection(*local).normalized());
    const GpuIntersectionRay expected =
      GpuIntersectionScenePacker().packRay(expectedRay, path.ray.rayIndex, /*minDistance=*/0.0,
                                           std::numeric_limits<double>::infinity(), timeSample);
    expectGpuRayNear(path.ray, expected, 2e-5);
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, CanLeaveFishEyePrimaryPathsDescriptorOnly) {
    FishEyeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
    camera.viewPlane()->sampler()->setup(4, 8, 42);

    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 4), 99, 1234, options);

    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    EXPECT_EQ("gpu_fisheye_primary_descriptor", generation.primaryPathExecutionPath);
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeFishEye, generation.primaryPathDescriptor->mode);
    EXPECT_EQ(64u, generation.primaryPathDescriptor->pathCount());
    EXPECT_EQ(64u, generation.generatedPrimarySamples);
    EXPECT_TRUE(generation.pathStates.empty());
  }

  TEST(GpuDiffusePrimaryPathStateGenerator, UsesActualRenderableRectForFitExactCameras) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setAspectMode(AspectMode::FitExact);
    camera.setAspectRatio(1.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 7);

    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2));

    EXPECT_EQ(0, generation.requestedRect.left());
    EXPECT_EQ(0, generation.requestedRect.top());
    EXPECT_EQ(4, generation.requestedRect.width());
    EXPECT_EQ(2, generation.requestedRect.height());
    EXPECT_EQ(1, generation.actualRect.left());
    EXPECT_EQ(0, generation.actualRect.top());
    EXPECT_EQ(2, generation.actualRect.width());
    EXPECT_EQ(2, generation.actualRect.height());
    EXPECT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.primaryPathDescriptor.has_value());
    EXPECT_EQ(1, generation.primaryPathDescriptor->actualRect().left());
    EXPECT_EQ(2, generation.primaryPathDescriptor->actualRect().width());
    ASSERT_EQ(4u, generation.pathStates.size());
    EXPECT_EQ(1u, generation.pathStates[0].pixelIndex);
    EXPECT_EQ(2u, generation.pathStates[1].pixelIndex);
    EXPECT_EQ(5u, generation.pathStates[2].pixelIndex);
    EXPECT_EQ(6u, generation.pathStates[3].pixelIndex);
  }

  TEST(GpuDiffusePathStep, OneBounceMissMatchesReferenceRecordsAndContribution) {
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.25, 0.5, 0.75));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              actual.stepRecords[0].event);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), Colord(actual.stepRecords[0].missRadiance),
                      1e-6);
    EXPECT_TRUE(actual.pathStates.empty());
    ASSERT_EQ(1u, actual.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(actual.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(actual.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ("packed_cpu", actual.metrics.closestHitExecutionPath);
    EXPECT_EQ(1u, actual.metrics.closestHitRays);
    EXPECT_EQ(1u, actual.metrics.misses);
    EXPECT_EQ(1u, actual.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, OneBounceDiffuseContinuationMatchesReferenceRecords) {
    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.sampleSeed = 12347;
    path.throughput = {0.2f, 0.4f, 0.6f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              actual.stepRecords[0].event);
    ASSERT_EQ(1u, actual.pathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(actual.pathStates[0]));
    EXPECT_EQ(1u, actual.pathStates[0].depth);
    ASSERT_COLOR_NEAR(Colord(expected.pathStates[0].throughput),
                      Colord(actual.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(actual.pathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(1u, actual.metrics.spawnedContinuations);
  }

  TEST(GpuDiffusePathStep, OneBounceDirectLightMatchesReferenceRecordsAndContribution) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.pathStates.size());
    ASSERT_EQ(1u, actual.directLightShadowRays.size());
    ASSERT_EQ(1u, actual.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, actual.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(actual.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(actual.stepRecords[0].directLightRadiance),
                      Colord(actual.pathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ("packed_cpu", actual.metrics.directLightVisibilityExecutionPath);
    EXPECT_EQ("cpu_record", actual.metrics.directLightContributionExecutionPath);
    EXPECT_EQ(1u, actual.metrics.directLightSamples);
    EXPECT_EQ(1u, actual.metrics.directLightVisibilityRays);
    EXPECT_EQ(1u, actual.metrics.directLightContributionEvaluations);
    EXPECT_EQ(1u, actual.metrics.directLightContributingSamples);
    EXPECT_EQ(1u, actual.stepRecords[0].directLightSampleCount);
    EXPECT_EQ(1u, actual.stepRecords[0].directLightVisibilityRayCount);
    EXPECT_EQ(1u, actual.stepRecords[0].directLightContributingSampleCount);
    EXPECT_EQ(0u, actual.stepRecords[0].directLightOccludedSampleCount);
  }

  TEST(GpuDiffusePathStep, DirectLightSamplesAreAveragedAcrossConfiguredSamples) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopSettings settings;
    settings.directLightSamples = 2;

    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths, settings);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.pathStates.size());
    ASSERT_EQ(2u, actual.directLightShadowRays.size());
    ASSERT_EQ(2u, actual.directLightOcclusionRecords.size());
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(actual.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(actual.stepRecords[0].directLightRadiance),
                      Colord(actual.pathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(2u, actual.metrics.directLightSamples);
    EXPECT_EQ(2u, actual.metrics.directLightVisibilityRays);
    EXPECT_EQ(2u, actual.metrics.directLightContributionEvaluations);
    EXPECT_EQ(2u, actual.metrics.directLightContributingSamples);
    EXPECT_EQ(2u, actual.stepRecords[0].directLightSampleCount);
    EXPECT_EQ(2u, actual.stepRecords[0].directLightVisibilityRayCount);
    EXPECT_EQ(2u, actual.stepRecords[0].directLightContributingSampleCount);
    EXPECT_EQ(0u, actual.stepRecords[0].directLightOccludedSampleCount);
  }

  TEST(GpuDiffusePathStep, UnsupportedMaterialLookupTerminatesExplicitly) {
    auto unsupportedSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedSphere->setMaterial(std::make_shared<UnsupportedGpuTracingMaterial>());

    Scene scene;
    scene.add(unsupportedSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {activePath()});

    ASSERT_EQ(1u, result.closestHitRecords.size());
    EXPECT_TRUE(result.closestHitRecords[0].hit);
    ASSERT_LT(result.closestHitRecords[0].material, sections.materials.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuTracingMaterialKind::Unsupported),
              sections.materials[result.closestHitRecords[0].material].kind);

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    EXPECT_NE(0u, result.terminatedPathStates[0].flags & gpuDiffusePathStateUnsupportedFlag);
    EXPECT_NE(0u, result.stepRecords[0].flags & gpuDiffusePathStateUnsupportedFlag);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Unsupported),
              result.stepRecords[0].event);
    EXPECT_EQ(result.closestHitRecords[0].material, result.stepRecords[0].material);
    EXPECT_EQ(1u, result.metrics.unsupportedHits);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, ReflectiveMaterialSpawnsExactMirrorContinuation) {
    auto reflective =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    reflective->setDiffuseCoefficient(0.0);
    reflective->setReflectionColor(Colord(0.75, 0.5, 0.25));
    reflective->setReflectionCoefficient(0.5);
    auto mirrorSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    mirrorSphere->setMaterial(reflective);

    Scene scene;
    scene.add(mirrorSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 1.0f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              actual.stepRecords[0].event);
    ASSERT_EQ(1u, actual.pathStates.size());
    EXPECT_TRUE(actual.terminatedPathStates.empty());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(actual.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.25 * 0.75 * 0.5, 0.5 * 0.5 * 0.5, 1.0 * 0.25 * 0.5),
                      Colord(actual.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord(actual.pathStates[0].throughput),
                      Colord(actual.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -1.0), Vector3d(actual.pathStates[0].ray.direction),
                       1e-6);
    EXPECT_EQ(1u, actual.pathStates[0].depth);
    EXPECT_FLOAT_EQ(1.0f, actual.pathStates[0].previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.0f, actual.pathStates[0].previousLightPdf);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              actual.pathStates[0].previousEventFlags);
    EXPECT_EQ(1u, actual.metrics.spawnedContinuations);
    EXPECT_EQ(0u, actual.metrics.unsupportedHits);
    EXPECT_EQ(0u, actual.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, TransparentMaterialSpawnsRefractedDeltaContinuation) {
    auto transparent = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::white()));
    transparent->setDiffuseCoefficient(0.0);
    transparent->setSpecularCoefficient(0.0);
    transparent->setReflectionCoefficient(0.0);
    transparent->setTransmissionCoefficient(1.0);
    transparent->setRefractionIndex(1.5);
    auto glassSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    glassSphere->setMaterial(transparent);

    Scene scene;
    scene.add(glassSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 1.0f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              actual.stepRecords[0].event);
    ASSERT_EQ(1u, actual.pathStates.size());
    EXPECT_TRUE(actual.terminatedPathStates.empty());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(actual.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 1.0) * (1.0 / (1.5 * 1.5)),
                      Colord(actual.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord(actual.pathStates[0].throughput),
                      Colord(actual.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 1.0), Vector3d(actual.pathStates[0].ray.direction), 1e-6);
    EXPECT_EQ(1u, actual.pathStates[0].depth);
    EXPECT_FLOAT_EQ(1.0f, actual.pathStates[0].previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.0f, actual.pathStates[0].previousLightPdf);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              actual.pathStates[0].previousEventFlags);
    EXPECT_EQ(1u, actual.metrics.spawnedContinuations);
    EXPECT_EQ(0u, actual.metrics.unsupportedHits);
    EXPECT_EQ(0u, actual.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, PortalMaterialSpawnsTransformedDeltaContinuation) {
    auto portal =
      std::make_shared<PortalMaterial>(Matrix4d::translate(0.0, 0.0, 2.0), Colord(0.75, 0.5, 0.25));
    auto portalSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    portalSphere->setMaterial(portal);

    Scene scene;
    scene.add(portalSphere);
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 1.0f, 0.0f};
    const std::vector<GpuDiffusePathStateRecord> paths{path};
    const GpuDiffusePathStepResult expected =
      GpuDiffusePathStepReference().step(sections, paths, closestHitsFor(sections, paths));

    const GpuDiffusePathStepResult actual = GpuDiffusePathStep().step(sections, paths);

    expectStepResultParity(actual, expected);
    ASSERT_EQ(1u, actual.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              actual.stepRecords[0].event);
    ASSERT_EQ(1u, actual.pathStates.size());
    EXPECT_TRUE(actual.terminatedPathStates.empty());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(actual.pathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.25 * 0.75, 0.5 * 0.5, 1.0 * 0.25),
                      Colord(actual.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord(actual.pathStates[0].throughput),
                      Colord(actual.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, 1.0), Vector3d(actual.pathStates[0].ray.direction), 1e-6);
    ASSERT_VECTOR_NEAR(Vector3d(0.0, 0.0, -3.0 + Ray<double>::epsilon),
                       Vector3d(actual.pathStates[0].ray.origin), 1e-6);
    EXPECT_EQ(1u, actual.pathStates[0].depth);
    EXPECT_FLOAT_EQ(1.0f, actual.pathStates[0].previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.0f, actual.pathStates[0].previousLightPdf);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              actual.pathStates[0].previousEventFlags);
    EXPECT_EQ(1u, actual.metrics.spawnedContinuations);
    EXPECT_EQ(0u, actual.metrics.unsupportedHits);
    EXPECT_EQ(0u, actual.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, PhongHitUsesDiffuseAndGlossyLobesForCompiledPathLoop) {
    Scene scene;
    auto phong = std::make_shared<PhongMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord::white(), 16.0);
    phong->setDiffuseCoefficient(0.8);
    phong->setSpecularCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(phong);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {path});

    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_EQ(1u, result.directLightShadowRays.size());
    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, result.directLightOcclusionRecords[0].occluded);
    const Colord finiteBsdf = Colord(0.25, 0.5, 0.75) * 0.8 * invPI + Colord::white();
    ASSERT_COLOR_NEAR(Colord(0.8, 0.6, 0.4) * finiteBsdf * Colord(0.5, 0.25, 0.125),
                      Colord(result.stepRecords[0].directLightRadiance), 1e-5);
    EXPECT_NE(Colord::black(), Colord(result.stepRecords[0].continuationThroughput));
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
    EXPECT_EQ(0u, result.metrics.unsupportedHits);
  }

  TEST(GpuDiffusePathStep, EmissiveHitFeedsContributionIntoStepRecord) {
    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {path});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0),
                      Colord(result.terminatedPathStates[0].accumulatedRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), Colord(result.stepRecords[0].emittedRadiance), 1e-5);
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ("cpu_record", result.metrics.emissionExecutionPath);
    EXPECT_EQ(1u, result.metrics.emissiveHits);
    EXPECT_EQ(1u, result.metrics.emissionContributionEvaluations);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStep, MatteHitFeedsDirectLightContributionIntoNextPathState) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);

    const GpuDiffusePathStepResult result = GpuDiffusePathStep().step(sections, {activePath()});

    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_EQ(1u, result.directLightShadowRays.size());
    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, result.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.pathStates[0].accumulatedRadiance), 1e-5);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);
    EXPECT_EQ("packed_cpu", result.metrics.directLightVisibilityExecutionPath);
    EXPECT_EQ("cpu_record", result.metrics.directLightContributionExecutionPath);
    EXPECT_EQ(1u, result.metrics.directLightSamples);
    EXPECT_EQ(1u, result.metrics.directLightVisibilityRays);
    EXPECT_EQ(1u, result.metrics.directLightContributionEvaluations);
    EXPECT_EQ(1u, result.metrics.directLightContributingSamples);
  }

  TEST(GpuDiffusePathStepReference, PrimaryMissAddsVisibleBackgroundAndTerminatesPath) {
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.75, 0.5, 0.25));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {path}, {GpuIntersectionScenePacker().packMiss(7)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(result.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), Colord(result.stepRecords[0].missRadiance),
                      1e-6);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    EXPECT_EQ(1u, result.metrics.misses);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStepReference, BouncedMissAddsEnvironmentRadianceAndTerminatesPath) {
    Scene scene;
    scene.setBackground(Colord(0.75, 0.5, 0.25));
    scene.setEnvironmentRadiance(Colord(0.25, 0.5, 0.75));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.depth = 1;
    path.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {path}, {GpuIntersectionScenePacker().packMiss(7)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(result.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), Colord(result.stepRecords[0].missRadiance),
                      1e-6);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    EXPECT_EQ(1u, result.metrics.misses);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
  }

  TEST(GpuDiffusePathStepReference, RejectsClosestHitRecordCountMismatch) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7), activePath(8)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(sections, paths,
                                                {GpuIntersectionScenePacker().packMiss(7)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, RejectsDuplicateClosestHitRayIndices) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7), activePath(8)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(
        sections, paths,
        {GpuIntersectionScenePacker().packMiss(7), GpuIntersectionScenePacker().packMiss(7)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, RejectsUnexpectedClosestHitRayIndex) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(sections, paths,
                                                {GpuIntersectionScenePacker().packMiss(99)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, RejectsDuplicateActivePathRayIndices) {
    GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(7), activePath(7)};
    const auto step = [&] {
      return GpuDiffusePathStepReference().step(
        sections, paths,
        {GpuIntersectionScenePacker().packMiss(7), GpuIntersectionScenePacker().packMiss(7)});
    };

    EXPECT_THROW(
      {
        const GpuDiffusePathStepResult result = step();
        (void)result;
      },
      std::logic_error);
  }

  TEST(GpuDiffusePathStepReference, EmissiveHitAddsEmissionAndTerminatesPath) {
    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Emissive);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0), Colord(result.stepRecords[0].emittedRadiance), 1e-6);
    EXPECT_EQ(1u, result.metrics.emissiveHits);
  }

  TEST(GpuDiffusePathStepReference, BsdfSampledEmissiveHitAppliesMisWeight) {
    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Emissive);
    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};
    path.previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;
    path.previousBsdfPdf = 0.25f;
    path.previousLightPdf = 0.75f;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    EXPECT_TRUE(result.pathStates.empty());
    const double misWeight = mis::powerHeuristic(0.25, 0.75);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0) * misWeight,
                      Colord(result.stepRecords[0].emittedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.5, 3.0) * misWeight,
                      Colord(result.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(1u, result.metrics.emissiveHits);
  }

  TEST(GpuDiffusePathStepReference, MatteHitAddsVisibleDirectLightAndContinuation) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -2.0), Colord(0.8, 0.6, 0.4)));
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {activePath()}, {hitRecord(7, material)});

    ASSERT_EQ(1u, result.directLightShadowRays.size());
    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(0u, result.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.stepRecords[0].directLightRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(0.8 * invPI, 0.6 * invPI, 0.4 * invPI),
                      Colord(result.pathStates[0].accumulatedRadiance), 1e-5);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ(1u, result.pathStates[0].depth);
    EXPECT_EQ(1u, result.metrics.directLightSamples);
    EXPECT_EQ(1u, result.metrics.directLightContributingSamples);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
  }

  TEST(GpuDiffusePathStepReference, MatteHitAddsCompiledSceneAmbientRadiance) {
    Scene scene;
    scene.setAmbient(Colord(0.2, 0.3, 0.4));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.5, 0.25, 1.0)));
    matte->setAmbientCoefficient(0.5);
    matte->setDiffuseCoefficient(0.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = compileGpuTracingScene(scene).sections;
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.25f, 0.5f, 0.75f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    ASSERT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    ASSERT_COLOR_NEAR(Colord(0.0125, 0.01875, 0.15),
                      Colord(result.terminatedPathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(0u, result.metrics.directLightSamples);
  }

  TEST(GpuDiffusePathStepReference, FixedGpuSamplesEmitExpectedDiffuseContinuationRecord) {
    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.sampleSeed = 12347;
    path.throughput = {0.2f, 0.4f, 0.6f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 0;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)}, settings);

    ASSERT_EQ(1u, result.pathStates.size());
    const GpuDiffusePathStateRecord& next = result.pathStates[0];
    const Vector3d normal(0.0, 0.0, -1.0);
    const Vector2d bsdfSample =
      GpuSampleStream::sample2D(path.sampleSeed, path.pixelIndex, path.primarySampleIndex,
                                path.sampleDimensionBase + path.depth * path.sampleDimensionStride);
    const Vector3d expectedDirection = expectedCosineHemisphereDirection(normal, bsdfSample);
    const double expectedPdf = (normal * expectedDirection) * invPI;
    const Colord expectedPreRouletteThroughput =
      Colord(0.2, 0.4, 0.6) * Colord(0.25, 0.5, 0.75) * 0.8;
    const double roulette = GpuSampleStream::sample1D(
      GpuSampleCoordinate{path.sampleSeed, path.pixelIndex, path.primarySampleIndex,
                          path.sampleDimensionBase + 3u, /*component=*/0});
    const double expectedContinuationProbability = expectedPreRouletteThroughput.max();
    ASSERT_LT(roulette, expectedContinuationProbability);
    const Colord expectedThroughput =
      expectedPreRouletteThroughput * (1.0 / expectedContinuationProbability);

    expectFloat4Near(next.ray.direction, {static_cast<float>(expectedDirection.x()),
                                          static_cast<float>(expectedDirection.y()),
                                          static_cast<float>(expectedDirection.z()), 0.0f});
    EXPECT_EQ(1u, next.depth);
    EXPECT_FLOAT_EQ(static_cast<float>(expectedPdf), next.previousBsdfPdf);
    EXPECT_FLOAT_EQ(0.0f, next.previousLightPdf);
    EXPECT_EQ(material, next.previousMaterial);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag, next.previousEventFlags);
    EXPECT_TRUE(gpuDiffusePathStateIsActive(next));
    EXPECT_EQ(0u, next.flags & gpuDiffusePathStateTerminatedFlag);
    ASSERT_COLOR_NEAR(expectedThroughput, Colord(next.throughput), 1e-6);
    ASSERT_COLOR_NEAR(expectedThroughput, Colord(result.stepRecords[0].continuationThroughput),
                      1e-6);
  }

  TEST(GpuDiffusePathStepReference, DiffuseContinuationRecordsAreaLightPdf) {
    GpuDiffusePathStateRecord path = activePath();
    path.sampleSeed = 12347;
    path.throughput = {0.2f, 0.4f, 0.6f, 0.0f};

    const Vector3d surfaceNormal(0.0, 0.0, -1.0);
    const Vector2d bsdfSample =
      GpuSampleStream::sample2D(path.sampleSeed, path.pixelIndex, path.primarySampleIndex,
                                path.sampleDimensionBase + path.depth * path.sampleDimensionStride);
    const Vector3d expectedDirection = expectedCosineHemisphereDirection(surfaceNormal, bsdfSample);
    const Vector3d lightNormal = -expectedDirection;
    const Vector3d edgeU =
      ((std::abs(lightNormal.y()) < 0.999 ? Vector3d::up() : Vector3d::right()) ^ lightNormal)
        .normalized();
    const Vector3d edgeV = (lightNormal ^ edgeU).normalized();

    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<RectangularAreaLight>(expectedDirection * 3.0, edgeU * 2.0,
                                                          edgeV * 2.0, Colord::white()));

    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)}, settings);

    ASSERT_EQ(1u, result.pathStates.size());
    const GpuDiffusePathStateRecord& next = result.pathStates[0];
    const double expectedBsdfPdf = (surfaceNormal * expectedDirection) * invPI;
    EXPECT_FLOAT_EQ(static_cast<float>(expectedBsdfPdf), next.previousBsdfPdf);
    EXPECT_NEAR(2.25f, next.previousLightPdf, 1e-5f);
  }

  TEST(GpuDiffusePathStepReference,
       PreservesPathTimeSampleForDiffuseContinuationAndDirectLightVisibility) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord::white()));
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.ray.timeSample = 0.625f;
    path.sampleSeed = 12347;
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)}, settings);

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_FLOAT_EQ(path.ray.timeSample, result.pathStates[0].ray.timeSample);
    ASSERT_EQ(1u, result.directLightShadowRays.size());
    EXPECT_FLOAT_EQ(path.ray.timeSample, result.directLightShadowRays[0].timeSample);
  }

  TEST(GpuDiffusePathStepReference, PreservesPathTimeSampleForDeltaContinuations) {
    Scene scene;
    auto reflective =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    reflective->setDiffuseCoefficient(0.0);
    reflective->setSpecularCoefficient(0.0);
    reflective->setReflectionCoefficient(1.0);
    auto mirror = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    mirror->setMaterial(reflective);
    scene.add(mirror);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Reflective);

    GpuDiffusePathStateRecord path = activePath();
    path.ray.timeSample = 0.375f;
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)}, settings);

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_FLOAT_EQ(path.ray.timeSample, result.pathStates[0].ray.timeSample);
  }

  TEST(GpuDiffusePathStepReference, PreservesPathTimeSampleForPortalContinuation) {
    Scene scene;
    auto portal =
      std::make_shared<PortalMaterial>(Matrix4d::translate(1.0, 0.0, 0.0), Colord::white());
    auto surface = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    surface->setMaterial(portal);
    scene.add(surface);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Portal);

    GpuDiffusePathStateRecord path = activePath();
    path.ray.timeSample = 0.875f;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_FLOAT_EQ(path.ray.timeSample, result.pathStates[0].ray.timeSample);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesUvCheckerTexture) {
    auto checker = std::make_shared<CheckerBoardTexture>(
      new UVMapping2D(2.0, 2.0), std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto matte = std::make_shared<MatteMaterial>(checker);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord brightPath = activePath(17);
    GpuDiffusePathStateRecord darkPath = activePath(18);
    GpuIntersectionHitRecord brightHit = hitRecord(17, material);
    brightHit.uv = {0.25f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord darkHit = hitRecord(18, material);
    darkHit.uv = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {brightPath, darkPath}, {brightHit, darkHit}, settings);

    ASSERT_EQ(2u, result.pathStates.size());
    ASSERT_COLOR_NEAR(Colord::red(), Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), Colord(result.pathStates[1].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::red(), Colord(result.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), Colord(result.stepRecords[1].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, RecordsFirstHitDenoiserFeaturesForPrimarySampleZero) {
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.2, 0.3, 0.4)));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord primary = activePath(17);
    primary.pixelIndex = 5;
    primary.primarySampleIndex = 0;
    GpuDiffusePathStateRecord secondary = activePath(18);
    secondary.pixelIndex = 5;
    secondary.primarySampleIndex = 1;
    GpuIntersectionHitRecord primaryHit = hitRecord(17, material);
    primaryHit.distance = 3.5f;
    primaryHit.normal = {0.0f, 1.0f, 0.0f, 0.0f};
    GpuIntersectionHitRecord secondaryHit = hitRecord(18, material);
    secondaryHit.distance = 7.0f;

    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {primary, secondary}, {primaryHit, secondaryHit}, settings);

    ASSERT_EQ(1u, result.denoiserFeatureRecords.size());
    const GpuDiffusePathDenoiserFeatureRecord& feature = result.denoiserFeatureRecords[0];
    EXPECT_EQ(5u, feature.pixelIndex);
    EXPECT_EQ(0u, feature.primarySampleIndex);
    EXPECT_NE(0u, feature.flags & gpuDiffusePathDenoiserFeatureValidFlag);
    ASSERT_COLOR_NEAR(Colord(0.2, 0.3, 0.4), Colord(feature.albedo), 1e-6);
    expectFloat4Near(feature.normal, {0.0f, 1.0f, 0.0f, 0.0f});
    EXPECT_FLOAT_EQ(3.5f, feature.depth);
  }

  TEST(GpuDiffusePathLoop, CapturesDenoiserFeatureRecordsAcrossDepthLoop) {
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.2, 0.3, 0.4)));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.primarySampleIndex = 0;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const GpuDiffusePathLoopResult result = GpuDiffusePathLoop().run(sections, {path}, settings);

    EXPECT_TRUE(result.denoiserFeatureRecordsCaptured);
    ASSERT_EQ(1u, result.denoiserFeatureRecords.size());
    ASSERT_COLOR_NEAR(Colord(0.2, 0.3, 0.4), Colord(result.denoiserFeatureRecords[0].albedo), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesNearestImageTexture) {
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image =
      std::make_shared<ImageTexture>(new UVMapping2D, 2, 2, pixels, ImageTextureFilter::Nearest);
    auto matte = std::make_shared<MatteMaterial>(image);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord redPath = activePath(17);
    GpuDiffusePathStateRecord greenPath = activePath(18);
    GpuDiffusePathStateRecord bluePath = activePath(19);
    GpuIntersectionHitRecord redHit = hitRecord(17, material);
    redHit.uv = {0.25f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord greenHit = hitRecord(18, material);
    greenHit.uv = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord blueHit = hitRecord(19, material);
    blueHit.uv = {0.25f, 0.75f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {redPath, greenPath, bluePath}, {redHit, greenHit, blueHit}, settings);

    ASSERT_EQ(3u, result.pathStates.size());
    ASSERT_COLOR_NEAR(Colord::red(), Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::green(), Colord(result.pathStates[1].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), Colord(result.pathStates[2].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::red(), Colord(result.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::green(), Colord(result.stepRecords[1].continuationThroughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord::blue(), Colord(result.stepRecords[2].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesBilinearImageTexture) {
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image =
      std::make_shared<ImageTexture>(new UVMapping2D, 2, 2, pixels, ImageTextureFilter::Bilinear);
    auto matte = std::make_shared<MatteMaterial>(image);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuIntersectionHitRecord hit = hitRecord(17, material);
    hit.uv = {0.5f, 0.5f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {activePath(17)}, {hit}, settings);

    const Colord expected = image->sample(0.5, 0.5);
    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_COLOR_NEAR(expected, Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(expected, Colord(result.stepRecords[0].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesMipmappedImageTextureAtBaseLevel) {
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image =
      std::make_shared<ImageTexture>(new UVMapping2D, 2, 2, pixels, ImageTextureFilter::Mipmap);
    auto matte = std::make_shared<MatteMaterial>(image);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuIntersectionHitRecord hit = hitRecord(17, material);
    hit.uv = {0.5f, 0.5f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {activePath(17)}, {hit}, settings);

    const Colord expected = image->sample(0.5, 0.5);
    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_COLOR_NEAR(expected, Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(expected, Colord(result.stepRecords[0].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesTintedTexture) {
    auto tinted = std::make_shared<TintedTexture>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.5, 0.25, 0.125));
    auto matte = std::make_shared<MatteMaterial>(tinted);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {activePath()}, {hitRecord(7, material)}, settings);

    const Colord expected(0.125, 0.125, 0.09375);
    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_COLOR_NEAR(expected, Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(expected, Colord(result.stepRecords[0].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesNestedTintedTexture) {
    auto matte = std::make_shared<MatteMaterial>(nestedTintedConstantTexture());
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {activePath()}, {hitRecord(7, material)}, settings);

    const Colord expected = nestedTintedConstantTextureColor();
    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_COLOR_NEAR(expected, Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(expected, Colord(result.stepRecords[0].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesCheckerTextureGraph) {
    auto matte = std::make_shared<MatteMaterial>(checkerTextureGraph(new UVMapping2D(2.0, 2.0)));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord brightPath = activePath(17);
    GpuDiffusePathStateRecord darkPath = activePath(18);
    GpuIntersectionHitRecord brightHit = hitRecord(17, material);
    brightHit.uv = {0.25f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord darkHit = hitRecord(18, material);
    darkHit.uv = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {brightPath, darkPath}, {brightHit, darkHit}, settings);

    ASSERT_EQ(2u, result.pathStates.size());
    ASSERT_COLOR_NEAR(nestedTintedConstantTextureColor(), Colord(result.pathStates[0].throughput),
                      1e-6);
    ASSERT_COLOR_NEAR(checkerTextureGraphDarkColor(), Colord(result.pathStates[1].throughput),
                      1e-6);
    ASSERT_COLOR_NEAR(nestedTintedConstantTextureColor(),
                      Colord(result.stepRecords[0].continuationThroughput), 1e-6);
    ASSERT_COLOR_NEAR(checkerTextureGraphDarkColor(),
                      Colord(result.stepRecords[1].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesUvColorTexture) {
    auto matte = std::make_shared<MatteMaterial>(std::make_shared<UVColorTexture>());
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuIntersectionHitRecord hit = hitRecord(17, material);
    hit.uv = {0.25f, 0.75f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {activePath(17)}, {hit}, settings);

    const Colord expected(0.25, 0.75, 0.0);
    ASSERT_EQ(1u, result.pathStates.size());
    ASSERT_COLOR_NEAR(expected, Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(expected, Colord(result.stepRecords[0].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, MatteHitSamplesTintedNearestImageTexture) {
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image =
      std::make_shared<ImageTexture>(new UVMapping2D, 2, 2, pixels, ImageTextureFilter::Nearest);
    auto tinted = std::make_shared<TintedTexture>(image, Colord(0.5, 0.25, 0.125));
    auto matte = std::make_shared<MatteMaterial>(tinted);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);

    Scene scene;
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord redPath = activePath(17);
    GpuDiffusePathStateRecord greenPath = activePath(18);
    GpuIntersectionHitRecord redHit = hitRecord(17, material);
    redHit.uv = {0.25f, 0.25f, 0.0f, 0.0f};
    GpuIntersectionHitRecord greenHit = hitRecord(18, material);
    greenHit.uv = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathLoopSettings settings;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathStepResult result = GpuDiffusePathStepReference().step(
      sections, {redPath, greenPath}, {redHit, greenHit}, settings);

    ASSERT_EQ(2u, result.pathStates.size());
    ASSERT_COLOR_NEAR(Colord(0.5, 0.0, 0.0), Colord(result.pathStates[0].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.0, 0.25, 0.0), Colord(result.pathStates[1].throughput), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.5, 0.0, 0.0), Colord(result.stepRecords[0].continuationThroughput),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord(0.0, 0.25, 0.0), Colord(result.stepRecords[1].continuationThroughput),
                      1e-6);
  }

  TEST(GpuDiffusePathStepReference, DirectLightOcclusionSuppressesContribution) {
    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.8, 0.4, 0.2)));
    auto receiver = std::make_shared<Sphere>(Vector3d(10.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    auto blocker = std::make_shared<Sphere>(Vector3d(0.0, 0.0, -1.0), 0.25);
    blocker->setMaterial(matte);
    scene.add(receiver);
    scene.add(blocker);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -2.0), Colord::white()));
    GpuTracingSceneSections sections = sectionsFor(scene);
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {activePath()}, {hitRecord(7, material)});

    ASSERT_EQ(1u, result.directLightOcclusionRecords.size());
    EXPECT_EQ(1u, result.directLightOcclusionRecords[0].occluded);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.stepRecords[0].directLightRadiance), 1e-6);
    EXPECT_EQ(1u, result.metrics.directLightOccludedSamples);
    EXPECT_EQ(1u, result.stepRecords[0].directLightSampleCount);
    EXPECT_EQ(1u, result.stepRecords[0].directLightVisibilityRayCount);
    EXPECT_EQ(0u, result.stepRecords[0].directLightContributingSampleCount);
    EXPECT_EQ(1u, result.stepRecords[0].directLightOccludedSampleCount);
  }

  TEST(GpuDiffusePathStepReference, FixedSeedContinuationIsRepeatableAndSeedDependent) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord first = activePath();
    GpuDiffusePathStateRecord second = first;
    second.sampleSeed += 1;

    const GpuDiffusePathStepResult firstRun =
      GpuDiffusePathStepReference().step(sections, {first}, {hitRecord(7, material)});
    const GpuDiffusePathStepResult repeatedRun =
      GpuDiffusePathStepReference().step(sections, {first}, {hitRecord(7, material)});
    const GpuDiffusePathStepResult changedSeedRun =
      GpuDiffusePathStepReference().step(sections, {second}, {hitRecord(7, material)});

    ASSERT_EQ(1u, firstRun.pathStates.size());
    EXPECT_EQ(firstRun.pathStates[0].ray.direction, repeatedRun.pathStates[0].ray.direction);
    EXPECT_EQ(firstRun.pathStates[0].throughput, repeatedRun.pathStates[0].throughput);
    EXPECT_NE(firstRun.pathStates[0].ray.direction, changedSeedRun.pathStates[0].ray.direction);
    EXPECT_NEAR(1.0, Vector3d(firstRun.pathStates[0].ray.direction).length(), 1e-5);
  }

  TEST(GpuDiffusePathStepReference, DiffuseHitBeforeRouletteDepthEmitsNextPathRecord) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.throughput = {0.1f, 0.1f, 0.1f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    ASSERT_EQ(1u, result.pathStates.size());
    EXPECT_TRUE(result.terminatedPathStates.empty());
    EXPECT_TRUE(gpuDiffusePathStateIsActive(result.pathStates[0]));
    EXPECT_EQ(1u, result.pathStates[0].depth);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
    EXPECT_EQ(0u, result.metrics.terminatedPaths);
    ASSERT_COLOR_NEAR(Colord(0.1, 0.1, 0.1), Colord(result.pathStates[0].throughput), 1e-6);
  }

  TEST(GpuDiffusePathStepReference, RouletteTerminatedDiffuseHitEmitsNoNextPathRecord) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);
    sections.geometry = GpuIntersectionSceneBuffers{};
    const std::uint32_t material = firstMaterialId(sections, GpuTracingMaterialKind::Matte);

    GpuDiffusePathStateRecord path = activePath();
    path.depth = 3;
    path.throughput = {0.1f, 0.1f, 0.1f, 0.0f};

    const GpuDiffusePathStepResult result =
      GpuDiffusePathStepReference().step(sections, {path}, {hitRecord(7, material)});

    EXPECT_TRUE(result.pathStates.empty());
    ASSERT_EQ(1u, result.terminatedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.terminatedPathStates[0]));
    EXPECT_EQ(0u, result.metrics.spawnedContinuations);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
    EXPECT_NE(0u, result.stepRecords[0].flags & gpuDiffusePathStateTerminatedFlag);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.stepRecords[0].continuationThroughput), 1e-6);
  }

  TEST(GpuDiffusePathLoop, ResolvesMissedPathsIntoImage) {
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.75, 0.5, 0.25));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 4;
    const GpuDiffusePathLoopResult result = GpuDiffusePathLoop().run(sections, {path}, settings);

    EXPECT_EQ(1u, result.initialPathCount);
    EXPECT_EQ(1u, result.depthCount);
    ASSERT_EQ(1u, result.activePathsPerDepth.size());
    EXPECT_EQ(1u, result.activePathsPerDepth[0]);
    EXPECT_EQ(1u, result.peakActivePathCount());
    EXPECT_EQ(1u, result.lastActivePathCount());
    ASSERT_EQ(1u, result.resolvedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(result.resolvedPathStates[0].accumulatedRadiance), 1e-6);
    EXPECT_EQ(1u, result.metrics.misses);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);
    EXPECT_EQ("packed_cpu", result.metrics.closestHitExecutionPath);

    Buffer<unsigned int> resolved(1, 1);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(1, 1);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    EXPECT_EQ(Colord(0.125, 0.125, 0.09375).rgb(), resolved[0][0]);
    EXPECT_EQ("gpu_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("resident_accumulation_resolve", diagnostics.residency);
    EXPECT_EQ(layout.totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(1u, diagnostics.addOperations);
    EXPECT_EQ(1u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.resolveBytes(), diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoopResult, ReportsEmptyActivePathShapeAsZero) {
    const GpuDiffusePathLoopResult result;

    EXPECT_EQ(0u, result.peakActivePathCount());
    EXPECT_EQ(0u, result.lastActivePathCount());
    EXPECT_EQ(0u, result.compactionPassCount());
    EXPECT_EQ(0u, result.inputPathCount());
    EXPECT_EQ(0u, result.retainedPathCount());
    EXPECT_EQ(0u, result.removedPathCount());
    EXPECT_EQ(0u, result.submittedIntersectionRayCount());
    EXPECT_FALSE(result.fullGpuPathLoopSupported());
    EXPECT_TRUE(result.fullGpuPathLoopUnavailable());
    EXPECT_FALSE(result.hasPlatformAccumulation());
  }

  TEST(GpuDiffusePathLoopResult, ReportsResidentPathLoopCounts) {
    GpuDiffusePathLoopResult result;
    result.depthCount = 3;
    result.activePathsPerDepth = {4, 7, 2};
    result.metrics.activePaths = 13;
    result.metrics.closestHitRays = 9;
    result.metrics.directLightVisibilityRays = 6;
    result.metrics.spawnedContinuations = 5;
    result.resolvedPathStates.resize(8);

    EXPECT_EQ(7u, result.peakActivePathCount());
    EXPECT_EQ(2u, result.lastActivePathCount());
    EXPECT_EQ(3u, result.compactionPassCount());
    EXPECT_EQ(13u, result.inputPathCount());
    EXPECT_EQ(5u, result.retainedPathCount());
    EXPECT_EQ(8u, result.removedPathCount());
    EXPECT_EQ(15u, result.submittedIntersectionRayCount());
  }

  TEST(GpuDiffusePathLoopResult, SaturatesSubmittedIntersectionRayCount) {
    GpuDiffusePathLoopResult result;
    result.metrics.closestHitRays = std::numeric_limits<std::uint64_t>::max() - 2u;
    result.metrics.directLightVisibilityRays = 8u;

    EXPECT_EQ(std::numeric_limits<std::uint64_t>::max(), result.submittedIntersectionRayCount());
  }

  TEST(GpuDiffusePathLoopResult, ReportsFullGpuPathLoopAvailabilityFromExecutionPath) {
    GpuDiffusePathLoopResult result;

    EXPECT_FALSE(result.fullGpuPathLoopSupported());
    EXPECT_TRUE(result.fullGpuPathLoopUnavailable());
    EXPECT_EQ("none", result.platformLabel());

    result.executionPath = "full_gpu_subset";

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_FALSE(result.fullGpuPathLoopUnavailable());
    EXPECT_EQ("platform_gpu_path_loop", result.platformLabel());

    result.platformName = "metal";

    EXPECT_EQ("metal", result.platformLabel());
  }

  TEST(GpuDiffusePathLoopBackend, DefaultFullGpuSceneSupportFollowsBackendAvailability) {
    const GpuDiffusePathLoopBackendSupport unavailableSupport =
      CpuReferenceGpuDiffusePathLoopBackend::sharedInstance()->fullGpuPathLoopSupport({});

    EXPECT_FALSE(unavailableSupport.supported);
    EXPECT_EQ("platform full-GPU path-loop kernel is not available yet", unavailableSupport.reason);

    const AvailableFullGpuPathLoopBackend backend;
    const GpuDiffusePathLoopBackendSupport availableSupport = backend.fullGpuPathLoopSupport({});

    EXPECT_TRUE(availableSupport.supported);
    EXPECT_TRUE(availableSupport.reason.empty());
  }

  TEST(GpuDiffusePathLoopBackend, AvailableFullGpuBackendCanRejectSpecificScene) {
    const SceneRejectingFullGpuPathLoopBackend backend;

    ASSERT_TRUE(backend.fullGpuPathLoopAvailable());
    const GpuDiffusePathLoopBackendSupport support = backend.fullGpuPathLoopSupport({});

    EXPECT_FALSE(support.supported);
    EXPECT_EQ("test backend supports only a narrower scene subset", support.reason);
  }

  TEST(GpuDiffusePathLoopBackend, SelectsLaterBackendWhenEarlierBackendRejectsScene) {
    const std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>> backends{
      std::make_shared<SceneRejectingFullGpuPathLoopBackend>(),
      std::make_shared<AvailableFullGpuPathLoopBackend>()};

    const GpuDiffusePathLoopBackendChoice choice = selectFullGpuDiffusePathLoopBackend(
      backends, GpuTracingSceneSections(), GpuDiffusePathLoopSettings());

    ASSERT_TRUE(choice.backend);
    EXPECT_STREQ("available_full_gpu_path_loop", choice.backend->name());
    EXPECT_TRUE(choice.fallbackReason.empty());
  }

  TEST(GpuDiffusePathLoopBackend, ReportsFirstRelevantReasonWhenNoBackendSupportsScene) {
    {
      const std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>> backends{
        std::make_shared<UnavailableFullGpuPathLoopBackend>(),
        std::make_shared<SceneRejectingFullGpuPathLoopBackend>()};

      const GpuDiffusePathLoopBackendChoice choice = selectFullGpuDiffusePathLoopBackend(
        backends, GpuTracingSceneSections(), GpuDiffusePathLoopSettings());

      EXPECT_FALSE(choice.backend);
      EXPECT_EQ("test backend supports only a narrower scene subset", choice.fallbackReason);
    }
    {
      const std::vector<std::shared_ptr<const GpuDiffusePathLoopBackend>> backends{
        std::make_shared<UnavailableFullGpuPathLoopBackend>()};

      const GpuDiffusePathLoopBackendChoice choice = selectFullGpuDiffusePathLoopBackend(
        backends, GpuTracingSceneSections(), GpuDiffusePathLoopSettings());

      EXPECT_FALSE(choice.backend);
      EXPECT_EQ("test backend is offline", choice.fallbackReason);
    }
  }

  TEST(GpuDiffusePathLoopBackend, SharedPlatformAccumulationPlanUsesSampleSlotsForDuplicatePixels) {
    std::vector<GpuDiffusePathStateRecord> paths{activePath(10), activePath(11)};
    paths[0].pixelIndex = 0;
    paths[0].primarySampleIndex = 0;
    paths[1].pixelIndex = 0;
    paths[1].primarySampleIndex = 1;

    const GpuDiffusePathLoopPlatformAccumulationPlan plan =
      platformGpuDiffusePathLoopAccumulationPlanFor(paths, "Test");

    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetSampleSlot, plan.targetMode);
    EXPECT_EQ(1, plan.layout.width);
    EXPECT_EQ(2, plan.layout.height);
  }

  TEST(GpuDiffusePathLoopBackend, SharedPlatformResultUsesGpuDirectLightStepCounters) {
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;

    GpuDiffusePathLoopPlatformResult platform;
    fillEchoedLaunchParameters(platform, 1, settings);
    platform.executionPath = "test_path_loop";
    platform.pathStateResidency = "test_path_state";
    platform.accumulationColorSums = {{{0.25f, 0.5f, 0.75f, 0.0f}}};
    platform.accumulationSampleCounts = {1};
    platform.resolvedPathStates = {activePath(10)};
    platform.resolvedPathStates[0].flags = gpuDiffusePathStateTerminatedFlag;
    platform.stepRecords.resize(1);
    platform.stepRecords[0].event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit);
    platform.stepRecords[0].directLightRadiance = {0.25f, 0.5f, 0.75f, 0.0f};
    platform.stepRecords[0].directLightSampleCount = 4;
    platform.stepRecords[0].directLightVisibilityRayCount = 3;
    platform.stepRecords[0].directLightContributingSampleCount = 2;
    platform.stepRecords[0].directLightOccludedSampleCount = 1;

    GpuDiffusePathLoopResult result = makePlatformGpuDiffusePathLoopResult(
      1, settings, std::move(platform), "Test", "test", "test_path_state", "test_accumulation",
      "test_accumulation_residency");

    EXPECT_EQ(4u, result.metrics.directLightSamples);
    EXPECT_EQ(3u, result.metrics.directLightVisibilityRays);
    EXPECT_EQ(2u, result.metrics.directLightContributionEvaluations);
    EXPECT_EQ(2u, result.metrics.directLightContributingSamples);
    EXPECT_EQ(1u, result.metrics.directLightOccludedSamples);
  }

  TEST(GpuDiffusePathLoopBackend, SharedPlatformResultHonorsTraceDisabledNoReadback) {
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.captureDiagnostics = false;
    settings.captureMetrics = false;

    GpuDiffusePathLoopPlatformResult platform;
    fillEchoedLaunchParameters(platform, 2, settings, 1, 2);
    platform.echoedParameters.accumulationTargetMode =
      gpuDiffusePathLoopAccumulationTargetSampleSlot;
    platform.executionPath = "test_path_loop";
    platform.pathStateResidency = "test_path_state";
    platform.retainedFrontierDispatchesIndirect = true;
    platform.retainedPathCount = 1;
    platform.activePathCountsPerDepth = {2, 1};
    platform.accumulationColorSums = {{{1.0f, 0.0f, 0.0f, 0.0f}}, {{0.0f, 1.0f, 0.0f, 0.0f}}};
    platform.accumulationSampleCounts = {1, 1};
    platform.resolvedPathStates = {activePath(10), activePath(11)};
    platform.resolvedPathStates[0].flags = gpuDiffusePathStateTerminatedFlag;
    platform.resolvedPathStates[1].flags = gpuDiffusePathStateTerminatedFlag;
    platform.stepRecords.resize(2);
    platform.stepRecords[0].event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit);
    platform.stepRecords[0].depth = 0;
    platform.stepRecords[0].continuationThroughput = {0.5f, 0.5f, 0.5f, 0.0f};
    platform.stepRecords[1].event = static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss);
    platform.stepRecords[1].depth = 1;

    GpuDiffusePathLoopResult result = makePlatformGpuDiffusePathLoopResult(
      2, settings, std::move(platform), "Test", "test", "test_path_state", "test_accumulation",
      "test_accumulation_residency");

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("test", result.platformName);
    EXPECT_EQ("depth_frontier", result.schedule);
    EXPECT_EQ("test_path_loop", result.frontierCompactionExecutionPath);
    EXPECT_EQ("test_path_state", result.pathStateResidency);
    EXPECT_EQ("test_path_state", result.frontierCompactionPathStateResidency);
    EXPECT_TRUE(result.retainedFrontierDispatchesIndirect);
    EXPECT_TRUE(result.resolvedPathStates.empty());
    EXPECT_EQ(2u, result.initialPathCount);
    EXPECT_EQ(0u, result.retainedIndexBytes);
    EXPECT_EQ(0u, result.metrics.activePaths);
    EXPECT_EQ(0u, result.metrics.spawnedContinuations);
    EXPECT_EQ(0u, result.metrics.terminatedPaths);
    EXPECT_TRUE(result.activePathsPerDepth.empty());
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetSampleSlot,
              result.platformAccumulationTargetMode);
    EXPECT_EQ("test_accumulation", result.platformAccumulationBackend);
    EXPECT_EQ("test_accumulation_residency", result.platformAccumulationResidency);
  }

  TEST(GpuDiffusePathLoopBackend, SharedPlatformResultReportsResolvedDisplayReadbacks) {
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.captureDiagnostics = false;
    settings.captureMetrics = false;
    settings.capturePlatformAccumulation = false;
    settings.captureResolvedDisplay = true;

    GpuDiffusePathLoopPlatformResult platform;
    fillEchoedLaunchParameters(platform, 2, settings, 2, 1);
    platform.executionPath = "test_path_loop";
    platform.pathStateResidency = "test_path_state";
    platform.resolvedDisplayPixels = {0x112233u, 0x445566u};
    platform.resolvedDisplayReadbacks = 1u;

    GpuDiffusePathLoopResult result = makePlatformGpuDiffusePathLoopResult(
      2, settings, std::move(platform), "Test", "test", "test_path_state", "test_accumulation",
      "test_accumulation_residency");

    EXPECT_EQ(1u, result.platformResolvedDisplayReadbacks);
    ASSERT_EQ(2u, result.platformResolvedDisplayPixels.size());
    EXPECT_EQ(0x445566u, result.platformResolvedDisplayPixels[1]);
  }

  TEST(GpuDiffusePathLoopBackend, SharedPlatformResultRejectsMismatchedLaunchEcho) {
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.directLightSamples = 4;

    GpuDiffusePathLoopPlatformResult platform;
    fillEchoedLaunchParameters(platform, 1, settings);
    platform.echoedParameters.directLightSamples = 1;
    platform.accumulationColorSums = {{{0.25f, 0.5f, 0.75f, 0.0f}}};
    platform.accumulationSampleCounts = {1};

    EXPECT_THROW((void)makePlatformGpuDiffusePathLoopResult(
                   1, settings, std::move(platform), "Test", "test", "test_path_state",
                   "test_accumulation", "test_accumulation_residency"),
                 std::logic_error);
  }

  TEST(GpuDiffusePathLoopBackend, SharedPlatformResultRejectsMalformedPlatformReadbacks) {
    {
      GpuDiffusePathLoopSettings settings;
      GpuDiffusePathLoopPlatformResult platform;
      fillEchoedLaunchParameters(platform, 1, settings, 2, 1);
      platform.accumulationColorSums = {{{0.25f, 0.5f, 0.75f, 0.0f}}};
      platform.accumulationSampleCounts = {1};

      EXPECT_THROW((void)makePlatformGpuDiffusePathLoopResult(
                     1, settings, std::move(platform), "Test", "test", "test_path_state",
                     "test_accumulation", "test_accumulation_residency"),
                   std::logic_error);
    }
    {
      GpuDiffusePathLoopSettings settings;
      settings.capturePlatformAccumulation = false;
      GpuDiffusePathLoopPlatformResult platform;
      fillEchoedLaunchParameters(platform, 1, settings);
      platform.accumulationColorSums = {{{0.25f, 0.5f, 0.75f, 0.0f}}};
      platform.accumulationSampleCounts = {1};

      EXPECT_THROW((void)makePlatformGpuDiffusePathLoopResult(
                     1, settings, std::move(platform), "Test", "test", "test_path_state",
                     "test_accumulation", "test_accumulation_residency"),
                   std::logic_error);
    }
    {
      GpuDiffusePathLoopSettings settings;
      settings.captureResolvedDisplay = true;
      GpuDiffusePathLoopPlatformResult platform;
      fillEchoedLaunchParameters(platform, 1, settings);
      platform.accumulationColorSums = {{{0.25f, 0.5f, 0.75f, 0.0f}}};
      platform.accumulationSampleCounts = {1};

      EXPECT_THROW((void)makePlatformGpuDiffusePathLoopResult(
                     1, settings, std::move(platform), "Test", "test", "test_path_state",
                     "test_accumulation", "test_accumulation_residency"),
                   std::logic_error);
    }
  }

  TEST(GpuDiffusePathLoopResult, ReportsCpuReferenceTracingCapabilitiesAsGpuFallbacks) {
    GpuDiffusePathLoopResult result;
    result.metrics.closestHitExecutionPath = "packed_cpu";
    result.metrics.directLightVisibilityExecutionPath = "packed_cpu";
    result.metrics.directLightContributionExecutionPath = "cpu_record";
    result.pathStateResidency = "cpu_host";
    TracingAccumulationDiagnostics accumulation;
    accumulation.backend = "gpu_diffuse_path_loop";
    accumulation.residency = "resident_accumulation_resolve";

    const TracingExecutionCapabilityRecords capabilities = result.tracingCapabilities(accumulation);

    EXPECT_TRUE(capabilities.hasFallback());
    EXPECT_EQ(TracingCapabilitySupport::Fallback, capabilities.intersection.closestHit.support);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.intersection.closestHit.requestedDevice);
    EXPECT_EQ(TracingExecutionDevice::CPU, capabilities.intersection.closestHit.resolvedDevice);
    EXPECT_EQ("packed_cpu", capabilities.intersection.closestHit.executionPath);
    EXPECT_EQ("platform full-GPU path-loop kernel is not available yet",
              capabilities.intersection.closestHit.fallback.reason);

    EXPECT_EQ(TracingCapabilitySupport::Restricted, capabilities.scene.materialRecords.support);
    EXPECT_EQ(TracingExecutionDevice::CPU, capabilities.scene.materialRecords.resolvedDevice);
    EXPECT_EQ("host_records", capabilities.scene.materialRecords.executionPath);

    EXPECT_EQ(TracingCapabilitySupport::Fallback, capabilities.bsdf.sample.support);
    EXPECT_EQ("compiled CPU-reference path loop samples diffuse BSDF continuations on the host",
              capabilities.bsdf.sample.fallback.reason);
    EXPECT_EQ(TracingCapabilitySupport::Fallback,
              capabilities.pathState.frontierCompaction.support);
    EXPECT_EQ("cpu_diffuse_frontier_compaction",
              capabilities.pathState.frontierCompaction.executionPath);
    EXPECT_EQ("compiled CPU-reference path loop compacts path state on the host",
              capabilities.pathState.frontierCompaction.fallback.reason);
    EXPECT_EQ(TracingCapabilitySupport::Fallback,
              capabilities.accumulation.sampleAccumulation.support);
    EXPECT_EQ("gpu_diffuse_path_loop", capabilities.accumulation.sampleAccumulation.executionPath);
    EXPECT_EQ(TracingCapabilitySupport::Supported,
              capabilities.accumulation.progressiveReadback.support);
    EXPECT_EQ(TracingExecutionDevice::CPU,
              capabilities.accumulation.progressiveReadback.resolvedDevice);
  }

  TEST(GpuDiffusePathLoopResult, ReportsGpuFrontierCompactionWithoutFullGpuPathLoop) {
    GpuDiffusePathLoopResult result;
    result.metrics.closestHitExecutionPath = "packed_cpu";
    result.metrics.directLightVisibilityExecutionPath = "packed_cpu";
    result.metrics.directLightContributionExecutionPath = "cpu_record";
    result.pathStateResidency = "cpu_host";
    result.frontierCompactionExecutionPath = "metal_diffuse_frontier_compaction";
    result.frontierCompactionPathStateResidency = "metal_shared_diffuse_path_state";
    TracingAccumulationDiagnostics accumulation;
    accumulation.backend = "gpu_diffuse_path_loop";
    accumulation.residency = "resident_accumulation_resolve";

    const TracingExecutionCapabilityRecords capabilities = result.tracingCapabilities(accumulation);

    EXPECT_TRUE(capabilities.hasFallback());
    EXPECT_EQ(TracingCapabilitySupport::Fallback, capabilities.pathState.residency.support);
    EXPECT_EQ("compiled CPU-reference path loop keeps path state on the host",
              capabilities.pathState.residency.fallback.reason);
    EXPECT_EQ(TracingCapabilitySupport::Supported,
              capabilities.pathState.frontierCompaction.support);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.pathState.frontierCompaction.requestedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.pathState.frontierCompaction.resolvedDevice);
    EXPECT_EQ("metal_diffuse_frontier_compaction",
              capabilities.pathState.frontierCompaction.executionPath);
    EXPECT_TRUE(capabilities.pathState.frontierCompaction.fallback.reason.empty());
  }

  TEST(GpuDiffusePathLoopResult, ReportsFullGpuTracingCapabilitiesWithoutCpuFallbacks) {
    GpuDiffusePathLoopResult result;
    result.executionPath = "full_gpu_subset";
    result.platformName = "metal";
    result.pathStateResidency = "metal_path_state";
    result.frontierCompactionExecutionPath = "metal_path_loop";
    result.frontierCompactionPathStateResidency = "metal_path_state";
    result.metrics.closestHitExecutionPath = "metal";
    result.metrics.directLightVisibilityExecutionPath = "metal";
    result.metrics.directLightContributionExecutionPath = "metal_path_loop";
    TracingAccumulationDiagnostics accumulation;
    accumulation.backend = "metal_accumulation";
    accumulation.residency = "metal_shared_accumulation";

    const TracingExecutionCapabilityRecords capabilities = result.tracingCapabilities(accumulation);

    EXPECT_FALSE(capabilities.hasFallback());
    EXPECT_EQ(TracingCapabilitySupport::Supported, capabilities.intersection.closestHit.support);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.intersection.closestHit.requestedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.intersection.closestHit.resolvedDevice);
    EXPECT_EQ("metal", capabilities.intersection.closestHit.platform);
    EXPECT_EQ("metal", capabilities.intersection.closestHit.executionPath);

    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.scene.geometryRecords.resolvedDevice);
    EXPECT_EQ("gpu_tracing_scene_records", capabilities.scene.geometryRecords.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.sampling.gpuRng.resolvedDevice);
    EXPECT_EQ("gpu_sample_stream", capabilities.sampling.gpuRng.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.directLighting.residentBatch.resolvedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.bsdf.eval.resolvedDevice);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.bsdf.deltaBranches.resolvedDevice);
    EXPECT_EQ(TracingCapabilitySupport::Supported, capabilities.bsdf.deltaBranches.support);
    EXPECT_EQ("full_gpu_subset", capabilities.bsdf.deltaBranches.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU, capabilities.pathState.residency.resolvedDevice);
    EXPECT_EQ("metal_path_state", capabilities.pathState.residency.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.pathState.frontierCompaction.resolvedDevice);
    EXPECT_EQ("metal_path_loop", capabilities.pathState.frontierCompaction.executionPath);
    EXPECT_EQ(TracingExecutionDevice::GPU,
              capabilities.accumulation.sampleAccumulation.resolvedDevice);
    EXPECT_EQ("metal_accumulation", capabilities.accumulation.sampleAccumulation.executionPath);
    EXPECT_EQ(TracingExecutionDevice::Hybrid,
              capabilities.accumulation.progressiveReadback.resolvedDevice);
    EXPECT_EQ("metal_shared_accumulation",
              capabilities.accumulation.progressiveReadback.executionPath);
  }

  TEST(GpuDiffusePathLoop, ReportsCpuReferenceResidencyAndCompactionDiagnostics) {
    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.25, 0.5, 0.75));
    GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;

    const GpuDiffusePathLoopResult result = GpuDiffusePathLoop().run(sections, {path});
    const std::uint64_t pathStateBytes = sizeof(GpuDiffusePathStateRecord);

    EXPECT_EQ("compiled_cpu_reference", result.executionPath);
    EXPECT_EQ("cpu_host", result.pathStateResidency);
    EXPECT_EQ("cpu_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
    EXPECT_EQ("cpu_host", result.frontierCompactionPathStateResidency);
    EXPECT_EQ(pathStateBytes, result.pathStateBytesPerPath());
    EXPECT_EQ(pathStateBytes, result.residentPathStateBytes());
    EXPECT_EQ(pathStateBytes, result.inputPathStateBytes());
    EXPECT_EQ(0u, result.retainedPathStateBytes());
    EXPECT_EQ(pathStateBytes, result.removedPathStateBytes());
    EXPECT_EQ(0u, result.retainedPathIndexBytes());
    EXPECT_EQ(1u, result.compactionPassCount());
    EXPECT_EQ(1u, result.inputPathCount());
    EXPECT_EQ(0u, result.retainedPathCount());
    EXPECT_EQ(1u, result.removedPathCount());
    EXPECT_EQ(1u, result.submittedIntersectionRayCount());
    EXPECT_EQ(0u, result.movedPathCount());
    EXPECT_DOUBLE_EQ(1.0, result.removedPathFraction());
    EXPECT_DOUBLE_EQ(0.0, result.movedRetainedPathFraction());
    EXPECT_EQ(1u, result.roundTrips);
    EXPECT_EQ(0u, result.savedHostReadbacks);
    EXPECT_EQ(0u, result.savedHostReadbackBytes);
  }

  TEST(CpuReferenceGpuDiffusePathFrontierCompactionBackend, CompactsRetainedPathsInOrder) {
    std::vector<GpuDiffusePathStateRecord> source{activePath(2), activePath(3), activePath(4)};
    source[0].pixelIndex = 10;
    source[1].pixelIndex = 11;
    source[2].pixelIndex = 12;

    const GpuDiffusePathFrontierCompactionResult result =
      CpuReferenceGpuDiffusePathFrontierCompactionBackend::instance().compact(source, {0u, 2u});

    EXPECT_EQ("cpu_diffuse_frontier_compaction", result.executionPath);
    EXPECT_EQ("cpu_host", result.pathStateResidency);
    EXPECT_EQ(3u, result.inputPathCount);
    EXPECT_EQ(2u, result.retainedPathCount());
    EXPECT_EQ(1u, result.removedPathCount());
    EXPECT_EQ(1u, result.movedPathCount());
    EXPECT_EQ(2u * sizeof(std::uint32_t), result.retainedIndexBytes());
    ASSERT_EQ(2u, result.retainedRecords.size());
    EXPECT_EQ(10u, result.retainedRecords[0].pixelIndex);
    EXPECT_EQ(12u, result.retainedRecords[1].pixelIndex);
  }

  TEST(MetalGpuDiffusePathFrontierCompactionBackend, CompactsRetainedPathsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathFrontierCompactionBackend backend;
    if (!backend.compactionPathAvailable()) {
      GTEST_SKIP() << backend.compactionPathUnavailableReason();
    }

    std::vector<GpuDiffusePathStateRecord> source{activePath(20), activePath(21), activePath(22)};
    source[0].pixelIndex = 100;
    source[1].pixelIndex = 101;
    source[2].pixelIndex = 102;
    source[1].sampleSeed = 9001;
    source[2].depth = 3;
    source[2].previousBsdfPdf = 0.25f;

    const GpuDiffusePathFrontierCompactionResult result = backend.compact(source, {1u, 2u});

    EXPECT_EQ("metal_diffuse_frontier_compaction", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(3u, result.inputPathCount);
    ASSERT_EQ(2u, result.retainedRecords.size());
    EXPECT_EQ(101u, result.retainedRecords[0].pixelIndex);
    EXPECT_EQ(9001u, result.retainedRecords[0].sampleSeed);
    EXPECT_EQ(21u, result.retainedRecords[0].ray.rayIndex);
    EXPECT_EQ(102u, result.retainedRecords[1].pixelIndex);
    EXPECT_EQ(3u, result.retainedRecords[1].depth);
    EXPECT_FLOAT_EQ(0.25f, result.retainedRecords[1].previousBsdfPdf);
    EXPECT_GT(result.uploadWorkerSeconds, 0.0);
    EXPECT_GT(result.kernelWorkerSeconds, 0.0);
    EXPECT_GT(result.readbackWorkerSeconds, 0.0);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathFrontierCompactionBackend, CompactsRetainedPathsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    VulkanGpuDiffusePathFrontierCompactionBackend backend;
    if (!backend.compactionPathAvailable()) {
      GTEST_SKIP() << backend.compactionPathUnavailableReason();
    }

    std::vector<GpuDiffusePathStateRecord> source{activePath(30), activePath(31), activePath(32)};
    source[0].pixelIndex = 200;
    source[1].pixelIndex = 201;
    source[2].pixelIndex = 202;
    source[1].sampleSeed = 7001;
    source[2].depth = 4;
    source[2].previousLightPdf = 0.125f;

    const GpuDiffusePathFrontierCompactionResult result = backend.compact(source, {1u, 2u});

    EXPECT_EQ("vulkan_diffuse_frontier_compaction", result.executionPath);
    EXPECT_EQ("vulkan_host_visible_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(3u, result.inputPathCount);
    ASSERT_EQ(2u, result.retainedRecords.size());
    EXPECT_EQ(201u, result.retainedRecords[0].pixelIndex);
    EXPECT_EQ(7001u, result.retainedRecords[0].sampleSeed);
    EXPECT_EQ(31u, result.retainedRecords[0].ray.rayIndex);
    EXPECT_EQ(202u, result.retainedRecords[1].pixelIndex);
    EXPECT_EQ(4u, result.retainedRecords[1].depth);
    EXPECT_FLOAT_EQ(0.125f, result.retainedRecords[1].previousLightPdf);
    EXPECT_GT(result.uploadWorkerSeconds, 0.0);
    EXPECT_GT(result.kernelWorkerSeconds, 0.0);
    EXPECT_GT(result.readbackWorkerSeconds, 0.0);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(GpuDiffusePathLoop, DispatchesSurvivingFrontierThroughCompactionBackend) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;

    RecordingFrontierCompactionBackend backend;
    const GpuDiffusePathLoopResult result =
      GpuDiffusePathLoop().run(sections, {path}, settings, backend);

    EXPECT_EQ(2, backend.calls);
    ASSERT_EQ(2u, backend.inputCounts.size());
    EXPECT_EQ(1u, backend.inputCounts[0]);
    EXPECT_EQ(0u, backend.inputCounts[1]);
    ASSERT_EQ(2u, backend.retainedIndices.size());
    EXPECT_EQ(std::vector<std::uint32_t>({0u}), backend.retainedIndices[0]);
    EXPECT_TRUE(backend.retainedIndices[1].empty());
    EXPECT_EQ("recording_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
    EXPECT_EQ("recording_path_state", result.frontierCompactionPathStateResidency);
    EXPECT_EQ(sizeof(std::uint32_t), result.retainedPathIndexBytes());
    EXPECT_EQ(2u, result.compactionPassCount());
    EXPECT_DOUBLE_EQ(0.02, result.frontierCompactionUploadWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.04, result.frontierCompactionKernelWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.06, result.frontierCompactionReadbackWorkerSeconds);
  }

  TEST(CompactingGpuDiffusePathLoopBackend, DispatchesLoopThroughInjectedCompactionBackend) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    auto compactionBackend = std::make_shared<RecordingFrontierCompactionBackend>();
    const CompactingGpuDiffusePathLoopBackend backend(compactionBackend);

    const GpuDiffusePathLoopResult result = backend.run(sections, {activePath()}, settings);

    EXPECT_EQ("compiled_cpu_reference_with_compaction_backend", std::string(backend.name()));
    EXPECT_EQ("recording_diffuse_frontier_compaction",
              std::string(backend.compactionBackend().name()));
    EXPECT_EQ(2, compactionBackend->calls);
    EXPECT_EQ("compiled_cpu_reference", result.executionPath);
    EXPECT_EQ("cpu_host", result.pathStateResidency);
    EXPECT_EQ("recording_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
    EXPECT_EQ("recording_path_state", result.frontierCompactionPathStateResidency);
    EXPECT_DOUBLE_EQ(0.02, result.frontierCompactionUploadWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.04, result.frontierCompactionKernelWorkerSeconds);
    EXPECT_DOUBLE_EQ(0.06, result.frontierCompactionReadbackWorkerSeconds);
  }

  TEST(GpuDiffusePathLoopBackend, DefaultGpuRequestBackendRunsCompiledLoop) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const std::shared_ptr<const GpuDiffusePathLoopBackend> backend =
      GpuDiffusePathLoopBackend::defaultBackendForGpuRequest();

    ASSERT_NE(nullptr, backend);
    EXPECT_FALSE(backend->fullGpuPathLoopAvailable());
    EXPECT_STREQ("platform full-GPU path-loop kernel is not available yet",
                 backend->fullGpuPathLoopUnavailableReason());
    EXPECT_STREQ("", backend->platformName());
    const GpuDiffusePathLoopResult result = backend->run(sections, {activePath()}, settings);

    EXPECT_EQ("compiled_cpu_reference", result.executionPath);
    EXPECT_FALSE(result.frontierCompactionExecutionPath.empty());
    EXPECT_FALSE(result.frontierCompactionPathStateResidency.empty());
    if (std::string(backend->name()) == "compiled_cpu_reference") {
      EXPECT_EQ("cpu_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
      EXPECT_EQ("cpu_host", result.frontierCompactionPathStateResidency);
    } else {
      EXPECT_EQ("compiled_cpu_reference_with_compaction_backend", std::string(backend->name()));
      EXPECT_NE("cpu_diffuse_frontier_compaction", result.frontierCompactionExecutionPath);
      EXPECT_NE("cpu_host", result.frontierCompactionPathStateResidency);
    }
  }

  TEST(GpuDiffusePathLoopBackend, DefaultFullGpuBackendNamesPlatformBackendWhenBuilt) {
    const std::shared_ptr<const GpuDiffusePathLoopBackend> backend =
      GpuDiffusePathLoopBackend::defaultFullGpuBackendForGpuRequest();
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT) && defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend metalBackend;
    const VulkanGpuDiffusePathLoopBackend vulkanBackend;

    ASSERT_NE(nullptr, backend);
    if (metalBackend.fullGpuPathLoopAvailable()) {
      EXPECT_STREQ("metal_diffuse_path_loop", backend->name());
      EXPECT_STREQ("metal", backend->platformName());
    } else if (vulkanBackend.fullGpuPathLoopAvailable()) {
      EXPECT_STREQ("vulkan_diffuse_path_loop", backend->name());
      EXPECT_STREQ("vulkan", backend->platformName());
    } else {
      EXPECT_STREQ("metal_diffuse_path_loop", backend->name());
      EXPECT_STREQ("metal", backend->platformName());
    }
#elif defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("metal_diffuse_path_loop", backend->name());
    EXPECT_STREQ("metal", backend->platformName());
#elif defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    ASSERT_NE(nullptr, backend);
    EXPECT_STREQ("vulkan_diffuse_path_loop", backend->name());
    EXPECT_STREQ("vulkan", backend->platformName());
#else
    EXPECT_EQ(nullptr, backend);
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, ReportsUnavailableWhenBuildOrDeviceCannotRunMetal) {
    const MetalGpuDiffusePathLoopBackend backend;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const GpuDiffusePathLoopBackendSupport support =
      backend.fullGpuPathLoopSupport(GpuTracingSceneSections(), settings);

#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    if (backend.fullGpuPathLoopAvailable()) {
      EXPECT_TRUE(support.supported);
      EXPECT_TRUE(support.reason.empty());
    } else {
      EXPECT_FALSE(support.supported);
      EXPECT_FALSE(support.reason.empty());
      EXPECT_EQ(MetalGpuDiffusePathLoopKernel().launchPathUnavailableReason(), support.reason);
      EXPECT_STREQ(support.reason.c_str(), backend.fullGpuPathLoopUnavailableReason());
    }
#else
    EXPECT_FALSE(backend.fullGpuPathLoopAvailable());
    EXPECT_FALSE(support.supported);
    EXPECT_EQ("Metal diffuse path-loop backend is not enabled in this build", support.reason);
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, SupportsMultiDepthSettingsForRestrictedSceneWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;

    const GpuDiffusePathLoopBackendSupport support =
      backend.fullGpuPathLoopSupport(sections, settings);

    EXPECT_TRUE(support.supported);
    EXPECT_TRUE(support.reason.empty());

    Scene portalScene;
    auto portal =
      std::make_shared<PortalMaterial>(Matrix4d::translate(0.0, 0.0, 2.0), Colord(0.25, 0.5, 0.75));
    auto portalSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    portalSphere->setMaterial(portal);
    portalScene.add(portalSphere);
    const GpuDiffusePathLoopBackendSupport portalSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(portalScene), settings);
    EXPECT_TRUE(portalSupport.supported);
    EXPECT_TRUE(portalSupport.reason.empty());

    Scene unsupportedMaterialScene;
    auto unsupportedMaterialSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedMaterialSphere->setMaterial(std::make_shared<UnsupportedGpuTracingMaterial>());
    unsupportedMaterialScene.add(unsupportedMaterialSphere);
    const GpuTracingSceneSections unsupportedMaterialSections =
      sectionsFor(unsupportedMaterialScene);
    const GpuDiffusePathLoopBackendSupport unsupportedMaterialSupport =
      backend.fullGpuPathLoopSupport(unsupportedMaterialSections, settings);
    EXPECT_FALSE(unsupportedMaterialSupport.supported);
    EXPECT_EQ("Metal diffuse path-loop backend currently supports Matte, Phong finite glossy, "
              "Reflective mirror, Transparent refraction, Emissive, and Portal materials only",
              unsupportedMaterialSupport.reason);

    Scene tintedScene;
    auto tinted = std::make_shared<TintedTexture>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.5, 0.25, 0.125));
    auto tintedMatte = std::make_shared<MatteMaterial>(tinted);
    auto tintedSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    tintedSphere->setMaterial(tintedMatte);
    tintedScene.add(tintedSphere);
    const GpuDiffusePathLoopBackendSupport tintedSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(tintedScene), settings);
    EXPECT_TRUE(tintedSupport.supported);
    EXPECT_TRUE(tintedSupport.reason.empty());

    Scene nestedTintedScene;
    auto nestedTintedMatte = std::make_shared<MatteMaterial>(nestedTintedConstantTexture());
    auto nestedTintedSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    nestedTintedSphere->setMaterial(nestedTintedMatte);
    nestedTintedScene.add(nestedTintedSphere);
    const GpuDiffusePathLoopBackendSupport nestedTintedSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(nestedTintedScene), settings);
    EXPECT_TRUE(nestedTintedSupport.supported);
    EXPECT_TRUE(nestedTintedSupport.reason.empty());

    Scene checkerGraphScene;
    auto checkerGraphMatte =
      std::make_shared<MatteMaterial>(checkerTextureGraph(new PlanarMapping2D));
    auto checkerGraphSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    checkerGraphSphere->setMaterial(checkerGraphMatte);
    checkerGraphScene.add(checkerGraphSphere);
    const GpuDiffusePathLoopBackendSupport checkerGraphSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(checkerGraphScene), settings);
    EXPECT_TRUE(checkerGraphSupport.supported);
    EXPECT_TRUE(checkerGraphSupport.reason.empty());

    Scene bilinearImageScene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto bilinearImage = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                        ImageTextureFilter::Bilinear);
    auto bilinearImageMatte = std::make_shared<MatteMaterial>(bilinearImage);
    auto bilinearImageSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    bilinearImageSphere->setMaterial(bilinearImageMatte);
    bilinearImageScene.add(bilinearImageSphere);
    const GpuDiffusePathLoopBackendSupport bilinearImageSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(bilinearImageScene), settings);
    EXPECT_TRUE(bilinearImageSupport.supported);
    EXPECT_TRUE(bilinearImageSupport.reason.empty());

    Scene mipmappedImageScene;
    auto mipmappedImage =
      std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels, ImageTextureFilter::Mipmap);
    auto mipmappedImageMatte = std::make_shared<MatteMaterial>(mipmappedImage);
    auto mipmappedImageSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    mipmappedImageSphere->setMaterial(mipmappedImageMatte);
    mipmappedImageScene.add(mipmappedImageSphere);
    const GpuDiffusePathLoopBackendSupport mipmappedImageSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(mipmappedImageScene), settings);
    EXPECT_TRUE(mipmappedImageSupport.supported);
    EXPECT_TRUE(mipmappedImageSupport.reason.empty());

    Scene uvColorScene;
    auto uvColorMatte = std::make_shared<MatteMaterial>(std::make_shared<UVColorTexture>());
    auto uvColorSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    uvColorSphere->setMaterial(uvColorMatte);
    uvColorScene.add(uvColorSphere);
    const GpuDiffusePathLoopBackendSupport uvColorSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(uvColorScene), settings);
    EXPECT_TRUE(uvColorSupport.supported);
    EXPECT_TRUE(uvColorSupport.reason.empty());

    Scene meshPrimitiveScene;
    auto meshPrimitive =
      std::make_shared<MeshPrimitive>(triangleMesh(), MeshPrimitive::NormalMode::Smooth);
    meshPrimitive->setMaterial(matte);
    meshPrimitiveScene.add(meshPrimitive);
    const GpuDiffusePathLoopBackendSupport meshPrimitiveSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(meshPrimitiveScene), settings);
    EXPECT_TRUE(meshPrimitiveSupport.supported) << meshPrimitiveSupport.reason;
    EXPECT_TRUE(meshPrimitiveSupport.reason.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsOneDepthAllMissPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend,
       ResolvesDescriptorOnlyPinholePrimaryPathsToDisplayWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    const Colord background(0.25, 0.5, 0.75);
    Scene scene;
    scene.setBackground(background);
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.captureDiagnostics = false;
    settings.captureMetrics = false;
    settings.capturePlatformAccumulation = false;
    settings.captureResolvedDisplay = true;
    settings.displayResolveTonemap = GpuDisplayResolveTonemap::Linear;

    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    EXPECT_TRUE(result.resolvedPathStates.empty());
    EXPECT_TRUE(result.stepRecords.empty());
    EXPECT_FALSE(result.hasPlatformAccumulation());
    ASSERT_TRUE(result.hasPlatformResolvedDisplay());
    ASSERT_EQ(4u, result.platformResolvedDisplayPixels.size());
    for (const unsigned int pixel : result.platformResolvedDisplayPixels) {
      EXPECT_EQ(background.rgb(), pixel);
    }
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, AddsSingleSampleChunksIntoPixelAccumulationWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    const Colord background(0.25, 0.5, 0.75);
    Scene scene;
    scene.setBackground(background);
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 1));
    camera.viewPlane()->sampler()->setup(3, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 1), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.captureDiagnostics = false;
    settings.capturePlatformAccumulation = true;
    settings.primarySampleChunkSize = 1;

    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ(3u, result.roundTrips);
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetPixel, result.platformAccumulationTargetMode);
    ASSERT_EQ(2u, result.platformAccumulationColorSums.size());
    ASSERT_EQ(2u, result.platformAccumulationSampleCounts.size());
    EXPECT_EQ(3u, result.platformAccumulationSampleCounts[0]);
    EXPECT_EQ(3u, result.platformAccumulationSampleCounts[1]);
    EXPECT_NEAR(3.0 * background.r(), result.platformAccumulationColorSums[0][0], 1e-4);
    EXPECT_NEAR(3.0 * background.g(), result.platformAccumulationColorSums[0][1], 1e-4);
    EXPECT_NEAR(3.0 * background.b(), result.platformAccumulationColorSums[0][2], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsDuplicatePixelSamplesWithSampleSlotAccumulation) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 0;
    paths[0].primarySampleIndex = 0;
    paths[0].throughput = {1.0f, 1.0f, 1.0f, 0.0f};
    paths[1].pixelIndex = 0;
    paths[1].primarySampleIndex = 1;
    paths[1].throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
    ASSERT_EQ(paths.size(), result.platformAccumulationColorSums.size());
    ASSERT_EQ(paths.size(), result.platformAccumulationSampleCounts.size());
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetSampleSlot,
              result.platformAccumulationTargetMode);
    EXPECT_EQ(1u, result.platformAccumulationWidth);
    EXPECT_EQ(2u, result.platformAccumulationHeight);
    EXPECT_EQ(1u, result.platformAccumulationSampleCounts[0]);
    EXPECT_EQ(1u, result.platformAccumulationSampleCounts[1]);

    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(1, 1);
    Buffer<Colord> expectedResolved(1, 1);
    Buffer<Colord> resolved(1, 1);
    (void)resolveGpuDiffusePathLoopImage(expected, layout, expectedResolved);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(expectedResolved[0][0], resolved[0][0], 1e-4);
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("metal_accumulation_buffer", diagnostics.residency);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsThinLensDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    ThinLensCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.2);
    camera.setFocalDistance(6.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeThinLens, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTiltShiftDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    TiltShiftCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.2);
    camera.setFocalDistance(6.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend,
       RunsEquirectangularDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    EquirectangularCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeEquirectangular,
              descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);
    ASSERT_EQ(8u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ(8u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsSphericalDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    SphericalCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(200_degrees, 90_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeSpherical, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);
    ASSERT_EQ(8u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ(8u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsFishEyeDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    FishEyeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(1, 1, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeFishEye, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(1, 1, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("metal", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, ReportsUnavailableWhenBuildOrDeviceCannotRunVulkan) {
    const VulkanGpuDiffusePathLoopBackend backend;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const GpuDiffusePathLoopBackendSupport support =
      backend.fullGpuPathLoopSupport(GpuTracingSceneSections(), settings);

#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    if (backend.fullGpuPathLoopAvailable()) {
      EXPECT_TRUE(support.supported);
      EXPECT_TRUE(support.reason.empty());
    } else {
      EXPECT_FALSE(support.supported);
      EXPECT_FALSE(support.reason.empty());
      EXPECT_EQ(VulkanGpuDiffusePathLoopKernel().launchPathUnavailableReason(), support.reason);
      EXPECT_STREQ(support.reason.c_str(), backend.fullGpuPathLoopUnavailableReason());
    }
#else
    EXPECT_FALSE(backend.fullGpuPathLoopAvailable());
    EXPECT_FALSE(support.supported);
    EXPECT_EQ("Vulkan diffuse path-loop backend is not enabled in this build", support.reason);
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, SupportsUntransformedAnalyticGeometryWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;

    Scene emptyScene;
    const GpuTracingSceneSections emptySections = sectionsFor(emptyScene);
    const GpuDiffusePathLoopBackendSupport emptySupport =
      backend.fullGpuPathLoopSupport(emptySections, settings);
    EXPECT_TRUE(emptySupport.supported);
    EXPECT_TRUE(emptySupport.reason.empty());

    Scene sphereScene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(matte);
    sphereScene.add(sphere);
    const GpuTracingSceneSections sphereSections = sectionsFor(sphereScene);
    const GpuDiffusePathLoopBackendSupport sphereSupport =
      backend.fullGpuPathLoopSupport(sphereSections, settings);
    EXPECT_TRUE(sphereSupport.supported);
    EXPECT_TRUE(sphereSupport.reason.empty());

    settings.maxDepth = 2;
    const GpuDiffusePathLoopBackendSupport multiDepthSphereSupport =
      backend.fullGpuPathLoopSupport(sphereSections, settings);
    EXPECT_TRUE(multiDepthSphereSupport.supported);
    EXPECT_TRUE(multiDepthSphereSupport.reason.empty());

    Scene planeScene;
    auto plane = std::make_shared<Plane>(Vector3d(0.0, 0.0, 1.0), 0.0);
    plane->setMaterial(matte);
    planeScene.add(plane);
    const GpuDiffusePathLoopBackendSupport planeSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(planeScene), settings);
    EXPECT_TRUE(planeSupport.supported);
    EXPECT_TRUE(planeSupport.reason.empty());

    Scene rectangleAndDiskScene;
    auto rectangle = std::make_shared<Rectangle>(Vector3d(-3.0, -1.0, 0.0), Vector3d(2.0, 0.0, 0.0),
                                                 Vector3d(0.0, 2.0, 0.0));
    rectangle->setMaterial(matte);
    rectangleAndDiskScene.add(rectangle);
    auto disk = std::make_shared<Disk>(Vector3d(2.0, 0.0, 0.0), Vector3d(0.0, 0.0, 1.0), 0.75);
    disk->setMaterial(matte);
    rectangleAndDiskScene.add(disk);
    const GpuDiffusePathLoopBackendSupport rectangleAndDiskSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(rectangleAndDiskScene), settings);
    EXPECT_TRUE(rectangleAndDiskSupport.supported);
    EXPECT_TRUE(rectangleAndDiskSupport.reason.empty());

    Scene triangleScene;
    auto triangle = std::make_shared<Triangle>(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, -1.0, 0.0),
                                               Vector3d(0.0, 1.0, 0.0));
    triangle->setMaterial(matte);
    triangleScene.add(triangle);
    const GpuDiffusePathLoopBackendSupport triangleSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(triangleScene), settings);
    EXPECT_TRUE(triangleSupport.supported);
    EXPECT_TRUE(triangleSupport.reason.empty());

    Scene meshPrimitiveScene;
    auto meshPrimitive =
      std::make_shared<MeshPrimitive>(triangleMesh(), MeshPrimitive::NormalMode::Smooth);
    meshPrimitive->setMaterial(matte);
    meshPrimitiveScene.add(meshPrimitive);
    const GpuDiffusePathLoopBackendSupport meshPrimitiveSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(meshPrimitiveScene), settings);
    EXPECT_TRUE(meshPrimitiveSupport.supported) << meshPrimitiveSupport.reason;
    EXPECT_TRUE(meshPrimitiveSupport.reason.empty());

    Scene openCylinderScene;
    auto openCylinder = std::make_shared<OpenCylinder>(1.0, 2.0);
    openCylinder->setMaterial(matte);
    openCylinderScene.add(openCylinder);
    const GpuDiffusePathLoopBackendSupport openCylinderSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(openCylinderScene), settings);
    EXPECT_TRUE(openCylinderSupport.supported);
    EXPECT_TRUE(openCylinderSupport.reason.empty());

    Scene torusScene;
    auto torus = std::make_shared<Torus>(1.0, 0.25);
    torus->setMaterial(matte);
    torusScene.add(torus);
    const GpuDiffusePathLoopBackendSupport torusSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(torusScene), settings);
    EXPECT_TRUE(torusSupport.supported);
    EXPECT_TRUE(torusSupport.reason.empty());

    Scene transformedScene;
    auto transformedSphere =
      std::make_shared<Instance>(std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0));
    transformedSphere->setMatrix(Matrix4d::translate(1.0, 0.0, 0.0));
    transformedSphere->setMaterial(matte);
    transformedScene.add(transformedSphere);
    const GpuDiffusePathLoopBackendSupport transformedSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(transformedScene), settings);
    EXPECT_TRUE(transformedSupport.supported);
    EXPECT_TRUE(transformedSupport.reason.empty());

    Scene phongScene;
    auto phong = std::make_shared<PhongMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord::white(), 16.0);
    auto phongSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    phongSphere->setMaterial(phong);
    phongScene.add(phongSphere);
    const GpuDiffusePathLoopBackendSupport phongSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(phongScene), settings);
    EXPECT_TRUE(phongSupport.supported);
    EXPECT_TRUE(phongSupport.reason.empty());

    Scene reflectiveScene;
    auto reflective =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::black()));
    reflective->setDiffuseCoefficient(0.0);
    reflective->setReflectionColor(Colord(0.75, 0.5, 0.25));
    reflective->setReflectionCoefficient(0.5);
    auto reflectiveSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    reflectiveSphere->setMaterial(reflective);
    reflectiveScene.add(reflectiveSphere);
    const GpuDiffusePathLoopBackendSupport reflectiveSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(reflectiveScene), settings);
    EXPECT_TRUE(reflectiveSupport.supported);
    EXPECT_TRUE(reflectiveSupport.reason.empty());

    Scene transparentScene;
    auto transparent = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::black()));
    transparent->setDiffuseCoefficient(0.0);
    transparent->setSpecularCoefficient(0.0);
    transparent->setReflectionCoefficient(0.0);
    transparent->setTransmissionCoefficient(1.0);
    transparent->setRefractionIndex(1.5);
    auto transparentSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    transparentSphere->setMaterial(transparent);
    transparentScene.add(transparentSphere);
    const GpuDiffusePathLoopBackendSupport transparentSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(transparentScene), settings);
    EXPECT_TRUE(transparentSupport.supported);
    EXPECT_TRUE(transparentSupport.reason.empty());

    Scene portalScene;
    auto portal =
      std::make_shared<PortalMaterial>(Matrix4d::translate(0.0, 0.0, 2.0), Colord(0.25, 0.5, 0.75));
    auto portalSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    portalSphere->setMaterial(portal);
    portalScene.add(portalSphere);
    const GpuDiffusePathLoopBackendSupport portalSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(portalScene), settings);
    EXPECT_TRUE(portalSupport.supported);
    EXPECT_TRUE(portalSupport.reason.empty());

    Scene checkerScene;
    auto checker = std::make_shared<CheckerBoardTexture>(
      new PlanarMapping2D, std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto checkerMatte = std::make_shared<MatteMaterial>(checker);
    auto checkerSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    checkerSphere->setMaterial(checkerMatte);
    checkerScene.add(checkerSphere);
    const GpuTracingSceneSections checkerSections = sectionsFor(checkerScene);
    const GpuDiffusePathLoopBackendSupport checkerSupport =
      backend.fullGpuPathLoopSupport(checkerSections, settings);
    EXPECT_TRUE(checkerSupport.supported);
    EXPECT_TRUE(checkerSupport.reason.empty());

    Scene imageScene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                ImageTextureFilter::Nearest);
    auto imageMatte = std::make_shared<MatteMaterial>(image);
    auto imageSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    imageSphere->setMaterial(imageMatte);
    imageScene.add(imageSphere);
    const GpuTracingSceneSections imageSections = sectionsFor(imageScene);
    const GpuDiffusePathLoopBackendSupport imageSupport =
      backend.fullGpuPathLoopSupport(imageSections, settings);
    EXPECT_TRUE(imageSupport.supported);
    EXPECT_TRUE(imageSupport.reason.empty());

    Scene bilinearImageScene;
    auto bilinearImage = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                        ImageTextureFilter::Bilinear);
    auto bilinearImageMatte = std::make_shared<MatteMaterial>(bilinearImage);
    auto bilinearImageSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    bilinearImageSphere->setMaterial(bilinearImageMatte);
    bilinearImageScene.add(bilinearImageSphere);
    const GpuDiffusePathLoopBackendSupport bilinearImageSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(bilinearImageScene), settings);
    EXPECT_TRUE(bilinearImageSupport.supported);
    EXPECT_TRUE(bilinearImageSupport.reason.empty());

    Scene mipmappedImageScene;
    auto mipmappedImage =
      std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels, ImageTextureFilter::Mipmap);
    auto mipmappedImageMatte = std::make_shared<MatteMaterial>(mipmappedImage);
    auto mipmappedImageSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    mipmappedImageSphere->setMaterial(mipmappedImageMatte);
    mipmappedImageScene.add(mipmappedImageSphere);
    const GpuDiffusePathLoopBackendSupport mipmappedImageSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(mipmappedImageScene), settings);
    EXPECT_TRUE(mipmappedImageSupport.supported);
    EXPECT_TRUE(mipmappedImageSupport.reason.empty());

    Scene uvColorScene;
    auto uvColorMatte = std::make_shared<MatteMaterial>(std::make_shared<UVColorTexture>());
    auto uvColorSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    uvColorSphere->setMaterial(uvColorMatte);
    uvColorScene.add(uvColorSphere);
    const GpuDiffusePathLoopBackendSupport uvColorSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(uvColorScene), settings);
    EXPECT_TRUE(uvColorSupport.supported);
    EXPECT_TRUE(uvColorSupport.reason.empty());

    Scene tintedScene;
    auto tinted = std::make_shared<TintedTexture>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.5, 0.25, 0.125));
    auto tintedMatte = std::make_shared<MatteMaterial>(tinted);
    auto tintedSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    tintedSphere->setMaterial(tintedMatte);
    tintedScene.add(tintedSphere);
    const GpuDiffusePathLoopBackendSupport tintedSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(tintedScene), settings);
    EXPECT_TRUE(tintedSupport.supported);
    EXPECT_TRUE(tintedSupport.reason.empty());

    Scene nestedTintedScene;
    auto nestedTintedMatte = std::make_shared<MatteMaterial>(nestedTintedConstantTexture());
    auto nestedTintedSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    nestedTintedSphere->setMaterial(nestedTintedMatte);
    nestedTintedScene.add(nestedTintedSphere);
    const GpuDiffusePathLoopBackendSupport nestedTintedSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(nestedTintedScene), settings);
    EXPECT_TRUE(nestedTintedSupport.supported);
    EXPECT_TRUE(nestedTintedSupport.reason.empty());

    Scene checkerGraphScene;
    auto checkerGraphMatte =
      std::make_shared<MatteMaterial>(checkerTextureGraph(new PlanarMapping2D));
    auto checkerGraphSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    checkerGraphSphere->setMaterial(checkerGraphMatte);
    checkerGraphScene.add(checkerGraphSphere);
    const GpuDiffusePathLoopBackendSupport checkerGraphSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(checkerGraphScene), settings);
    EXPECT_TRUE(checkerGraphSupport.supported);
    EXPECT_TRUE(checkerGraphSupport.reason.empty());

    Scene tintedImageScene;
    auto tintedImage = std::make_shared<TintedTexture>(image, Colord(0.5, 0.25, 0.125));
    auto tintedImageMatte = std::make_shared<MatteMaterial>(tintedImage);
    auto tintedImageSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    tintedImageSphere->setMaterial(tintedImageMatte);
    tintedImageScene.add(tintedImageSphere);
    const GpuDiffusePathLoopBackendSupport tintedImageSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(tintedImageScene), settings);
    EXPECT_TRUE(tintedImageSupport.supported);
    EXPECT_TRUE(tintedImageSupport.reason.empty());

    Scene directionalLightScene;
    auto directionalLightSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    directionalLightSphere->setMaterial(matte);
    directionalLightScene.add(directionalLightSphere);
    directionalLightScene.addLight(
      std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
    const GpuDiffusePathLoopBackendSupport directionalLightSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(directionalLightScene), settings);
    EXPECT_TRUE(directionalLightSupport.supported);
    EXPECT_TRUE(directionalLightSupport.reason.empty());

    Scene areaLightScene;
    auto areaLightSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    areaLightSphere->setMaterial(matte);
    areaLightScene.add(areaLightSphere);
    areaLightScene.addLight(std::make_shared<RectangularAreaLight>(
      Vector3d(0.0, 2.0, -3.0), Vector3d(2.0, 0.0, 0.0), Vector3d(0.0, 2.0, 0.0), Colord::white()));
    const GpuDiffusePathLoopBackendSupport areaLightSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(areaLightScene), settings);
    EXPECT_TRUE(areaLightSupport.supported);
    EXPECT_TRUE(areaLightSupport.reason.empty());

    Scene curveScene;
    curveScene.add(std::make_shared<Curve>(
      core::Polyline({Vector3d(0.0, 0.0, 0.0), Vector3d(1.0, 0.0, 0.0)}), 0.1));
    const GpuDiffusePathLoopBackendSupport curveSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(curveScene), settings);
    EXPECT_TRUE(curveSupport.supported);
    EXPECT_TRUE(curveSupport.reason.empty());

    Scene unsupportedScene;
    unsupportedScene.add(std::make_shared<UnsupportedGpuTracingPrimitive>());
    const GpuTracingSceneSections unsupportedSections = sectionsFor(unsupportedScene);
    const GpuDiffusePathLoopBackendSupport unsupportedSupport =
      backend.fullGpuPathLoopSupport(unsupportedSections, settings);
    EXPECT_FALSE(unsupportedSupport.supported);
    EXPECT_EQ("Vulkan diffuse path-loop backend currently supports empty geometry or "
              "triangle-backed MeshPrimitive, Box, and finite-width Curve geometry plus "
              "triangle, sphere, plane, rectangle, disk, open-cylinder, or torus records with "
              "static transforms only",
              unsupportedSupport.reason);

    Scene unsupportedMaterialScene;
    auto unsupportedMaterialSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedMaterialSphere->setMaterial(std::make_shared<UnsupportedGpuTracingMaterial>());
    unsupportedMaterialScene.add(unsupportedMaterialSphere);
    const GpuTracingSceneSections unsupportedMaterialSections =
      sectionsFor(unsupportedMaterialScene);
    const GpuDiffusePathLoopBackendSupport unsupportedMaterialSupport =
      backend.fullGpuPathLoopSupport(unsupportedMaterialSections, settings);
    EXPECT_FALSE(unsupportedMaterialSupport.supported);
    EXPECT_EQ("Vulkan diffuse path-loop backend currently supports Matte, Phong finite glossy, "
              "Reflective mirror, Transparent refraction, Emissive, and Portal materials only",
              unsupportedMaterialSupport.reason);

    Scene unsupportedTextureScene;
    auto unsupportedTextureMatte =
      std::make_shared<MatteMaterial>(std::make_shared<UnsupportedGpuTracingTexture>());
    auto unsupportedTextureSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    unsupportedTextureSphere->setMaterial(unsupportedTextureMatte);
    unsupportedTextureScene.add(unsupportedTextureSphere);
    const GpuTracingSceneSections unsupportedTextureSections = sectionsFor(unsupportedTextureScene);
    const GpuDiffusePathLoopBackendSupport unsupportedTextureSupport =
      backend.fullGpuPathLoopSupport(unsupportedTextureSections, settings);
    EXPECT_FALSE(unsupportedTextureSupport.supported);
    EXPECT_EQ("Vulkan diffuse path-loop backend currently supports ConstantColor, CheckerBoard "
              "texture graphs, nearest, bilinear, and base-level mipmapped ImageTexture, "
              "UVColorTexture, and bounded Tinted wrapper chains over those textures only",
              unsupportedTextureSupport.reason);

    Scene multiLightScene;
    auto multiLightSphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    multiLightSphere->setMaterial(matte);
    multiLightScene.add(multiLightSphere);
    multiLightScene.addLight(
      std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord::white()));
    multiLightScene.addLight(
      std::make_shared<DirectionalLight>(Vector3d(0.0, 0.0, -1.0), Colord::white()));
    multiLightScene.addLight(std::make_shared<RectangularAreaLight>(
      Vector3d(0.0, 2.0, -3.0), Vector3d(2.0, 0.0, 0.0), Vector3d(0.0, 2.0, 0.0), Colord::white()));
    const GpuDiffusePathLoopBackendSupport multiLightSupport =
      backend.fullGpuPathLoopSupport(sectionsFor(multiLightScene), settings);
    EXPECT_TRUE(multiLightSupport.supported);
    EXPECT_TRUE(multiLightSupport.reason.empty());
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsOneDepthAllMissPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ("vulkan_host_visible_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend,
       ResolvesDescriptorOnlyPinholePrimaryPathsToDisplayWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    const Colord background(0.25, 0.5, 0.75);
    Scene scene;
    scene.setBackground(background);
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.captureDiagnostics = false;
    settings.captureMetrics = false;
    settings.capturePlatformAccumulation = false;
    settings.captureResolvedDisplay = true;
    settings.displayResolveTonemap = GpuDisplayResolveTonemap::Linear;

    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    EXPECT_TRUE(result.resolvedPathStates.empty());
    EXPECT_TRUE(result.stepRecords.empty());
    EXPECT_FALSE(result.hasPlatformAccumulation());
    ASSERT_TRUE(result.hasPlatformResolvedDisplay());
    ASSERT_EQ(4u, result.platformResolvedDisplayPixels.size());
    for (const unsigned int pixel : result.platformResolvedDisplayPixels) {
      EXPECT_EQ(background.rgb(), pixel);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, AddsSingleSampleChunksIntoPixelAccumulationWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    const Colord background(0.25, 0.5, 0.75);
    Scene scene;
    scene.setBackground(background);
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 1));
    camera.viewPlane()->sampler()->setup(3, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 1), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.captureDiagnostics = false;
    settings.capturePlatformAccumulation = true;
    settings.primarySampleChunkSize = 1;

    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(3u, result.roundTrips);
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetPixel, result.platformAccumulationTargetMode);
    ASSERT_EQ(2u, result.platformAccumulationColorSums.size());
    ASSERT_EQ(2u, result.platformAccumulationSampleCounts.size());
    EXPECT_EQ(3u, result.platformAccumulationSampleCounts[0]);
    EXPECT_EQ(3u, result.platformAccumulationSampleCounts[1]);
    EXPECT_NEAR(3.0 * background.r(), result.platformAccumulationColorSums[0][0], 1e-4);
    EXPECT_NEAR(3.0 * background.g(), result.platformAccumulationColorSums[0][1], 1e-4);
    EXPECT_NEAR(3.0 * background.b(), result.platformAccumulationColorSums[0][2], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsThinLensDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    ThinLensCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.2);
    camera.setFocalDistance(6.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeThinLens, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTiltShiftDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    TiltShiftCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.2);
    camera.setFocalDistance(6.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend,
       RunsEquirectangularDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    EquirectangularCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeEquirectangular,
              descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);
    ASSERT_EQ(8u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(8u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsSphericalDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    SphericalCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(200_degrees, 90_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeSpherical, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234);
    ASSERT_EQ(8u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(8u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsFishEyeDescriptorOnlyPrimaryPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    FishEyeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(1, 1, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());
    ASSERT_TRUE(descriptorOnly.primaryPathDescriptor.has_value());
    EXPECT_EQ(gpuPrimaryPathGenerationModeFishEye, descriptorOnly.primaryPathDescriptor->mode);

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(1, 1, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, descriptorOnly, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ(4u, result.initialPathCount);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    for (std::size_t index = 0; index != expected.resolvedPathStates.size(); ++index) {
      expectPathStateNear(result.resolvedPathStates[index], expected.resolvedPathStates[index],
                          1e-4);
    }
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsDuplicatePixelSamplesWithSampleSlotAccumulation) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.2, 0.4, 0.6));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 0;
    paths[0].primarySampleIndex = 0;
    paths[0].throughput = {1.0f, 1.0f, 1.0f, 0.0f};
    paths[1].pixelIndex = 0;
    paths[1].primarySampleIndex = 1;
    paths[1].throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
    ASSERT_EQ(paths.size(), result.platformAccumulationColorSums.size());
    ASSERT_EQ(paths.size(), result.platformAccumulationSampleCounts.size());
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetSampleSlot,
              result.platformAccumulationTargetMode);
    EXPECT_EQ(1u, result.platformAccumulationWidth);
    EXPECT_EQ(2u, result.platformAccumulationHeight);
    EXPECT_EQ(1u, result.platformAccumulationSampleCounts[0]);
    EXPECT_EQ(1u, result.platformAccumulationSampleCounts[1]);

    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(1, 1);
    Buffer<Colord> expectedResolved(1, 1);
    Buffer<Colord> resolved(1, 1);
    (void)resolveGpuDiffusePathLoopImage(expected, layout, expectedResolved);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(expectedResolved[0][0], resolved[0][0], 1e-4);
    EXPECT_EQ("vulkan_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("vulkan_accumulation_buffer", diagnostics.residency);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsOneDepthSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ("vulkan", result.platformName);
    EXPECT_EQ("vulkan_host_visible_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsOneDepthPlaneDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsRectangleAndDiskDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    material->setDiffuseCoefficient(0.9);
    auto rectangle = std::make_shared<Rectangle>(Vector3d(-3.0, -1.0, 0.0), Vector3d(2.0, 0.0, 0.0),
                                                 Vector3d(0.0, 2.0, 0.0));
    rectangle->setMaterial(material);
    scene.add(rectangle);
    auto disk = std::make_shared<Disk>(Vector3d(2.0, 0.0, 0.0), Vector3d(0.0, 0.0, 1.0), 0.75);
    disk->setMaterial(material);
    scene.add(disk);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, 3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord rectanglePath =
      activePath(Rayd(Vector4d(-2.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    rectanglePath.pixelIndex = 0;
    GpuDiffusePathStateRecord diskPath =
      activePath(Rayd(Vector4d(2.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 18);
    diskPath.pixelIndex = 1;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{rectanglePath, diskPath};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, CapturesDenoiserFeaturesWhenRequested) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.2, 0.3, 0.4)));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    Scene scene;
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.primarySampleIndex = 0;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.captureDenoiserFeatures = true;

    const GpuDiffusePathLoopResult result = backend.run(sections, {path}, settings);

    EXPECT_TRUE(result.denoiserFeatureRecordsCaptured);
    ASSERT_EQ(1u, result.denoiserFeatureRecords.size());
    const GpuDiffusePathDenoiserFeatureRecord& feature = result.denoiserFeatureRecords[0];
    EXPECT_NE(0u, feature.flags & gpuDiffusePathDenoiserFeatureValidFlag);
    EXPECT_EQ(0u, feature.pixelIndex);
    EXPECT_EQ(0u, feature.primarySampleIndex);
    ASSERT_COLOR_NEAR(Colord(0.2, 0.3, 0.4), Colord(feature.albedo), 1e-5);
    EXPECT_GT(feature.depth, 0.0f);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTriangleDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    material->setDiffuseCoefficient(0.9);
    auto receiver = std::make_shared<Triangle>(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, -1.0, 0.0),
                                               Vector3d(0.0, 1.0, 0.0));
    receiver->setMaterial(material);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, 3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsOpenCylinderDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    material->setDiffuseCoefficient(0.8);
    auto cylinder = std::make_shared<OpenCylinder>(1.0, 2.0);
    cylinder->setMaterial(material);
    scene.add(cylinder);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 19);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTorusDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.7, 0.35, 0.2)));
    material->setDiffuseCoefficient(0.85);
    auto torus = std::make_shared<Torus>(1.0, 0.25);
    torus->setMaterial(material);
    scene.add(torus);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 20);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTransformedSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    material->setDiffuseCoefficient(0.8);
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(material);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setMatrix(Matrix4d::translate(0.0, 0.0, 2.0) * Matrix4d(Matrix3d::scale(1.5)));
    scene.add(instance);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 3.0, -2.0), Colord::white()));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 21);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTriangleBackedGeometryPathLoopsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendPathLoopMatchesReference(backend, meshPrimitiveGpuPathLoopCase());
    expectBackendPathLoopMatchesReference(backend, boxGpuPathLoopCase());
    expectBackendPathLoopMatchesReference(backend, curveGpuPathLoopCase());
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsDirectionalLightPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendDirectLightPathLoopMatchesReference(backend, directionalLightGpuPathLoopCase());
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsRectangularAreaLightPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendDirectLightPathLoopMatchesReference(backend,
                                                     rectangularAreaLightGpuPathLoopCase());
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsMultipleLightPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendDirectLightPathLoopMatchesReference(backend, multipleLightGpuPathLoopCase());
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsPhongGlossyPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto phong = std::make_shared<PhongMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.75, 0.5, 0.25),
      16.0);
    phong->setDiffuseCoefficient(0.8);
    phong->setSpecularCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(phong);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].directLightRadiance,
                     expected.stepRecords[0].directLightRadiance, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsReflectiveMirrorPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto reflective =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::black()));
    reflective->setDiffuseCoefficient(0.0);
    reflective->setReflectionColor(Colord(0.75, 0.5, 0.25));
    reflective->setReflectionCoefficient(0.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(reflective);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTransparentTransmissionPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto transparent = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::black()));
    transparent->setDiffuseCoefficient(0.0);
    transparent->setSpecularCoefficient(0.0);
    transparent->setReflectionCoefficient(0.0);
    transparent->setTransmissionCoefficient(1.0);
    transparent->setRefractionIndex(1.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(transparent);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsPortalPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto portal =
      std::make_shared<PortalMaterial>(Matrix4d::translate(0.0, 0.0, 2.0), Colord(0.75, 0.5, 0.25));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(portal);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsMultiDepthSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsPlanarCheckerTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto checker = std::make_shared<CheckerBoardTexture>(
      new PlanarMapping2D, std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto matte = std::make_shared<MatteMaterial>(checker);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsCheckerTextureGraphPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(checkerTextureGraph(new PlanarMapping2D));
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord brightPath =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    brightPath.pixelIndex = 0;
    GpuDiffusePathStateRecord darkPath =
      activePath(Rayd(Vector4d(1.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 18);
    darkPath.pixelIndex = 1;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{brightPath, darkPath};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    expectFloat4Near(result.stepRecords[1].continuationThroughput,
                     expected.stepRecords[1].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsNearestImageTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                ImageTextureFilter::Nearest);
    auto matte = std::make_shared<MatteMaterial>(image);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsBilinearImageTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                ImageTextureFilter::Bilinear);
    auto matte = std::make_shared<MatteMaterial>(image);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsMipmappedImageTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image =
      std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels, ImageTextureFilter::Mipmap);
    auto matte = std::make_shared<MatteMaterial>(image);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsUvColorTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(std::make_shared<UVColorTexture>());
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTintedTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto tinted = std::make_shared<TintedTexture>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.5, 0.25, 0.125));
    auto matte = std::make_shared<MatteMaterial>(tinted);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsNestedTintedTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(nestedTintedConstantTexture());
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(VulkanGpuDiffusePathLoopBackend, RunsTintedImageTexturePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_VULKAN_WAVEFRONT)
    const VulkanGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                ImageTextureFilter::Nearest);
    auto tinted = std::make_shared<TintedTexture>(image, Colord(0.5, 0.25, 0.125));
    auto matte = std::make_shared<MatteMaterial>(tinted);
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Vulkan wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsMultiDepthSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_GT(result.frontierCompactionUploadWorkerSeconds, 0.0);
    EXPECT_GT(result.frontierCompactionKernelWorkerSeconds, 0.0);
    EXPECT_GT(result.frontierCompactionReadbackWorkerSeconds, 0.0);
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsOneDepthSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, CapturesDenoiserFeaturesWhenRequested) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.2, 0.3, 0.4)));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    Scene scene;
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.primarySampleIndex = 0;
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.captureDenoiserFeatures = true;

    const GpuDiffusePathLoopResult result = backend.run(sections, {path}, settings);

    EXPECT_TRUE(result.denoiserFeatureRecordsCaptured);
    ASSERT_EQ(1u, result.denoiserFeatureRecords.size());
    const GpuDiffusePathDenoiserFeatureRecord& feature = result.denoiserFeatureRecords[0];
    EXPECT_NE(0u, feature.flags & gpuDiffusePathDenoiserFeatureValidFlag);
    EXPECT_EQ(0u, feature.pixelIndex);
    EXPECT_EQ(0u, feature.primarySampleIndex);
    ASSERT_COLOR_NEAR(Colord(0.2, 0.3, 0.4), Colord(feature.albedo), 1e-5);
    EXPECT_GT(feature.depth, 0.0f);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsPhongGlossyPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto phong = std::make_shared<PhongMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.75, 0.5, 0.25),
      16.0);
    phong->setDiffuseCoefficient(0.8);
    phong->setSpecularCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(phong);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsReflectiveMirrorPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto reflective =
      std::make_shared<ReflectiveMaterial>(std::make_shared<ConstantColorTexture>(Colord::black()));
    reflective->setDiffuseCoefficient(0.0);
    reflective->setReflectionColor(Colord(0.75, 0.5, 0.25));
    reflective->setReflectionCoefficient(0.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(reflective);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTransparentTransmissionPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto transparent = std::make_shared<TransparentMaterial>(
      std::make_shared<ConstantColorTexture>(Colord::black()));
    transparent->setDiffuseCoefficient(0.0);
    transparent->setSpecularCoefficient(0.0);
    transparent->setReflectionCoefficient(0.0);
    transparent->setTransmissionCoefficient(1.0);
    transparent->setRefractionIndex(1.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(transparent);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsPortalPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto portal =
      std::make_shared<PortalMaterial>(Matrix4d::translate(0.0, 0.0, 2.0), Colord(0.75, 0.5, 0.25));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(portal);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_EQ(gpuDiffusePathStateSampledFromBsdfFlag | gpuDiffusePathStateBsdfSampleDeltaFlag,
              result.resolvedPathStates[0].previousEventFlags);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsSphereAndPlaneDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto floorMaterial = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    floorMaterial->setDiffuseCoefficient(0.9);
    auto sphereMaterial = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    sphereMaterial->setDiffuseCoefficient(0.8);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(floorMaterial);
    scene.add(floor);
    auto receiver = std::make_shared<Sphere>(Vector3d(3.0, 0.0, 0.0), 0.5);
    receiver->setMaterial(sphereMaterial);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsDirectionalLightPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendDirectLightPathLoopMatchesReference(backend, directionalLightGpuPathLoopCase());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsRectangularAreaLightPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendDirectLightPathLoopMatchesReference(backend,
                                                     rectangularAreaLightGpuPathLoopCase());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsMultipleLightPathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendDirectLightPathLoopMatchesReference(backend, multipleLightGpuPathLoopCase());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsPlanarCheckerDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto checker = std::make_shared<CheckerBoardTexture>(
      new PlanarMapping2D, std::make_shared<ConstantColorTexture>(Colord::red()),
      std::make_shared<ConstantColorTexture>(Colord::blue()));
    auto material = std::make_shared<MatteMaterial>(checker);
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord brightPath =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    brightPath.pixelIndex = 0;
    GpuDiffusePathStateRecord darkPath =
      activePath(Rayd(Vector4d(1.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 18);
    darkPath.pixelIndex = 1;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{brightPath, darkPath};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    expectFloat4Near(result.stepRecords[1].continuationThroughput,
                     expected.stepRecords[1].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsCheckerTextureGraphDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(checkerTextureGraph(new PlanarMapping2D));
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord brightPath =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    brightPath.pixelIndex = 0;
    GpuDiffusePathStateRecord darkPath =
      activePath(Rayd(Vector4d(1.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 18);
    darkPath.pixelIndex = 1;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{brightPath, darkPath};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    expectFloat4Near(result.stepRecords[1].continuationThroughput,
                     expected.stepRecords[1].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTintedTextureDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto tinted = std::make_shared<TintedTexture>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)), Colord(0.5, 0.25, 0.125));
    auto material = std::make_shared<MatteMaterial>(tinted);
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsNestedTintedTextureDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(nestedTintedConstantTexture());
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTintedImageTextureDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                ImageTextureFilter::Nearest);
    auto tinted = std::make_shared<TintedTexture>(image, Colord(0.5, 0.25, 0.125));
    auto material = std::make_shared<MatteMaterial>(tinted);
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsBilinearImageTextureDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image = std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels,
                                                ImageTextureFilter::Bilinear);
    auto material = std::make_shared<MatteMaterial>(image);
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsMipmappedImageTextureDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    std::vector<Colord> pixels{Colord::red(), Colord::green(), Colord::blue(), Colord::white()};
    auto image =
      std::make_shared<ImageTexture>(new PlanarMapping2D, 2, 2, pixels, ImageTextureFilter::Mipmap);
    auto material = std::make_shared<MatteMaterial>(image);
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsUvColorTextureDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(std::make_shared<UVColorTexture>());
    material->setDiffuseCoefficient(1.0);
    auto floor = std::make_shared<Plane>(Vector3d(0.0, 0.0, -1.0), 0.0);
    floor->setMaterial(material);
    scene.add(floor);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.25, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTriangleDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    material->setDiffuseCoefficient(0.9);
    auto receiver = std::make_shared<Triangle>(Vector3d(-1.0, -1.0, 0.0), Vector3d(1.0, -1.0, 0.0),
                                               Vector3d(0.0, 1.0, 0.0));
    receiver->setMaterial(material);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, 3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsRectangleAndDiskDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.6, 0.5, 0.4)));
    material->setDiffuseCoefficient(0.9);
    auto rectangle = std::make_shared<Rectangle>(Vector3d(-3.0, -1.0, 0.0), Vector3d(2.0, 0.0, 0.0),
                                                 Vector3d(0.0, 2.0, 0.0));
    rectangle->setMaterial(material);
    scene.add(rectangle);
    auto disk = std::make_shared<Disk>(Vector3d(2.0, 0.0, 0.0), Vector3d(0.0, 0.0, 1.0), 0.75);
    disk->setMaterial(material);
    scene.add(disk);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, 3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord rectanglePath =
      activePath(Rayd(Vector4d(-2.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 17);
    rectanglePath.pixelIndex = 0;
    GpuDiffusePathStateRecord diskPath =
      activePath(Rayd(Vector4d(2.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 18);
    diskPath.pixelIndex = 1;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{rectanglePath, diskPath};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
    expectPathStateNear(result.resolvedPathStates[1], expected.resolvedPathStates[1], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsOpenCylinderDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    material->setDiffuseCoefficient(0.8);
    auto cylinder = std::make_shared<OpenCylinder>(1.0, 2.0);
    cylinder->setMaterial(material);
    scene.add(cylinder);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 19);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTorusDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.7, 0.35, 0.2)));
    material->setDiffuseCoefficient(0.85);
    auto torus = std::make_shared<Torus>(1.0, 0.25);
    torus->setMaterial(material);
    scene.add(torus);
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path =
      activePath(Rayd(Vector4d(0.0, 0.0, -3.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 20);
    path.pixelIndex = 0;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-4);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTransformedSphereDiffusePathLoopWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto material = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    material->setDiffuseCoefficient(0.8);
    auto sphere = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    sphere->setMaterial(material);
    auto instance = std::make_shared<Instance>(sphere);
    instance->setMatrix(Matrix4d::translate(0.0, 0.0, 3.0));
    scene.add(instance);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathStateRecord path = activePath();
    path.pixelIndex = 0;
    path.sampleSeed = 12347;
    path.throughput = {0.5f, 0.25f, 0.125f, 0.0f};

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{path};

    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);
    const GpuDiffusePathLoopResult result = backend.run(sections, paths, settings);

    EXPECT_TRUE(result.fullGpuPathLoopSupported());
    EXPECT_EQ(expected.depthCount, result.depthCount);
    EXPECT_EQ(expected.maxDepthTerminatedPaths, result.maxDepthTerminatedPaths);
    ASSERT_EQ(expected.resolvedPathStates.size(), result.resolvedPathStates.size());
    expectPathStateNear(result.resolvedPathStates[0], expected.resolvedPathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopBackend, RunsTriangleBackedGeometryPathLoopsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    const MetalGpuDiffusePathLoopBackend backend;
    if (!backend.fullGpuPathLoopAvailable()) {
      GTEST_SKIP() << backend.fullGpuPathLoopUnavailableReason();
    }

    expectBackendPathLoopMatchesReference(backend, meshPrimitiveGpuPathLoopCase());
    expectBackendPathLoopMatchesReference(backend, boxGpuPathLoopCase());
    expectBackendPathLoopMatchesReference(backend, curveGpuPathLoopCase());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, BuildsShaderFacingLaunchPlan) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    settings.russianRouletteDepth = 2;
    settings.directLightSamples = 4;
    settings.displayResolveTonemap = GpuDisplayResolveTonemap::Reinhard;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);
    const std::vector<GpuDiffusePathStateRecord> paths{activePath(), activePath()};

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const auto layouts = sections.sectionLayouts();

    EXPECT_EQ(gpuDiffusePathLoopLaunchLayoutVersion, plan.parameters.layoutVersion);
    EXPECT_EQ(3u, plan.parameters.maxDepth);
    EXPECT_EQ(2u, plan.parameters.russianRouletteDepth);
    EXPECT_EQ(4u, plan.parameters.directLightSamples);
    EXPECT_EQ(1u, plan.parameters.captureDiagnostics);
    EXPECT_EQ(1u, plan.parameters.captureMetrics);
    EXPECT_EQ(0u, plan.parameters.captureDenoiserFeatures);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDisplayResolveTonemap::Reinhard),
              plan.parameters.displayResolveTonemap);
    EXPECT_EQ(2u, plan.parameters.initialPathCount);
    EXPECT_EQ(gpuPrimaryPathGenerationModeHostPathStates,
              plan.parameters.primaryPathGenerationMode);
    EXPECT_FALSE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(3u, plan.parameters.imageWidth);
    EXPECT_EQ(2u, plan.parameters.imageHeight);
    EXPECT_EQ(gpuDiffusePathLoopAccumulationTargetPixel, plan.parameters.accumulationTargetMode);
    EXPECT_EQ(sections.materials.size(), plan.parameters.materialCount);
    EXPECT_EQ(sections.textures.size(), plan.parameters.textureCount);
    EXPECT_EQ(sections.lights.size(), plan.parameters.lightCount);
    EXPECT_EQ(sections.environment.size(), plan.parameters.environmentCount);
    EXPECT_EQ(sections.debugIds.size(), plan.parameters.debugIdCount);
    EXPECT_EQ(layouts[0].byteOffset, plan.parameters.geometryByteOffset);
    EXPECT_EQ(layouts[1].byteOffset, plan.parameters.materialByteOffset);
    EXPECT_EQ(layouts[2].byteOffset, plan.parameters.textureByteOffset);
    EXPECT_EQ(layouts[3].byteOffset, plan.parameters.lightByteOffset);
    EXPECT_EQ(layouts[4].byteOffset, plan.parameters.environmentByteOffset);
    EXPECT_EQ(layouts[5].byteOffset, plan.parameters.debugIdByteOffset);
    EXPECT_EQ(sections.uploadByteCount(), plan.parameters.sceneUploadBytes);
    EXPECT_EQ(sections.geometry.bvh.size(), plan.parameters.bvhNodeCount);
    EXPECT_EQ(sections.geometry.primitives.size(), plan.parameters.primitiveCount);
    EXPECT_EQ(sections.geometry.triangles.size(), plan.parameters.triangleCount);
    EXPECT_EQ(sections.geometry.spheres.size(), plan.parameters.sphereCount);
    EXPECT_EQ(sections.geometry.planes.size(), plan.parameters.planeCount);
    EXPECT_EQ(sections.geometry.rectangles.size(), plan.parameters.rectangleCount);
    EXPECT_EQ(sections.geometry.disks.size(), plan.parameters.diskCount);
    EXPECT_EQ(sections.geometry.openCylinders.size(), plan.parameters.openCylinderCount);
    EXPECT_EQ(sections.geometry.tori.size(), plan.parameters.torusCount);
    EXPECT_EQ(sections.geometry.transforms.size(), plan.parameters.transformCount);

    std::uint32_t geometryOffset = layouts[0].byteOffset;
    const auto expectGeometryRange = [&geometryOffset](std::uint32_t actualOffset,
                                                       std::size_t count, std::size_t recordSize) {
      EXPECT_EQ(geometryOffset, actualOffset);
      geometryOffset += static_cast<std::uint32_t>(count * recordSize);
    };
    expectGeometryRange(plan.parameters.bvhByteOffset, sections.geometry.bvh.size(),
                        sizeof(GpuIntersectionBvhNode));
    expectGeometryRange(plan.parameters.primitiveByteOffset, sections.geometry.primitives.size(),
                        sizeof(GpuIntersectionPrimitiveRecord));
    expectGeometryRange(plan.parameters.triangleByteOffset, sections.geometry.triangles.size(),
                        sizeof(GpuIntersectionTrianglePayload));
    expectGeometryRange(plan.parameters.sphereByteOffset, sections.geometry.spheres.size(),
                        sizeof(GpuIntersectionSpherePayload));
    expectGeometryRange(plan.parameters.planeByteOffset, sections.geometry.planes.size(),
                        sizeof(GpuIntersectionPlanePayload));
    expectGeometryRange(plan.parameters.rectangleByteOffset, sections.geometry.rectangles.size(),
                        sizeof(GpuIntersectionRectanglePayload));
    expectGeometryRange(plan.parameters.diskByteOffset, sections.geometry.disks.size(),
                        sizeof(GpuIntersectionDiskPayload));
    expectGeometryRange(plan.parameters.openCylinderByteOffset,
                        sections.geometry.openCylinders.size(),
                        sizeof(GpuIntersectionOpenCylinderPayload));
    expectGeometryRange(plan.parameters.torusByteOffset, sections.geometry.tori.size(),
                        sizeof(GpuIntersectionTorusPayload));
    expectGeometryRange(plan.parameters.transformByteOffset, sections.geometry.transforms.size(),
                        sizeof(GpuIntersectionTransformPayload));
    EXPECT_EQ(layouts[0].byteOffset + layouts[0].byteCount, geometryOffset);

    EXPECT_EQ(sections.uploadByteCount(), plan.buffers.sceneUploadBytes);
    EXPECT_EQ(sections.uploadBytes(), plan.sceneUpload);
    EXPECT_EQ(2u * sizeof(GpuDiffusePathStateRecord), plan.buffers.initialPathStateBytes);
    EXPECT_EQ(2u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(2u * sizeof(GpuDiffusePathStateRecord), plan.buffers.nextPathStateBytes);
    EXPECT_EQ(2u * 3u * sizeof(GpuDiffusePathStepRecord), plan.buffers.stepRecordBytes);
    EXPECT_EQ(3u * sizeof(std::uint32_t), plan.buffers.retainedIndexBytes);
    EXPECT_EQ(0u, plan.buffers.denoiserFeatureRecordBytes);
    EXPECT_EQ(3u * sizeof(std::uint32_t), plan.buffers.activePathCountBytes);
    EXPECT_EQ(accumulationLayout.totalBytes(), plan.buffers.accumulationBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.initialPathStateBytes,
              plan.buffers.totalUploadBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.activePathStateBytes +
                plan.buffers.nextPathStateBytes + plan.buffers.stepRecordBytes +
                plan.buffers.retainedIndexBytes + plan.buffers.denoiserFeatureRecordBytes +
                plan.buffers.activePathCountBytes + plan.buffers.accumulationBytes,
              plan.buffers.totalResidentBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, UsesGpuPrimaryDescriptorToSkipInitialPathUpload) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(0.0, 0.0, 2.0)}}));
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(24u, plan.parameters.initialPathCount);
    EXPECT_EQ(0u, plan.parameters.primaryPathSampleOffset);
    EXPECT_EQ(4u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(3u, plan.parameters.primaryPathRequestedWidth);
    EXPECT_EQ(2u, plan.parameters.primaryPathRequestedHeight);
    EXPECT_EQ(0, plan.parameters.primaryPathRequestedLeft);
    EXPECT_EQ(0, plan.parameters.primaryPathRequestedTop);
    EXPECT_EQ(3u, plan.parameters.primaryPathActualWidth);
    EXPECT_EQ(2u, plan.parameters.primaryPathActualHeight);
    EXPECT_EQ(0, plan.parameters.primaryPathActualLeft);
    EXPECT_EQ(0, plan.parameters.primaryPathActualTop);
    EXPECT_EQ(gpuPrimaryPathMotionModeOriginDelta, plan.parameters.primaryPathMotionMode);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionOriginDelta[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionOriginDelta[1]);
    EXPECT_FLOAT_EQ(2.0f, plan.parameters.primaryPathMotionOriginDelta[2]);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
    EXPECT_EQ(24u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, CopiesPrimarySampleSubrangeDescriptor) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    options.sampleOffset = 2u;
    options.sampleCount = 1u;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    Scene scene;
    const GpuTracingSceneSections sections = sectionsFor(scene);
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModePinhole, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(6u, plan.parameters.initialPathCount);
    EXPECT_EQ(2u, plan.parameters.primaryPathSampleOffset);
    EXPECT_EQ(1u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(6u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
  }

  TEST(GpuDiffusePrimarySampleChunks, SplitsDescriptorOnlyPrimarySamples) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 1u;

    ASSERT_TRUE(canChunkGpuDiffusePrimarySamples(generation, settings));
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);

    ASSERT_EQ(4u, chunks.size());
    for (std::uint32_t i = 0; i != chunks.size(); ++i) {
      ASSERT_TRUE(chunks[i].primaryPathGeneration.primaryPathDescriptor.has_value());
      const GpuRectilinearPrimaryPathDescriptor& descriptor =
        chunks[i].primaryPathGeneration.primaryPathDescriptor->rectilinear;
      EXPECT_TRUE(chunks[i].primaryPathGeneration.pathStates.empty());
      EXPECT_EQ(i, descriptor.sampleOffset);
      EXPECT_EQ(1u, descriptor.samplesPerPixel);
      EXPECT_EQ(6u, chunks[i].primaryPathGeneration.generatedPrimarySamples);
      EXPECT_EQ(6u, chunks[i].primaryPathGeneration.primaryPathDescriptor->pathCount());
      EXPECT_EQ(i == 0u, chunks[i].firstChunk);
      EXPECT_EQ(i == 3u, chunks[i].finalChunk);
    }
  }

  TEST(GpuDiffusePrimarySampleChunks, KeepsSmallAutomaticLaunchesUnchunked) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 0u;

    EXPECT_EQ(0u, resolvedGpuDiffusePrimarySampleChunkSize(generation, settings));
    EXPECT_FALSE(canChunkGpuDiffusePrimarySamples(generation, settings));
    EXPECT_TRUE(gpuDiffusePrimarySampleChunksFor(generation, settings).empty());
  }

  TEST(GpuDiffusePrimarySampleChunks, AutoChunksLargeDescriptorBackedSamples) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 1024, 1024));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 1024, 1024), 99, 1234,
                                                     options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 0u;

    EXPECT_EQ(1u, resolvedGpuDiffusePrimarySampleChunkSize(generation, settings));
    ASSERT_TRUE(canChunkGpuDiffusePrimarySamples(generation, settings));
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);

    ASSERT_EQ(32u, chunks.size());
    for (std::uint32_t i = 0; i != chunks.size(); ++i) {
      ASSERT_TRUE(chunks[i].primaryPathGeneration.primaryPathDescriptor.has_value());
      const GpuRectilinearPrimaryPathDescriptor& descriptor =
        chunks[i].primaryPathGeneration.primaryPathDescriptor->rectilinear;
      EXPECT_EQ(i / 8u, descriptor.sampleOffset);
      EXPECT_EQ(1u, descriptor.samplesPerPixel);
      EXPECT_EQ(1024u, descriptor.actualWidth);
      EXPECT_LE(chunks[i].primaryPathGeneration.generatedPrimarySamples, 128u * 1024u);
      EXPECT_EQ(i == 0u, chunks[i].firstChunk);
      EXPECT_EQ(i + 1u == chunks.size(), chunks[i].finalChunk);
    }
  }

  TEST(GpuDiffusePrimarySampleChunks, AutoChunkingTilesSingleSampleChunksForMediumImages) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 640, 480));
    camera.viewPlane()->sampler()->setup(64, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 640, 480), 99, 1234,
                                                     options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 0u;
    settings.maxDepth = 10u;

    EXPECT_EQ(1u, resolvedGpuDiffusePrimarySampleChunkSize(generation, settings));
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);

    ASSERT_EQ(192u, chunks.size());
    EXPECT_FALSE(chunks.front().completesSampleRange);
    EXPECT_EQ(0u,
              chunks.front().primaryPathGeneration.primaryPathDescriptor->rectilinear.sampleOffset);
    EXPECT_EQ(0, chunks.front().primaryPathGeneration.actualRect.top());
    EXPECT_EQ(163, chunks.front().primaryPathGeneration.actualRect.height());
    EXPECT_LE(chunks.front().primaryPathGeneration.generatedPrimarySamples, 128u * 1024u);
    EXPECT_TRUE(chunks.front().firstChunk);
    ASSERT_LT(2u, chunks.size());
    EXPECT_TRUE(chunks[2].completesSampleRange);
    EXPECT_EQ(63u,
              chunks.back().primaryPathGeneration.primaryPathDescriptor->rectilinear.sampleOffset);
    EXPECT_EQ(326, chunks.back().primaryPathGeneration.actualRect.top());
    EXPECT_EQ(154, chunks.back().primaryPathGeneration.actualRect.height());
    EXPECT_LE(chunks.back().primaryPathGeneration.generatedPrimarySamples, 128u * 1024u);
    EXPECT_TRUE(chunks.back().completesSampleRange);
    EXPECT_TRUE(chunks.back().finalChunk);
  }

  TEST(GpuDiffusePrimarySampleChunks, CapturesProgressDisplayOnlyAtSampleCompletion) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 640, 480));
    camera.viewPlane()->sampler()->setup(64, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 640, 480), 99, 1234,
                                                     options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.captureResolvedDisplay = true;
    settings.maxDepth = 10u;
    settings.primarySampleChunkSize = 0u;
    settings.chunkProgressObserver = [](const GpuDiffusePathLoopChunkProgress&) {};
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);

    ASSERT_EQ(192u, chunks.size());
    EXPECT_LE(chunks.front().primaryPathGeneration.generatedPrimarySamples, 128u * 1024u);
    EXPECT_EQ(204, chunks.front().primaryPathGeneration.actualRect.height());
    std::size_t capturedProgressDisplays = 0;
    for (const GpuDiffusePrimaryPathSampleChunk& chunk : chunks) {
      if (shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunk)) {
        ++capturedProgressDisplays;
      }
    }
    EXPECT_EQ(64u, capturedProgressDisplays);
    EXPECT_FALSE(shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunks.front()));
    ASSERT_LT(2u, chunks.size());
    EXPECT_TRUE(shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunks[2]));
    EXPECT_EQ(408, chunks[2].primaryPathGeneration.actualRect.top());
    EXPECT_EQ(72, chunks[2].primaryPathGeneration.actualRect.height());
    EXPECT_TRUE(shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunks.back()));

    settings.chunkProgressObserver = {};
    capturedProgressDisplays = 0;
    for (const GpuDiffusePrimaryPathSampleChunk& chunk : chunks) {
      if (shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunk)) {
        ++capturedProgressDisplays;
      }
    }
    EXPECT_EQ(1u, capturedProgressDisplays);

    settings.captureResolvedDisplay = false;
    EXPECT_FALSE(shouldCaptureGpuDiffusePathLoopChunkResolvedDisplay(settings, chunks.back()));
  }

  TEST(GpuDiffusePrimarySampleChunks, AutoChunksLargeDenoiserFeatureLaunches) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 1024, 1024));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 1024, 1024), 99, 1234,
                                                     options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.captureDenoiserFeatures = true;
    settings.primarySampleChunkSize = 0u;

    EXPECT_EQ(1u, resolvedGpuDiffusePrimarySampleChunkSize(generation, settings));
    ASSERT_TRUE(canChunkGpuDiffusePrimarySamples(generation, settings));
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);

    ASSERT_EQ(32u, chunks.size());
    EXPECT_EQ(0u,
              chunks.front().primaryPathGeneration.primaryPathDescriptor->rectilinear.sampleOffset);
    EXPECT_TRUE(chunks.front().firstChunk);
    EXPECT_TRUE(chunks.back().finalChunk);
  }

  TEST(GpuDiffusePrimarySampleChunks, CapsExplicitChunkSizeForLargeDescriptorBackedSamples) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 1024, 1024));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 1024, 1024), 99, 1234,
                                                     options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 4u;

    EXPECT_EQ(1u, resolvedGpuDiffusePrimarySampleChunkSize(generation, settings));
    ASSERT_TRUE(canChunkGpuDiffusePrimarySamples(generation, settings));
    EXPECT_EQ(32u, gpuDiffusePrimarySampleChunksFor(generation, settings).size());
  }

  TEST(GpuDiffusePrimarySampleChunks, KeepsDiagnosticsLaunchesUnchunked) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    GpuDiffusePathLoopSettings settings;
    settings.primarySampleChunkSize = 1u;

    EXPECT_EQ(0u, resolvedGpuDiffusePrimarySampleChunkSize(generation, settings));
    EXPECT_FALSE(canChunkGpuDiffusePrimarySamples(generation, settings));
    EXPECT_TRUE(gpuDiffusePrimarySampleChunksFor(generation, settings).empty());
  }

  TEST(GpuDiffusePrimarySampleChunks, ReusesFullSampleSlotAccumulationLayoutForChunks) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 1u;
    const GpuDiffusePathLoopPlatformAccumulationPlan accumulation =
      platformGpuDiffusePathLoopAccumulationPlanFor(generation, "test");
    ASSERT_EQ(gpuDiffusePathLoopAccumulationTargetSampleSlot, accumulation.targetMode);
    EXPECT_EQ(6, accumulation.layout.width);
    EXPECT_EQ(4, accumulation.layout.height);

    const GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);
    ASSERT_EQ(4u, chunks.size());
    const GpuDiffusePathLoopLaunchPlan secondChunkPlan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, chunks[1].primaryPathGeneration, accumulation.layout, settings);

    EXPECT_EQ(6u, secondChunkPlan.parameters.imageWidth);
    EXPECT_EQ(4u, secondChunkPlan.parameters.imageHeight);
    EXPECT_EQ(1u, secondChunkPlan.parameters.primaryPathSampleOffset);
    EXPECT_EQ(1u, secondChunkPlan.parameters.primaryPathSamplesPerPixel);
  }

  TEST(GpuDiffusePrimarySampleChunks, UsesPixelAccumulationLayoutForSingleSampleChunks) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 1u;
    const GpuDiffusePathLoopPlatformAccumulationPlan accumulation =
      platformGpuDiffusePathLoopAccumulationPlanFor(generation, settings, "test");

    ASSERT_EQ(gpuDiffusePathLoopAccumulationTargetPixel, accumulation.targetMode);
    EXPECT_EQ(6, accumulation.layout.width);
    EXPECT_EQ(1, accumulation.layout.height);

    const GpuTracingSceneSections sections;
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);
    ASSERT_EQ(4u, chunks.size());
    const GpuDiffusePathLoopLaunchPlan secondChunkPlan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, chunks[1].primaryPathGeneration, accumulation.layout, settings);

    EXPECT_EQ(6u, secondChunkPlan.parameters.imageWidth);
    EXPECT_EQ(1u, secondChunkPlan.parameters.imageHeight);
    EXPECT_EQ(1u, secondChunkPlan.parameters.primaryPathSampleOffset);
    EXPECT_EQ(1u, secondChunkPlan.parameters.primaryPathSamplesPerPixel);
  }

  TEST(GpuDiffusePrimarySampleChunks, MergesPlatformChunkResults) {
    GpuDiffusePathLoopPlatformResult merged;
    GpuDiffusePathLoopPlatformResult first;
    first.executionPath = "platform_path_loop";
    first.schedule = gpuDiffusePathLoopScheduleDepthFrontier;
    first.pathStateResidency = "platform_path_state";
    first.retainedFrontierDispatchesIndirect = true;
    first.retainedPathCount = 2u;
    first.activePathCountsPerDepth = {3u, 1u};
    first.sceneUploadBytesWritten = 64u;
    first.kernelWorkerSeconds = 0.25;
    first.resolvedDisplayReadbacks = 1u;
    first.stepRecords.push_back({});
    first.denoiserFeatureRecords.resize(3);
    first.denoiserFeatureRecords[1].pixelIndex = 1u;
    first.denoiserFeatureRecords[1].flags = gpuDiffusePathDenoiserFeatureValidFlag;
    first.denoiserFeatureRecords[1].albedo = {0.25f, 0.0f, 0.0f, 0.0f};

    GpuDiffusePathLoopPlatformResult second;
    second.executionPath = "platform_path_loop";
    second.schedule = gpuDiffusePathLoopScheduleDepthFrontier;
    second.pathStateResidency = "platform_path_state";
    second.retainedPathCount = 1u;
    second.activePathCountsPerDepth = {2u, 4u};
    second.sceneUploadCacheHit = true;
    second.sceneUploadBytesWritten = 0u;
    second.kernelWorkerSeconds = 0.5;
    second.accumulationColorSums = {{{1.0f, 2.0f, 3.0f, 0.0f}}};
    second.accumulationSampleCounts = {2u};
    second.resolvedDisplayPixels = {0x112233u};
    second.resolvedDisplayReadbacks = 2u;
    second.denoiserFeatureRecords.resize(3);
    second.denoiserFeatureRecords[1].pixelIndex = 1u;
    second.denoiserFeatureRecords[1].albedo = {0.75f, 0.0f, 0.0f, 0.0f};
    second.denoiserFeatureRecords[2].pixelIndex = 2u;
    second.denoiserFeatureRecords[2].flags = gpuDiffusePathDenoiserFeatureValidFlag;
    second.denoiserFeatureRecords[2].albedo = {0.5f, 0.0f, 0.0f, 0.0f};

    mergePlatformGpuDiffusePathLoopChunkResult(merged, std::move(first));
    mergePlatformGpuDiffusePathLoopChunkResult(merged, std::move(second));

    EXPECT_EQ("platform_path_loop", merged.executionPath);
    EXPECT_TRUE(merged.retainedFrontierDispatchesIndirect);
    EXPECT_EQ(3u, merged.retainedPathCount);
    ASSERT_EQ(2u, merged.activePathCountsPerDepth.size());
    EXPECT_EQ(5u, merged.activePathCountsPerDepth[0]);
    EXPECT_EQ(5u, merged.activePathCountsPerDepth[1]);
    EXPECT_TRUE(merged.sceneUploadCacheHit);
    EXPECT_EQ(64u, merged.sceneUploadBytesWritten);
    EXPECT_DOUBLE_EQ(0.75, merged.kernelWorkerSeconds);
    ASSERT_EQ(1u, merged.accumulationColorSums.size());
    EXPECT_FLOAT_EQ(2.0f, merged.accumulationColorSums[0][1]);
    ASSERT_EQ(1u, merged.accumulationSampleCounts.size());
    EXPECT_EQ(2u, merged.accumulationSampleCounts[0]);
    ASSERT_EQ(1u, merged.resolvedDisplayPixels.size());
    EXPECT_EQ(0x112233u, merged.resolvedDisplayPixels[0]);
    EXPECT_EQ(3u, merged.resolvedDisplayReadbacks);
    ASSERT_EQ(3u, merged.denoiserFeatureRecords.size());
    EXPECT_EQ(gpuDiffusePathDenoiserFeatureValidFlag, merged.denoiserFeatureRecords[1].flags);
    EXPECT_FLOAT_EQ(0.25f, merged.denoiserFeatureRecords[1].albedo[0]);
    EXPECT_EQ(gpuDiffusePathDenoiserFeatureValidFlag, merged.denoiserFeatureRecords[2].flags);
    EXPECT_FLOAT_EQ(0.5f, merged.denoiserFeatureRecords[2].albedo[0]);
  }

  TEST(GpuDiffusePrimarySampleChunks, RejectsMismatchedChunkDenoiserFeatureSizes) {
    GpuDiffusePathLoopPlatformResult merged;
    merged.executionPath = "platform_path_loop";
    merged.denoiserFeatureRecords.resize(2);

    GpuDiffusePathLoopPlatformResult chunk;
    chunk.executionPath = "platform_path_loop";
    chunk.denoiserFeatureRecords.resize(3);

    EXPECT_THROW(mergePlatformGpuDiffusePathLoopChunkResult(merged, std::move(chunk)),
                 std::logic_error);
  }

  TEST(GpuDiffusePrimarySampleChunks, NotifiesChunkProgressObserver) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.primarySampleChunkSize = 1u;
    std::vector<GpuDiffusePathLoopChunkProgress> callbacks;
    settings.chunkProgressObserver = [&callbacks](const GpuDiffusePathLoopChunkProgress& progress) {
      callbacks.push_back(progress);
    };
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);
    ASSERT_EQ(4u, chunks.size());

    GpuDiffusePathLoopPlatformResult platformChunk;
    platformChunk.resolvedDisplayPixels = {0x112233u, 0x445566u};
    notifyGpuDiffusePathLoopChunkProgress(settings, generation, chunks[1], platformChunk);

    ASSERT_EQ(1u, callbacks.size());
    EXPECT_EQ(1u, callbacks[0].sampleOffset);
    EXPECT_EQ(1u, callbacks[0].sampleCount);
    EXPECT_EQ(4u, callbacks[0].totalSampleCount);
    EXPECT_EQ(2u, callbacks[0].completedSampleCount);
    EXPECT_FALSE(callbacks[0].firstChunk);
    EXPECT_FALSE(callbacks[0].finalChunk);
    ASSERT_NE(nullptr, callbacks[0].resolvedDisplayPixels);
    ASSERT_EQ(2u, callbacks[0].resolvedDisplayPixels->size());
    EXPECT_EQ(0x445566u, callbacks[0].resolvedDisplayPixels->at(1));
  }

  TEST(GpuDiffusePrimarySampleChunks, ReportsTiledProgressOnlyAfterCompletedSampleRange) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 1024, 1024));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 1024, 1024), 99, 1234,
                                                     options);

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.captureResolvedDisplay = true;
    settings.primarySampleChunkSize = 0u;
    settings.maxDepth = 10u;
    std::vector<GpuDiffusePathLoopChunkProgress> callbacks;
    settings.chunkProgressObserver = [&callbacks](const GpuDiffusePathLoopChunkProgress& progress) {
      callbacks.push_back(progress);
    };
    const std::vector<GpuDiffusePrimaryPathSampleChunk> chunks =
      gpuDiffusePrimarySampleChunksFor(generation, settings);
    ASSERT_EQ(32u, chunks.size());
    ASSERT_FALSE(chunks.front().completesSampleRange);
    ASSERT_LT(7u, chunks.size());
    ASSERT_TRUE(chunks[7].completesSampleRange);

    GpuDiffusePathLoopPlatformResult platformChunk;
    notifyGpuDiffusePathLoopChunkProgress(settings, generation, chunks.front(), platformChunk);
    platformChunk.resolvedDisplayPixels = {0x112233u};
    notifyGpuDiffusePathLoopChunkProgress(settings, generation, chunks[7], platformChunk);

    ASSERT_EQ(2u, callbacks.size());
    EXPECT_EQ(0u, callbacks[0].completedSampleCount);
    EXPECT_EQ(nullptr, callbacks[0].resolvedDisplayPixels);
    EXPECT_EQ(1u, callbacks[1].completedSampleCount);
    ASSERT_NE(nullptr, callbacks[1].resolvedDisplayPixels);
    EXPECT_EQ(0x112233u, callbacks[1].resolvedDisplayPixels->front());
  }

  TEST(GpuDiffusePathLoopBackend, CancellationCallbackReportsCancellation) {
    GpuDiffusePathLoopSettings settings;
    EXPECT_FALSE(gpuDiffusePathLoopCancelled(settings));
    EXPECT_NO_THROW(throwIfGpuDiffusePathLoopCancelled(settings));

    bool cancelled = false;
    settings.cancellationCallback = [&cancelled] { return cancelled; };
    EXPECT_FALSE(gpuDiffusePathLoopCancelled(settings));
    EXPECT_NO_THROW(throwIfGpuDiffusePathLoopCancelled(settings));

    cancelled = true;
    EXPECT_TRUE(gpuDiffusePathLoopCancelled(settings));
    EXPECT_THROW(throwIfGpuDiffusePathLoopCancelled(settings), std::runtime_error);
  }

  TEST(GpuDiffusePathLoop, ThrowsWhenCancelledBeforeRun) {
    GpuDiffusePathLoopSettings settings;
    settings.cancellationCallback = [] { return true; };

    EXPECT_THROW((void)GpuDiffusePathLoop().run(GpuTracingSceneSections(), {}, settings),
                 std::runtime_error);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, CopiesLookAtPrimaryMotionDescriptor) {
    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("position",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, -5.0)}, {1.0, Vector3d(0.0, 0.0, -3.0)}}));
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 2.0)}}));
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      GpuTracingSceneSections(), generation, TracingAccumulationLayout::image(3, 2), settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, plan.parameters.primaryPathMotionMode);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathOrigin[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathOrigin[1]);
    EXPECT_FLOAT_EQ(-5.0f, plan.parameters.primaryPathOrigin[2]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionOriginDelta[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionOriginDelta[1]);
    EXPECT_FLOAT_EQ(2.0f, plan.parameters.primaryPathMotionOriginDelta[2]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTarget[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTarget[1]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTarget[2]);
    EXPECT_FLOAT_EQ(1.0f, plan.parameters.primaryPathMotionTargetDelta[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTargetDelta[1]);
    EXPECT_FLOAT_EQ(2.0f, plan.parameters.primaryPathMotionTargetDelta[2]);
    EXPECT_FLOAT_EQ(5.0f, plan.parameters.primaryPathMotionParameters[0]);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, CopiesOrthographicLookAtPrimaryMotionDescriptor) {
    OrthographicCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    camera.setAnimationFrame(0.0);
    camera.setShutterInterval(0.0, 1.0);
    camera.setAnimationTrack("target",
                             render::animation::AnimationTrack(
                               {{0.0, Vector3d(0.0, 0.0, 0.0)}, {1.0, Vector3d(1.0, 0.0, 0.0)}}));
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      GpuTracingSceneSections(), generation, TracingAccumulationLayout::image(3, 2), settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeOrthographic, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(gpuPrimaryPathMotionModeLookAt, plan.parameters.primaryPathMotionMode);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathOrigin[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathOrigin[1]);
    EXPECT_FLOAT_EQ(-5.0f, plan.parameters.primaryPathOrigin[2]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionOriginDelta[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionOriginDelta[1]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionOriginDelta[2]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTarget[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTarget[1]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTarget[2]);
    EXPECT_FLOAT_EQ(1.0f, plan.parameters.primaryPathMotionTargetDelta[0]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTargetDelta[1]);
    EXPECT_FLOAT_EQ(0.0f, plan.parameters.primaryPathMotionTargetDelta[2]);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, SizesDescriptorOnlyPrimaryLaunches) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(24u, plan.parameters.initialPathCount);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(24u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(24u * sizeof(GpuDiffusePathStateRecord), plan.buffers.nextPathStateBytes);
    EXPECT_EQ(24u * 3u * sizeof(GpuDiffusePathStepRecord), plan.buffers.stepRecordBytes);
    EXPECT_EQ(3u * sizeof(std::uint32_t), plan.buffers.activePathCountBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, AcceptsOrthographicPrimaryDescriptorLaunches) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    OrthographicCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeOrthographic, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(24u, plan.parameters.initialPathCount);
    EXPECT_EQ(4u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(24u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, AcceptsThinLensPrimaryDescriptorLaunches) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    ThinLensCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.25);
    camera.setFocalDistance(6.0);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeThinLens, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(24u, plan.parameters.initialPathCount);
    EXPECT_EQ(4u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(24u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, AcceptsTiltShiftPrimaryDescriptorLaunches) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    TiltShiftCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.setApertureRadius(0.25);
    camera.setFocalDistance(6.0);
    camera.setTilt(20_degrees);
    camera.setShift(Vector2d(0.2, -0.1));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeTiltShift, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(24u, plan.parameters.initialPathCount);
    EXPECT_EQ(4u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(24u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, AcceptsEquirectangularPrimaryDescriptorLaunches) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    EquirectangularCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(4, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeEquirectangular,
              plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(32u, plan.parameters.initialPathCount);
    EXPECT_EQ(4u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(32u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, AcceptsSphericalPrimaryDescriptorLaunches) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    SphericalCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(200_degrees, 90_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(4, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeSpherical, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(32u, plan.parameters.initialPathCount);
    EXPECT_EQ(4u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(32u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, AcceptsFishEyePrimaryDescriptorLaunches) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    FishEyeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, -4.0));
    camera.setFieldOfView(180_degrees);
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 4, 4));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 4, 4), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(4, 4);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(gpuPrimaryPathGenerationModeFishEye, plan.parameters.primaryPathGenerationMode);
    EXPECT_EQ(64u, plan.parameters.initialPathCount);
    EXPECT_EQ(4u, plan.parameters.primaryPathSamplesPerPixel);
    EXPECT_EQ(1234u, plan.parameters.primaryPathSampleSeed);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(64u * sizeof(GpuDiffusePathStateRecord), plan.buffers.activePathStateBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, CanDisableDiagnosticReadbackCapture) {
    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      GpuTracingSceneSections(), {activePath()}, TracingAccumulationLayout::image(1, 1), settings);

    EXPECT_EQ(0u, plan.parameters.captureDiagnostics);
    EXPECT_EQ(1u, plan.parameters.captureMetrics);
    EXPECT_EQ(0u, plan.parameters.captureDenoiserFeatures);
    EXPECT_EQ(sizeof(GpuDiffusePathStateRecord), plan.buffers.initialPathStateBytes);
    EXPECT_EQ(0u, plan.buffers.activePathStateBytes);
    EXPECT_EQ(0u, plan.buffers.nextPathStateBytes);
    EXPECT_EQ(0u, plan.buffers.stepRecordBytes);
    EXPECT_EQ(2u * sizeof(std::uint32_t), plan.buffers.retainedIndexBytes);
    EXPECT_EQ(0u, plan.buffers.denoiserFeatureRecordBytes);
    EXPECT_EQ(8u * sizeof(std::uint32_t), plan.buffers.activePathCountBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.initialPathStateBytes,
              plan.buffers.totalUploadBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.retainedIndexBytes +
                plan.buffers.activePathCountBytes + plan.buffers.accumulationBytes,
              plan.buffers.totalResidentBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, CanDisableMetricsReadbackCapture) {
    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.captureMetrics = false;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      GpuTracingSceneSections(), {activePath()}, TracingAccumulationLayout::image(1, 1), settings);

    EXPECT_EQ(0u, plan.parameters.captureDiagnostics);
    EXPECT_EQ(0u, plan.parameters.captureMetrics);
    EXPECT_EQ(sizeof(GpuDiffusePathStateRecord), plan.buffers.initialPathStateBytes);
    EXPECT_EQ(0u, plan.buffers.activePathStateBytes);
    EXPECT_EQ(0u, plan.buffers.nextPathStateBytes);
    EXPECT_EQ(0u, plan.buffers.stepRecordBytes);
    EXPECT_EQ(2u * sizeof(std::uint32_t), plan.buffers.retainedIndexBytes);
    EXPECT_EQ(0u, plan.buffers.activePathCountBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.initialPathStateBytes,
              plan.buffers.totalUploadBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.retainedIndexBytes +
                plan.buffers.accumulationBytes,
              plan.buffers.totalResidentBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, CapturesDenoiserFeaturesWithoutDiagnostics) {
    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.captureDenoiserFeatures = true;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      GpuTracingSceneSections(), {activePath()}, accumulationLayout, settings);

    EXPECT_EQ(0u, plan.parameters.captureDiagnostics);
    EXPECT_EQ(1u, plan.parameters.captureMetrics);
    EXPECT_EQ(1u, plan.parameters.captureDenoiserFeatures);
    EXPECT_EQ(0u, plan.buffers.activePathStateBytes);
    EXPECT_EQ(0u, plan.buffers.nextPathStateBytes);
    EXPECT_EQ(0u, plan.buffers.stepRecordBytes);
    EXPECT_EQ(2u * sizeof(std::uint32_t), plan.buffers.retainedIndexBytes);
    EXPECT_EQ(6u * sizeof(GpuDiffusePathDenoiserFeatureRecord),
              plan.buffers.denoiserFeatureRecordBytes);
    EXPECT_EQ(8u * sizeof(std::uint32_t), plan.buffers.activePathCountBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.initialPathStateBytes,
              plan.buffers.totalUploadBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.denoiserFeatureRecordBytes +
                plan.buffers.retainedIndexBytes + plan.buffers.activePathCountBytes +
                plan.buffers.accumulationBytes,
              plan.buffers.totalResidentBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, SkipsHostAndTraceBuffersForTraceDisabledDescriptorLaunch) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 3, 2));
    camera.viewPlane()->sampler()->setup(4, 8, 42);
    GpuDiffusePrimaryPathStateGenerationOptions options;
    options.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration generation =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 3, 2), 99, 1234, options);
    ASSERT_TRUE(generation.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(generation.pathStates.empty());

    GpuDiffusePathLoopSettings settings;
    settings.captureDiagnostics = false;
    settings.maxDepth = 3;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(3, 2);

    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, generation, accumulationLayout, settings);

    EXPECT_TRUE(plan.generatesPrimaryPathsOnDevice());
    EXPECT_EQ(0u, plan.parameters.captureDiagnostics);
    EXPECT_EQ(1u, plan.parameters.captureMetrics);
    EXPECT_EQ(0u, plan.parameters.captureDenoiserFeatures);
    EXPECT_EQ(24u, plan.parameters.initialPathCount);
    EXPECT_EQ(0u, plan.buffers.initialPathStateBytes);
    EXPECT_EQ(0u, plan.buffers.activePathStateBytes);
    EXPECT_EQ(0u, plan.buffers.nextPathStateBytes);
    EXPECT_EQ(0u, plan.buffers.stepRecordBytes);
    EXPECT_EQ(25u * sizeof(std::uint32_t), plan.buffers.retainedIndexBytes);
    EXPECT_EQ(0u, plan.buffers.denoiserFeatureRecordBytes);
    EXPECT_EQ(3u * sizeof(std::uint32_t), plan.buffers.activePathCountBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes, plan.buffers.totalUploadBytes);
    EXPECT_EQ(plan.buffers.sceneUploadBytes + plan.buffers.retainedIndexBytes +
                plan.buffers.activePathCountBytes + plan.buffers.accumulationBytes,
              plan.buffers.totalResidentBytes);
  }

  TEST(GpuDiffusePathLoopLaunchPlanner, RejectsInvalidSettingsAndLayout) {
    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 0;
    EXPECT_THROW((void)GpuDiffusePathLoopLaunchPlanner().plan(
                   GpuTracingSceneSections(), std::vector<GpuDiffusePathStateRecord>{},
                   TracingAccumulationLayout::image(1, 1), settings),
                 std::invalid_argument);

    settings.maxDepth = 1;
    TracingAccumulationLayout invalidLayout;
    invalidLayout.width = 0;
    invalidLayout.height = 1;
    EXPECT_THROW((void)GpuDiffusePathLoopLaunchPlanner().plan(
                   GpuTracingSceneSections(), std::vector<GpuDiffusePathStateRecord>{},
                   invalidLayout, settings),
                 std::invalid_argument);
  }

  TEST(MetalGpuDiffusePathLoopKernel, LaunchProbeConsumesShaderFacingPlanWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 1;
    settings.directLightSamples = 3;
    settings.displayResolveTonemap = GpuDisplayResolveTonemap::Aces;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 12;
    paths[0].sampleSeed = 101;
    paths[1].pixelIndex = 13;
    paths[1].depth = 2;
    paths[1].previousBsdfPdf = 0.5f;
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runLaunchProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_launch_probe", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    EXPECT_EQ(plan.parameters.layoutVersion, result.echoedParameters.layoutVersion);
    EXPECT_EQ(plan.parameters.maxDepth, result.echoedParameters.maxDepth);
    EXPECT_EQ(plan.parameters.russianRouletteDepth, result.echoedParameters.russianRouletteDepth);
    EXPECT_EQ(plan.parameters.directLightSamples, result.echoedParameters.directLightSamples);
    EXPECT_EQ(plan.parameters.captureDiagnostics, result.echoedParameters.captureDiagnostics);
    EXPECT_EQ(plan.parameters.captureMetrics, result.echoedParameters.captureMetrics);
    EXPECT_EQ(plan.parameters.captureDenoiserFeatures,
              result.echoedParameters.captureDenoiserFeatures);
    EXPECT_EQ(plan.parameters.displayResolveTonemap, result.echoedParameters.displayResolveTonemap);
    EXPECT_EQ(plan.parameters.initialPathCount, result.echoedParameters.initialPathCount);
    EXPECT_EQ(plan.parameters.imageWidth, result.echoedParameters.imageWidth);
    EXPECT_EQ(plan.parameters.imageHeight, result.echoedParameters.imageHeight);
    EXPECT_EQ(plan.parameters.materialCount, result.echoedParameters.materialCount);
    EXPECT_EQ(plan.parameters.textureCount, result.echoedParameters.textureCount);
    EXPECT_EQ(plan.parameters.lightCount, result.echoedParameters.lightCount);
    EXPECT_EQ(plan.parameters.environmentCount, result.echoedParameters.environmentCount);
    EXPECT_EQ(plan.parameters.debugIdCount, result.echoedParameters.debugIdCount);
    EXPECT_EQ(plan.parameters.geometryByteOffset, result.echoedParameters.geometryByteOffset);
    EXPECT_EQ(plan.parameters.materialByteOffset, result.echoedParameters.materialByteOffset);
    EXPECT_EQ(plan.parameters.textureByteOffset, result.echoedParameters.textureByteOffset);
    EXPECT_EQ(plan.parameters.lightByteOffset, result.echoedParameters.lightByteOffset);
    EXPECT_EQ(plan.parameters.environmentByteOffset, result.echoedParameters.environmentByteOffset);
    EXPECT_EQ(plan.parameters.debugIdByteOffset, result.echoedParameters.debugIdByteOffset);
    EXPECT_EQ(plan.parameters.sceneUploadBytes, result.echoedParameters.sceneUploadBytes);
    EXPECT_EQ(plan.parameters.accumulationTargetMode,
              result.echoedParameters.accumulationTargetMode);
    EXPECT_EQ(plan.parameters.bvhByteOffset, result.echoedParameters.bvhByteOffset);
    EXPECT_EQ(plan.parameters.primitiveByteOffset, result.echoedParameters.primitiveByteOffset);
    EXPECT_EQ(plan.parameters.triangleByteOffset, result.echoedParameters.triangleByteOffset);
    EXPECT_EQ(plan.parameters.sphereByteOffset, result.echoedParameters.sphereByteOffset);
    EXPECT_EQ(plan.parameters.planeByteOffset, result.echoedParameters.planeByteOffset);
    EXPECT_EQ(plan.parameters.rectangleByteOffset, result.echoedParameters.rectangleByteOffset);
    EXPECT_EQ(plan.parameters.diskByteOffset, result.echoedParameters.diskByteOffset);
    EXPECT_EQ(plan.parameters.openCylinderByteOffset,
              result.echoedParameters.openCylinderByteOffset);
    EXPECT_EQ(plan.parameters.torusByteOffset, result.echoedParameters.torusByteOffset);
    EXPECT_EQ(plan.parameters.transformByteOffset, result.echoedParameters.transformByteOffset);
    EXPECT_EQ(plan.parameters.bvhNodeCount, result.echoedParameters.bvhNodeCount);
    EXPECT_EQ(plan.parameters.primitiveCount, result.echoedParameters.primitiveCount);
    EXPECT_EQ(plan.parameters.triangleCount, result.echoedParameters.triangleCount);
    EXPECT_EQ(plan.parameters.sphereCount, result.echoedParameters.sphereCount);
    EXPECT_EQ(plan.parameters.planeCount, result.echoedParameters.planeCount);
    EXPECT_EQ(plan.parameters.rectangleCount, result.echoedParameters.rectangleCount);
    EXPECT_EQ(plan.parameters.diskCount, result.echoedParameters.diskCount);
    EXPECT_EQ(plan.parameters.openCylinderCount, result.echoedParameters.openCylinderCount);
    EXPECT_EQ(plan.parameters.torusCount, result.echoedParameters.torusCount);
    EXPECT_EQ(plan.parameters.transformCount, result.echoedParameters.transformCount);
    EXPECT_EQ(plan.buffers.totalUploadBytes, result.bufferSizes.totalUploadBytes);
    EXPECT_EQ(plan.buffers.totalResidentBytes, result.bufferSizes.totalResidentBytes);
    ASSERT_EQ(paths.size(), result.copiedInitialPathStates.size());
    EXPECT_EQ(12u, result.copiedInitialPathStates[0].pixelIndex);
    EXPECT_EQ(101u, result.copiedInitialPathStates[0].sampleSeed);
    EXPECT_EQ(40u, result.copiedInitialPathStates[0].ray.rayIndex);
    EXPECT_EQ(13u, result.copiedInitialPathStates[1].pixelIndex);
    EXPECT_EQ(2u, result.copiedInitialPathStates[1].depth);
    EXPECT_EQ(41u, result.copiedInitialPathStates[1].ray.rayIndex);
    EXPECT_FLOAT_EQ(0.5f, result.copiedInitialPathStates[1].previousBsdfPdf);
    ASSERT_EQ(paths.size(), result.stepRecords.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[0].event);
    EXPECT_EQ(0u, result.stepRecords[0].pathIndex);
    EXPECT_EQ(12u, result.stepRecords[0].pixelIndex);
    EXPECT_EQ(paths[0].primarySampleIndex, result.stepRecords[0].primarySampleIndex);
    EXPECT_EQ(paths[0].depth, result.stepRecords[0].depth);
    EXPECT_EQ(paths[0].flags, result.stepRecords[0].flags);
    EXPECT_EQ(paths[0].throughput, result.stepRecords[0].continuationThroughput);
    EXPECT_EQ(1u, result.stepRecords[1].pathIndex);
    EXPECT_EQ(13u, result.stepRecords[1].pixelIndex);
    EXPECT_EQ(2u, result.stepRecords[1].depth);
    EXPECT_EQ(paths[1].flags, result.stepRecords[1].flags);
    EXPECT_EQ(paths[1].throughput, result.stepRecords[1].continuationThroughput);
    EXPECT_EQ(std::vector<std::uint32_t>({0u, 1u}),
              sortedRetainedPathIndices(result.retainedPathIndices));
    EXPECT_GE(result.uploadWorkerSeconds, 0.0);
    EXPECT_GE(result.kernelWorkerSeconds, 0.0);
    EXPECT_GE(result.readbackWorkerSeconds, 0.0);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, AllMissProbeResolvesBackgroundAndEnvironmentWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    scene.setEnvironmentRadiance(Colord(0.75, 0.5, 0.25));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41),
                                                 makeTerminatedGpuDiffusePathState()};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[1].pixelIndex = 1;
    paths[1].depth = 1;
    paths[1].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[2].pixelIndex = 2;
    paths[2].primarySampleIndex = 3;
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runAllMissProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_all_miss_probe", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(paths.size(), result.resolvedPathStates.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());

    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[0]));
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[1]));
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[2]));
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375),
                      Colord(result.resolvedPathStates[0].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.375, 0.125, 0.03125),
                      Colord(result.resolvedPathStates[1].accumulatedRadiance), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.resolvedPathStates[2].accumulatedRadiance),
                      1e-6);

    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[1].event);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[2].event);
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), Colord(result.stepRecords[0].missRadiance),
                      1e-6);
    ASSERT_COLOR_NEAR(Colord(0.375, 0.125, 0.03125), Colord(result.stepRecords[1].missRadiance),
                      1e-6);
    EXPECT_EQ(result.resolvedPathStates[0].flags, result.stepRecords[0].flags);
    EXPECT_EQ(result.resolvedPathStates[1].flags, result.stepRecords[1].flags);
    EXPECT_EQ(paths[2].flags, result.stepRecords[2].flags);
    EXPECT_TRUE(result.retainedPathIndices.empty());

    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord(0.125, 0.125, 0.09375), Colord(result.accumulationColorSums[0]), 1e-6);
    ASSERT_COLOR_NEAR(Colord(0.375, 0.125, 0.03125), Colord(result.accumulationColorSums[1]), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.accumulationColorSums[2]), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.accumulationColorSums[3]), 1e-6);
    EXPECT_EQ(1u, result.accumulationSampleCounts[0]);
    EXPECT_EQ(1u, result.accumulationSampleCounts[1]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[2]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[3]);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, ClosestHitProbeIntersectsSphereWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40),
      activePath(Rayd(Vector4d(3.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 41),
      makeTerminatedGpuDiffusePathState()};
    paths[0].pixelIndex = 0;
    paths[1].pixelIndex = 1;
    paths[2].pixelIndex = 2;
    paths[2].ray = GpuIntersectionScenePacker().packRay(
      Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 42);
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runClosestHitProbe(plan, paths);
    const GpuIntersectionHitRecord expectedHit =
      GpuIntersectionIntersector().intersectClosest(sections.geometry, paths[0].ray);

    EXPECT_EQ("metal_diffuse_path_loop_closest_hit_probe", result.executionPath);
    EXPECT_EQ("metal_shared_diffuse_path_state", result.pathStateResidency);
    ASSERT_EQ(paths.size(), result.copiedInitialPathStates.size());
    ASSERT_EQ(paths.size(), result.closestHitRecords.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());

    EXPECT_EQ(1u, result.closestHitRecords[0].hit);
    EXPECT_EQ(expectedHit.material, result.closestHitRecords[0].material);
    EXPECT_EQ(expectedHit.object, result.closestHitRecords[0].object);
    EXPECT_EQ(expectedHit.primitiveRecord, result.closestHitRecords[0].primitiveRecord);
    EXPECT_EQ(40u, result.closestHitRecords[0].rayIndex);
    EXPECT_NEAR(expectedHit.distance, result.closestHitRecords[0].distance, 1e-5f);
    expectFloat4Near(result.closestHitRecords[0].point, expectedHit.point, 1e-5f);
    expectFloat4Near(result.closestHitRecords[0].normal, expectedHit.normal, 1e-5f);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(expectedHit.material, result.stepRecords[0].material);
    EXPECT_EQ(expectedHit.object, result.stepRecords[0].object);
    EXPECT_EQ(paths[0].flags, result.stepRecords[0].flags);

    EXPECT_EQ(0u, result.closestHitRecords[1].hit);
    EXPECT_EQ(41u, result.closestHitRecords[1].rayIndex);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[1].event);
    EXPECT_EQ(paths[1].flags, result.stepRecords[1].flags);

    EXPECT_EQ(0u, result.closestHitRecords[2].hit);
    EXPECT_EQ(42u, result.closestHitRecords[2].rayIndex);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Inactive),
              result.stepRecords[2].event);
    EXPECT_EQ(paths[2].flags, result.stepRecords[2].flags);
    EXPECT_EQ(std::vector<std::uint32_t>({0u, 1u}),
              sortedRetainedPathIndices(result.retainedPathIndices));
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, ClosestHitProbeRejectsEmptyGeometryBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runClosestHitProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, ClosestHitProbeRejectsUnsupportedGeometryBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);
    plan.parameters.sphereCount = 0;

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runClosestHitProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteHitShadingProbeComputesContinuationWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.2, 0.4, 0.8)));
    matte->setDiffuseCoefficient(0.5);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40),
      activePath(Rayd(Vector4d(3.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 41)};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[1].pixelIndex = 1;
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runMatteHitShadingProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_matte_hit_shading_probe", result.executionPath);
    ASSERT_EQ(paths.size(), result.closestHitRecords.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());
    EXPECT_EQ(1u, result.closestHitRecords[0].hit);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(result.closestHitRecords[0].material, result.stepRecords[0].material);
    EXPECT_EQ(result.closestHitRecords[0].object, result.stepRecords[0].object);
    expectFloat4Near(result.stepRecords[0].continuationThroughput, {0.05f, 0.05f, 0.05f, 0.0f},
                     1e-5f);
    EXPECT_EQ(paths[0].flags, result.stepRecords[0].flags);

    EXPECT_EQ(0u, result.closestHitRecords[1].hit);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[1].event);
    EXPECT_EQ(paths[1].flags, result.stepRecords[1].flags);
    EXPECT_EQ(std::vector<std::uint32_t>({0u, 1u}),
              sortedRetainedPathIndices(result.retainedPathIndices));
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeSpawnsNextPathWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40)};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].ray.timeSample = 0.625f;
    paths[0].throughput = {0.2f, 0.4f, 0.6f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.pathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_matte_continuation_probe", result.executionPath);
    ASSERT_EQ(paths.size(), result.copiedInitialPathStates.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    ASSERT_EQ(paths.size(), result.closestHitRecords.size());
    ASSERT_EQ(paths.size(), result.stepRecords.size());
    expectPathStateNear(result.copiedInitialPathStates[0], paths[0], 1e-5);
    expectHitRecordNear(result.closestHitRecords[0], expected.closestHitRecords[0], 1e-5);
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_EQ(expected.stepRecords[0].material, result.stepRecords[0].material);
    EXPECT_EQ(expected.stepRecords[0].object, result.stepRecords[0].object);
    EXPECT_EQ(expected.pathStates[0].flags, result.stepRecords[0].flags);
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.pathStates[0], 1e-4);
    EXPECT_EQ(std::vector<std::uint32_t>({0u}), result.retainedPathIndices);
    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.accumulationColorSums[0]), 1e-6);
    EXPECT_EQ(0u, result.accumulationSampleCounts[0]);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeAddsCompiledSceneAmbientWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setAmbient(Colord(0.2, 0.3, 0.4));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.5, 0.25, 1.0)));
    matte->setAmbientCoefficient(0.5);
    matte->setDiffuseCoefficient(0.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = compileGpuTracingScene(scene).sections;

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(0.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40)};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.25f, 0.5f, 0.75f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.terminatedPathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.nextPathStates[0]));
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      Colord(result.nextPathStates[0].accumulatedRadiance), 1e-5);
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      Colord(result.accumulationColorSums[0]), 1e-5);
    EXPECT_EQ(1u, result.accumulationSampleCounts[0]);
    EXPECT_TRUE(result.retainedPathIndices.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeResolvesMissAndAccumulatesWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{
      activePath(Rayd(Vector4d(3.0, 0.0, -4.0, 1.0), Vector3d(0.0, 0.0, 1.0)), 40)};
    paths[0].pixelIndex = 1;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    paths[0].accumulatedRadiance = {0.1f, 0.2f, 0.3f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_TRUE(expected.pathStates.empty());
    ASSERT_EQ(1u, expected.terminatedPathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Miss),
              result.stepRecords[0].event);
    expectFloat4Near(result.stepRecords[0].missRadiance, expected.stepRecords[0].missRadiance,
                     1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.terminatedPathStates[0], 1e-4);
    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      Colord(result.accumulationColorSums[1]), 1e-5);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.accumulationColorSums[0]), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.accumulationColorSums[2]), 1e-6);
    ASSERT_COLOR_NEAR(Colord::black(), Colord(result.accumulationColorSums[3]), 1e-6);
    EXPECT_EQ(0u, result.accumulationSampleCounts[0]);
    EXPECT_EQ(1u, result.accumulationSampleCounts[1]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[2]);
    EXPECT_EQ(0u, result.accumulationSampleCounts[3]);
    EXPECT_TRUE(result.retainedPathIndices.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeAddsPointLightContributionWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].ray.timeSample = 0.625f;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.pathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    expectFloat4Near(result.stepRecords[0].directLightRadiance,
                     expected.stepRecords[0].directLightRadiance, 1e-5);
    expectFloat4Near(result.nextPathStates[0].accumulatedRadiance,
                     expected.pathStates[0].accumulatedRadiance, 1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.pathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel,
       MatteContinuationProbeAddsRectangularAreaLightContributionWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(
      std::make_shared<RectangularAreaLight>(Vector3d(0.0, 2.0, -3.0), Vector3d(2.0, 0.0, 0.0),
                                             Vector3d(0.0, 2.0, 0.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_EQ(1u, expected.pathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    expectFloat4Near(result.stepRecords[0].directLightRadiance,
                     expected.stepRecords[0].directLightRadiance, 1e-4);
    expectFloat4Near(result.nextPathStates[0].accumulatedRadiance,
                     expected.pathStates[0].accumulatedRadiance, 1e-4);
    expectPathStateNear(result.nextPathStates[0], expected.pathStates[0], 1e-4);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeTerminatesEmissiveHitWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    auto lightCard = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    lightCard->setMaterial(std::make_shared<EmissiveMaterial>(Colord(2.0, 3.0, 4.0)));
    scene.add(lightCard);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].throughput = {0.25f, 0.5f, 0.75f, 0.0f};
    paths[0].previousEventFlags = gpuDiffusePathStateSampledFromBsdfFlag;
    paths[0].previousBsdfPdf = 0.25f;
    paths[0].previousLightPdf = 0.75f;
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathStepResult expected = GpuDiffusePathStepReference().step(
      sections, paths, closestHitsFor(sections, paths), settings);
    ASSERT_TRUE(expected.pathStates.empty());
    ASSERT_EQ(1u, expected.terminatedPathStates.size());

    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMatteContinuationProbe(plan, paths);

    ASSERT_EQ(paths.size(), result.stepRecords.size());
    ASSERT_EQ(paths.size(), result.nextPathStates.size());
    EXPECT_EQ(static_cast<std::uint32_t>(GpuDiffusePathStepEvent::Hit),
              result.stepRecords[0].event);
    expectFloat4Near(result.stepRecords[0].emittedRadiance, expected.stepRecords[0].emittedRadiance,
                     1e-5);
    expectFloat4Near(result.stepRecords[0].continuationThroughput,
                     expected.stepRecords[0].continuationThroughput, 1e-5);
    expectPathStateNear(result.nextPathStates[0], expected.terminatedPathStates[0], 1e-4);
    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    ASSERT_COLOR_NEAR(Colord(expected.terminatedPathStates[0].accumulatedRadiance),
                      Colord(result.accumulationColorSums[0]), 1e-5);
    EXPECT_EQ(1u, result.accumulationSampleCounts[0]);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MattePathLoopRunsMultipleDepthsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runMattePathLoop(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop", result.executionPath);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    for (std::size_t index = 0; index != expected.stepRecords.size(); ++index) {
      EXPECT_EQ(expected.stepRecords[index].event, result.stepRecords[index].event);
      EXPECT_EQ(expected.stepRecords[index].depth, result.stepRecords[index].depth);
      expectFloat4Near(result.stepRecords[index].directLightRadiance,
                       expected.stepRecords[index].directLightRadiance, 1e-4);
      expectFloat4Near(result.stepRecords[index].missRadiance,
                       expected.stepRecords[index].missRadiance, 1e-4);
      expectFloat4Near(result.stepRecords[index].continuationThroughput,
                       expected.stepRecords[index].continuationThroughput, 1e-4);
    }
    ASSERT_EQ(expected.resolvedPathStates.size(), result.nextPathStates.size());
    expectPathStateNear(result.nextPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_TRUE(result.retainedPathIndices.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, WavefrontPathLoopCompactsFrontiersOnDeviceWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setEnvironmentRadiance(Colord(0.1, 0.2, 0.3));
    auto matte = std::make_shared<MatteMaterial>(
      std::make_shared<ConstantColorTexture>(Colord(0.25, 0.5, 0.75)));
    matte->setDiffuseCoefficient(0.8);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    scene.addLight(std::make_shared<PointLight>(Vector3d(0.0, 0.0, -3.0), Colord(0.8, 0.6, 0.4)));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 2;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    paths[0].pixelIndex = 0;
    paths[0].sampleSeed = 12347;
    paths[0].throughput = {0.5f, 0.25f, 0.125f, 0.0f};
    const GpuDiffusePathLoopLaunchPlan plan =
      GpuDiffusePathLoopLaunchPlanner().plan(sections, paths, accumulationLayout, settings);
    const GpuDiffusePathLoopResult expected = GpuDiffusePathLoop().run(sections, paths, settings);

    const MetalGpuDiffusePathLoopKernelResult result = kernel.runWavefrontPathLoop(plan, paths);

    EXPECT_EQ("metal_diffuse_path_loop_wavefront", result.executionPath);
    EXPECT_TRUE(result.retainedFrontierDispatchesIndirect);
    ASSERT_GE(result.activePathCountsPerDepth.size(), 2u);
    EXPECT_EQ(1u, result.activePathCountsPerDepth[0]);
    EXPECT_EQ(1u, result.activePathCountsPerDepth[1]);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    for (std::size_t index = 0; index != expected.stepRecords.size(); ++index) {
      EXPECT_EQ(expected.stepRecords[index].event, result.stepRecords[index].event);
      EXPECT_EQ(expected.stepRecords[index].depth, result.stepRecords[index].depth);
      expectFloat4Near(result.stepRecords[index].directLightRadiance,
                       expected.stepRecords[index].directLightRadiance, 1e-4);
      expectFloat4Near(result.stepRecords[index].missRadiance,
                       expected.stepRecords[index].missRadiance, 1e-4);
      expectFloat4Near(result.stepRecords[index].continuationThroughput,
                       expected.stepRecords[index].continuationThroughput, 1e-4);
    }
    ASSERT_EQ(expected.resolvedPathStates.size(), result.nextPathStates.size());
    expectPathStateNear(result.nextPathStates[0], expected.resolvedPathStates[0], 1e-4);
    EXPECT_TRUE(result.retainedPathIndices.empty());
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel,
       MattePathLoopDispatchesDescriptorOnlyPrimaryPathsWhenEnabled) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    MetalGpuDiffusePathLoopKernel kernel;
    if (!kernel.launchPathAvailable()) {
      GTEST_SKIP() << kernel.launchPathUnavailableReason();
    }

    Scene scene;
    scene.setBackground(Colord(0.125, 0.25, 0.5));
    scene.setEnvironmentRadiance(Colord(0.0625, 0.125, 0.25));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    PinholeCamera camera(Vector3d(0.0, 0.0, -5.0), Vector3d(0.0, 0.0, 0.0));
    camera.viewPlane()->setup(camera.matrix(), Recti(0, 0, 2, 2));
    camera.viewPlane()->sampler()->setup(1, 4, 42);

    GpuDiffusePrimaryPathStateGenerationOptions descriptorOnlyOptions;
    descriptorOnlyOptions.materializeHostPathStates = false;
    const GpuDiffusePrimaryPathStateGeneration descriptorOnly =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234,
                                                     descriptorOnlyOptions);
    ASSERT_TRUE(descriptorOnly.canGeneratePrimaryPathsOnDevice());
    ASSERT_TRUE(descriptorOnly.pathStates.empty());

    const GpuDiffusePrimaryPathStateGeneration materialized =
      GpuDiffusePrimaryPathStateGenerator().generate(camera, Recti(0, 0, 2, 2), 99, 1234);
    ASSERT_EQ(4u, materialized.pathStates.size());

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    settings.russianRouletteDepth = 10;
    settings.directLightSamples = 1;
    const TracingAccumulationLayout accumulationLayout = TracingAccumulationLayout::image(2, 2);
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, descriptorOnly, accumulationLayout, settings);
    ASSERT_TRUE(plan.generatesPrimaryPathsOnDevice());
    ASSERT_EQ(4u, plan.parameters.initialPathCount);

    const GpuDiffusePathLoopResult expected =
      GpuDiffusePathLoop().run(sections, materialized.pathStates, settings);
    const MetalGpuDiffusePathLoopKernelResult result =
      kernel.runMattePathLoop(plan, descriptorOnly.pathStates);

    EXPECT_EQ("metal_diffuse_path_loop", result.executionPath);
    ASSERT_EQ(expected.stepRecords.size(), result.stepRecords.size());
    ASSERT_EQ(expected.resolvedPathStates.size(), result.nextPathStates.size());
    for (std::size_t index = 0; index != expected.stepRecords.size(); ++index) {
      EXPECT_EQ(expected.stepRecords[index].event, result.stepRecords[index].event);
      EXPECT_EQ(expected.stepRecords[index].depth, result.stepRecords[index].depth);
      expectFloat4Near(result.stepRecords[index].missRadiance,
                       expected.stepRecords[index].missRadiance, 1e-5);
      expectPathStateNear(result.nextPathStates[index], expected.resolvedPathStates[index], 1e-4);
    }
    ASSERT_EQ(4u, result.accumulationColorSums.size());
    ASSERT_EQ(4u, result.accumulationSampleCounts.size());
    for (std::size_t index = 0; index != result.accumulationSampleCounts.size(); ++index) {
      EXPECT_EQ(1u, result.accumulationSampleCounts[index]);
    }
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteHitShadingProbeRejectsMissingRecordsBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);
    plan.parameters.textureCount = 0;

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runMatteHitShadingProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeRejectsMissingRecordsBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);
    plan.parameters.materialCount = 0;

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runMatteContinuationProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, AllMissProbeRejectsDuplicateAccumulationTargets) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    scene.setBackground(Colord(0.25, 0.5, 0.75));
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 0;
    paths[1].pixelIndex = 0;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 2), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runAllMissProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, MatteContinuationProbeRejectsDuplicateAccumulationTargets) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    std::vector<GpuDiffusePathStateRecord> paths{activePath(40), activePath(41)};
    paths[0].pixelIndex = 0;
    paths[1].pixelIndex = 0;
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 2), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runMatteContinuationProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(MetalGpuDiffusePathLoopKernel, AllMissProbeRejectsNonEmptyGeometryBeforeDispatch) {
#if defined(RAYTRACER_ENABLE_METAL_WAVEFRONT)
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    const GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const std::vector<GpuDiffusePathStateRecord> paths{activePath()};
    const GpuDiffusePathLoopLaunchPlan plan = GpuDiffusePathLoopLaunchPlanner().plan(
      sections, paths, TracingAccumulationLayout::image(1, 1), settings);

    EXPECT_THROW((void)MetalGpuDiffusePathLoopKernel().runAllMissProbe(plan, paths),
                 std::invalid_argument);
#else
    GTEST_SKIP() << "Metal wavefront support is not enabled in this build";
#endif
  }

  TEST(CompactingGpuDiffusePathLoopBackend, RejectsMissingCompactionBackend) {
    EXPECT_THROW(CompactingGpuDiffusePathLoopBackend(nullptr), std::invalid_argument);
  }

  TEST(GpuDiffusePathLoop, TerminatesSurvivingContinuationsAtMaxDepth) {
    Scene scene;
    auto matte =
      std::make_shared<MatteMaterial>(std::make_shared<ConstantColorTexture>(Colord::white()));
    matte->setDiffuseCoefficient(1.0);
    auto receiver = std::make_shared<Sphere>(Vector3d(0.0, 0.0, 0.0), 1.0);
    receiver->setMaterial(matte);
    scene.add(receiver);
    GpuTracingSceneSections sections = sectionsFor(scene);

    GpuDiffusePathLoopSettings settings;
    settings.maxDepth = 1;
    const GpuDiffusePathLoopResult result =
      GpuDiffusePathLoop().run(sections, {activePath()}, settings);

    EXPECT_EQ(1u, result.depthCount);
    EXPECT_EQ(1u, result.maxDepthTerminatedPaths);
    ASSERT_EQ(1u, result.resolvedPathStates.size());
    EXPECT_TRUE(gpuDiffusePathStateIsTerminated(result.resolvedPathStates[0]));
    EXPECT_FALSE(gpuDiffusePathStateIsActive(result.resolvedPathStates[0]));
    EXPECT_EQ(1u, result.resolvedPathStates[0].depth);
    EXPECT_EQ(1u, result.metrics.spawnedContinuations);
    EXPECT_EQ(1u, result.metrics.terminatedPaths);

    Buffer<unsigned int> resolved(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, TracingAccumulationLayout::image(2, 2), resolved);
    EXPECT_EQ(1u, diagnostics.addedSamples);
  }

  TEST(GpuDiffusePathLoop, ResolvesMultipleSamplesPerPixelAndRejectsOutOfRangePixels) {
    GpuDiffusePathStateRecord first = makeTerminatedGpuDiffusePathState();
    first.pixelIndex = 0;
    first.accumulatedRadiance = {0.25f, 0.5f, 0.75f, 0.0f};
    GpuDiffusePathStateRecord second = makeTerminatedGpuDiffusePathState();
    second.pixelIndex = 0;
    second.accumulatedRadiance = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathStateRecord third = makeTerminatedGpuDiffusePathState();
    third.pixelIndex = 2;
    third.accumulatedRadiance = {0.25f, 0.0f, 0.5f, 0.0f};

    Buffer<unsigned int> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage({first, second, third}, layout, resolved);

    EXPECT_EQ(Colord(0.5, 0.375, 0.375).rgb(), resolved[0][0]);
    EXPECT_EQ(Colord::black().rgb(), resolved[0][1]);
    EXPECT_EQ(Colord(0.25, 0.0, 0.5).rgb(), resolved[1][0]);
    EXPECT_EQ(Colord::black().rgb(), resolved[1][1]);
    EXPECT_EQ(3u, diagnostics.addedSamples);

    GpuDiffusePathStateRecord outOfRange = makeTerminatedGpuDiffusePathState();
    outOfRange.pixelIndex = 4;
    outOfRange.accumulatedRadiance = {1.0f, 1.0f, 1.0f, 0.0f};
    const auto resolve = [&]() {
      const TracingAccumulationDiagnostics unused =
        resolveGpuDiffusePathLoopImage({outOfRange}, layout, resolved);
      (void)unused;
    };
    EXPECT_THROW(resolve(), std::out_of_range);
  }

  TEST(GpuDiffusePathLoop, ResolvesMultipleSamplesPerPixelIntoHdrImage) {
    GpuDiffusePathStateRecord first = makeTerminatedGpuDiffusePathState();
    first.pixelIndex = 0;
    first.accumulatedRadiance = {0.25f, 0.5f, 0.75f, 0.0f};
    GpuDiffusePathStateRecord second = makeTerminatedGpuDiffusePathState();
    second.pixelIndex = 0;
    second.accumulatedRadiance = {0.75f, 0.25f, 0.0f, 0.0f};
    GpuDiffusePathStateRecord third = makeTerminatedGpuDiffusePathState();
    third.pixelIndex = 2;
    third.accumulatedRadiance = {0.25f, 0.0f, 0.5f, 0.0f};

    Buffer<Colord> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {first, second, third};
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord(0.5, 0.375, 0.375), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][1], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.0, 0.5), resolved[1][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[1][1], 1e-12);
    EXPECT_EQ("gpu_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("resident_accumulation_resolve", diagnostics.residency);
    EXPECT_EQ(layout.totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(3u, diagnostics.addOperations);
    EXPECT_EQ(3u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.colorSumBytes(), diagnostics.readbackBytes);

    Buffer<Colord> wrongSize(1, 2);
    const auto resolveWrongSize = [&]() {
      const TracingAccumulationDiagnostics unused =
        resolveGpuDiffusePathLoopImage(result, layout, wrongSize);
      (void)unused;
    };
    EXPECT_THROW(resolveWrongSize(), std::invalid_argument);
  }

  TEST(GpuDiffusePathLoop, ResolvesMatchingPlatformAccumulationIntoHdrImage) {
    GpuDiffusePathStateRecord fallback = makeTerminatedGpuDiffusePathState();
    fallback.pixelIndex = 0;
    fallback.accumulatedRadiance = {1.0f, 0.0f, 0.0f, 0.0f};

    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {fallback};
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationColorSums = {{{1.0f, 0.5f, 0.0f, 0.0f},
                                             {0.0f, 0.0f, 0.0f, 0.0f},
                                             {0.25f, 0.75f, 0.5f, 0.0f},
                                             {0.0f, 0.0f, 0.0f, 0.0f}}};
    result.platformAccumulationSampleCounts = {2u, 0u, 1u, 0u};

    Buffer<Colord> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord(0.5, 0.25, 0.0), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][1], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.75, 0.5), resolved[1][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[1][1], 1e-12);
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("metal_accumulation_buffer", diagnostics.residency);
    EXPECT_EQ(layout.totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(2u, diagnostics.addOperations);
    EXPECT_EQ(3u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.accumulationBytes(), diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoop, ResolvesPlatformAccumulationWithoutDiagnosticPathStates) {
    GpuDiffusePathLoopResult result;
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationColorSums = {{{0.25f, 0.5f, 0.75f, 0.0f}}};
    result.platformAccumulationSampleCounts = {1u};

    Buffer<Colord> resolved(1, 1);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(1, 1);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), resolved[0][0], 1e-12);
    EXPECT_TRUE(result.resolvedPathStates.empty());
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.accumulationBytes(), diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoop, UsesPlatformResolvedDisplayPixelsWithoutAccumulationReadback) {
    GpuDiffusePathLoopResult result;
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationTargetMode = gpuDiffusePathLoopAccumulationTargetSampleSlot;
    result.platformAccumulationWidth = 2u;
    result.platformAccumulationHeight = 3u;
    result.platformAccumulationAddedSamples = 5u;
    result.platformResolvedDisplayPixels = {Colord(0.5, 0.375, 0.125).rgb(),
                                            Colord(0.5, 1.0 / 3.0, 0.25).rgb()};

    Buffer<unsigned int> resolved(2, 1);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 1);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    EXPECT_EQ(result.platformResolvedDisplayPixels[0], resolved[0][0]);
    EXPECT_EQ(result.platformResolvedDisplayPixels[1], resolved[0][1]);
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("metal_accumulation_buffer", diagnostics.residency);
    EXPECT_EQ(TracingAccumulationLayout::image(2, 3).totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(1u, diagnostics.addOperations);
    EXPECT_EQ(5u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(layout.resolveBytes(), diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoop, ResolvesSampleSlotPlatformAccumulationIntoHdrImage) {
    GpuDiffusePathStateRecord fallback = makeTerminatedGpuDiffusePathState();
    fallback.pixelIndex = 0;
    fallback.accumulatedRadiance = {1.0f, 0.0f, 0.0f, 0.0f};

    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {fallback};
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationTargetMode = gpuDiffusePathLoopAccumulationTargetSampleSlot;
    result.platformAccumulationWidth = 2u;
    result.platformAccumulationHeight = 3u;
    result.platformAccumulationColorSums = {{{0.25f, 0.5f, 0.0f, 0.0f},
                                             {0.75f, 0.25f, 0.25f, 0.0f},
                                             {0.0f, 0.0f, 0.0f, 0.0f},
                                             {0.0f, 0.25f, 0.5f, 0.0f},
                                             {0.5f, 0.0f, 0.25f, 0.0f},
                                             {1.0f, 0.75f, 0.0f, 0.0f}}};
    result.platformAccumulationSampleCounts = {1u, 1u, 0u, 1u, 1u, 1u};

    Buffer<Colord> resolved(2, 1);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 1);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord(0.5, 0.375, 0.125), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.5, 1.0 / 3.0, 0.25), resolved[0][1], 1e-12);
    EXPECT_EQ("metal_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("metal_accumulation_buffer", diagnostics.residency);
    EXPECT_EQ(TracingAccumulationLayout::image(2, 3).totalBytes(), diagnostics.residentBytes);
    EXPECT_EQ(1u, diagnostics.clearOperations);
    EXPECT_EQ(5u, diagnostics.addOperations);
    EXPECT_EQ(5u, diagnostics.addedSamples);
    EXPECT_EQ(1u, diagnostics.resolveOperations);
    EXPECT_EQ(1u, diagnostics.readbackOperations);
    EXPECT_EQ(TracingAccumulationLayout::image(2, 3).accumulationBytes(),
              diagnostics.readbackBytes);
  }

  TEST(GpuDiffusePathLoop, FallsBackToPathStatesWhenPlatformAccumulationShapeDiffers) {
    GpuDiffusePathStateRecord path = makeTerminatedGpuDiffusePathState();
    path.pixelIndex = 3;
    path.accumulatedRadiance = {0.25f, 0.5f, 0.75f, 0.0f};

    GpuDiffusePathLoopResult result;
    result.resolvedPathStates = {path};
    result.platformAccumulationBackend = "metal_diffuse_path_loop";
    result.platformAccumulationResidency = "metal_accumulation_buffer";
    result.platformAccumulationColorSums = {{{1.0f, 0.0f, 0.0f, 0.0f}}};
    result.platformAccumulationSampleCounts = {1u};

    Buffer<Colord> resolved(2, 2);
    const TracingAccumulationLayout layout = TracingAccumulationLayout::image(2, 2);
    const TracingAccumulationDiagnostics diagnostics =
      resolveGpuDiffusePathLoopImage(result, layout, resolved);

    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[0][1], 1e-12);
    ASSERT_COLOR_NEAR(Colord::black(), resolved[1][0], 1e-12);
    ASSERT_COLOR_NEAR(Colord(0.25, 0.5, 0.75), resolved[1][1], 1e-12);
    EXPECT_EQ("gpu_diffuse_path_loop", diagnostics.backend);
    EXPECT_EQ("resident_accumulation_resolve", diagnostics.residency);
    EXPECT_EQ(1u, diagnostics.addedSamples);
  }
}
