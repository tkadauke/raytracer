#include "render/GpuDiffusePathLoopLaunch.h"

#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

namespace render {
  namespace {
    static_assert(std::is_standard_layout_v<GpuDiffusePathLoopLaunchParameters>,
                  "GPU diffuse path-loop launch parameters must stay shader ABI friendly");
    static_assert(alignof(GpuDiffusePathLoopLaunchParameters) == 16,
                  "GPU diffuse path-loop launch parameters must stay 16-byte aligned");
    static_assert(sizeof(GpuDiffusePathLoopLaunchParameters) % 16 == 0,
                  "GPU diffuse path-loop launch parameters must stay 16-byte sized");

    std::uint32_t checkedU32(std::uint64_t value, const char* label) {
      if (value > std::numeric_limits<std::uint32_t>::max()) {
        throw std::overflow_error(std::string(label) + " exceeds GPU 32-bit count range");
      }
      return static_cast<std::uint32_t>(value);
    }

    std::uint64_t checkedAdd(std::uint64_t left, std::uint64_t right, const char* label) {
      if (left > std::numeric_limits<std::uint64_t>::max() - right) {
        throw std::overflow_error(std::string(label) + " byte size overflows");
      }
      return left + right;
    }

    std::uint64_t checkedProduct(std::uint64_t count, std::uint64_t bytes, const char* label) {
      if (bytes != 0 && count > std::numeric_limits<std::uint64_t>::max() / bytes) {
        throw std::overflow_error(std::string(label) + " byte size overflows");
      }
      return count * bytes;
    }

    std::uint64_t pathStateBytes(std::uint64_t pathCount, const char* label) {
      return checkedProduct(pathCount, sizeof(GpuDiffusePathStateRecord), label);
    }

    template<typename Record>
    std::uint32_t assignGeometryRange(std::uint64_t& byteOffset, std::size_t count,
                                      std::uint32_t& countField, const char* label);

    [[nodiscard]] GpuDiffusePathLoopLaunchPlan
    planForInitialPathCount(const GpuTracingSceneSections& scene, std::uint64_t initialPathCount,
                            bool uploadInitialPathStates,
                            const TracingAccumulationLayout& accumulationLayout,
                            const GpuDiffusePathLoopSettings& settings) {
      if (settings.maxDepth == 0) {
        throw std::invalid_argument("GPU diffuse path-loop launch requires positive max depth");
      }

      accumulationLayout.validate();
      const std::uint64_t maxDepth = settings.maxDepth;

      GpuDiffusePathLoopLaunchPlan plan;
      plan.parameters.layoutVersion = gpuDiffusePathLoopLaunchLayoutVersion;
      plan.parameters.maxDepth = settings.maxDepth;
      plan.parameters.russianRouletteDepth = settings.russianRouletteDepth;
      plan.parameters.directLightSamples = settings.directLightSamples;
      plan.parameters.captureDiagnostics = settings.captureDiagnostics ? 1u : 0u;
      plan.parameters.initialPathCount = checkedU32(initialPathCount, "initial path count");
      plan.parameters.imageWidth =
        checkedU32(static_cast<std::uint64_t>(accumulationLayout.width), "image width");
      plan.parameters.imageHeight =
        checkedU32(static_cast<std::uint64_t>(accumulationLayout.height), "image height");
      plan.parameters.materialCount = checkedU32(scene.materials.size(), "material count");
      plan.parameters.textureCount = checkedU32(scene.textures.size(), "texture count");
      plan.parameters.lightCount = checkedU32(scene.lights.size(), "light count");
      plan.parameters.environmentCount = checkedU32(scene.environment.size(), "environment count");
      plan.parameters.debugIdCount = checkedU32(scene.debugIds.size(), "debug id count");
      const auto sectionLayouts = scene.sectionLayouts();
      plan.parameters.geometryByteOffset = sectionLayouts[0].byteOffset;
      plan.parameters.materialByteOffset = sectionLayouts[1].byteOffset;
      plan.parameters.textureByteOffset = sectionLayouts[2].byteOffset;
      plan.parameters.lightByteOffset = sectionLayouts[3].byteOffset;
      plan.parameters.environmentByteOffset = sectionLayouts[4].byteOffset;
      plan.parameters.debugIdByteOffset = sectionLayouts[5].byteOffset;
      std::uint64_t geometryOffset = plan.parameters.geometryByteOffset;
      plan.parameters.bvhByteOffset = assignGeometryRange<GpuIntersectionBvhNode>(
        geometryOffset, scene.geometry.bvh.size(), plan.parameters.bvhNodeCount, "BVH nodes");
      plan.parameters.primitiveByteOffset = assignGeometryRange<GpuIntersectionPrimitiveRecord>(
        geometryOffset, scene.geometry.primitives.size(), plan.parameters.primitiveCount,
        "primitive records");
      plan.parameters.triangleByteOffset = assignGeometryRange<GpuIntersectionTrianglePayload>(
        geometryOffset, scene.geometry.triangles.size(), plan.parameters.triangleCount,
        "triangles");
      plan.parameters.sphereByteOffset = assignGeometryRange<GpuIntersectionSpherePayload>(
        geometryOffset, scene.geometry.spheres.size(), plan.parameters.sphereCount, "spheres");
      plan.parameters.planeByteOffset = assignGeometryRange<GpuIntersectionPlanePayload>(
        geometryOffset, scene.geometry.planes.size(), plan.parameters.planeCount, "planes");
      plan.parameters.rectangleByteOffset = assignGeometryRange<GpuIntersectionRectanglePayload>(
        geometryOffset, scene.geometry.rectangles.size(), plan.parameters.rectangleCount,
        "rectangles");
      plan.parameters.diskByteOffset = assignGeometryRange<GpuIntersectionDiskPayload>(
        geometryOffset, scene.geometry.disks.size(), plan.parameters.diskCount, "disks");
      plan.parameters.openCylinderByteOffset =
        assignGeometryRange<GpuIntersectionOpenCylinderPayload>(
          geometryOffset, scene.geometry.openCylinders.size(), plan.parameters.openCylinderCount,
          "open cylinders");
      plan.parameters.torusByteOffset = assignGeometryRange<GpuIntersectionTorusPayload>(
        geometryOffset, scene.geometry.tori.size(), plan.parameters.torusCount, "tori");
      plan.parameters.transformByteOffset = assignGeometryRange<GpuIntersectionTransformPayload>(
        geometryOffset, scene.geometry.transforms.size(), plan.parameters.transformCount,
        "transforms");

      plan.sceneUpload = scene.uploadBytes();
      plan.buffers.sceneUploadBytes = plan.sceneUpload.size();
      plan.parameters.sceneUploadBytes = checkedU32(plan.sceneUpload.size(), "scene upload bytes");
      plan.buffers.initialPathStateBytes =
        uploadInitialPathStates ? pathStateBytes(initialPathCount, "initial path state") : 0u;
      if (settings.captureDiagnostics) {
        plan.buffers.activePathStateBytes = pathStateBytes(initialPathCount, "active path state");
        plan.buffers.nextPathStateBytes = pathStateBytes(initialPathCount, "next path state");
        plan.buffers.stepRecordBytes =
          checkedProduct(checkedProduct(initialPathCount, maxDepth, "path-step record count"),
                         sizeof(GpuDiffusePathStepRecord), "path-step record");
        const std::uint64_t retainedIndexCount =
          checkedAdd(initialPathCount, 1u, "retained path index count");
        plan.buffers.retainedIndexBytes =
          checkedProduct(retainedIndexCount, sizeof(std::uint32_t), "retained path index");
      }
      plan.buffers.accumulationBytes = accumulationLayout.totalBytes();

      plan.buffers.totalUploadBytes =
        checkedAdd(plan.buffers.sceneUploadBytes, plan.buffers.initialPathStateBytes,
                   "GPU diffuse path-loop upload");

      std::uint64_t residentBytes = plan.buffers.sceneUploadBytes;
      residentBytes = checkedAdd(residentBytes, plan.buffers.activePathStateBytes,
                                 "GPU diffuse path-loop resident");
      residentBytes = checkedAdd(residentBytes, plan.buffers.nextPathStateBytes,
                                 "GPU diffuse path-loop resident");
      residentBytes =
        checkedAdd(residentBytes, plan.buffers.stepRecordBytes, "GPU diffuse path-loop resident");
      residentBytes = checkedAdd(residentBytes, plan.buffers.retainedIndexBytes,
                                 "GPU diffuse path-loop resident");
      residentBytes =
        checkedAdd(residentBytes, plan.buffers.accumulationBytes, "GPU diffuse path-loop resident");
      plan.buffers.totalResidentBytes = residentBytes;

      return plan;
    }

    void copyPrimaryPathDescriptor(GpuDiffusePathLoopLaunchParameters& parameters,
                                   const GpuPrimaryPathDescriptor& descriptor) {
      if (descriptor.mode != gpuPrimaryPathGenerationModePinhole &&
          descriptor.mode != gpuPrimaryPathGenerationModeOrthographic &&
          descriptor.mode != gpuPrimaryPathGenerationModeThinLens) {
        return;
      }

      const GpuRectilinearPrimaryPathDescriptor& rectilinear = descriptor.rectilinear;
      parameters.primaryPathGenerationMode = descriptor.mode;
      parameters.primaryPathSamplesPerPixel = rectilinear.samplesPerPixel;
      parameters.primaryPathSampleSeed = rectilinear.sampleSeed;
      parameters.primaryPathRequestedWidth = rectilinear.requestedWidth;
      parameters.primaryPathRequestedLeft = rectilinear.requestedLeft;
      parameters.primaryPathRequestedTop = rectilinear.requestedTop;
      parameters.primaryPathRequestedHeight = rectilinear.requestedHeight;
      parameters.primaryPathActualWidth = rectilinear.actualWidth;
      parameters.primaryPathActualLeft = rectilinear.actualLeft;
      parameters.primaryPathActualTop = rectilinear.actualTop;
      parameters.primaryPathActualHeight = rectilinear.actualHeight;
      parameters.primaryPathOrigin = rectilinear.originOrDirection;
      parameters.primaryPathTopLeft = rectilinear.topLeft;
      parameters.primaryPathRight = rectilinear.right;
      parameters.primaryPathDown = rectilinear.down;
      parameters.primaryPathLensRight = rectilinear.lensRight;
      parameters.primaryPathLensUp = rectilinear.lensUp;
      parameters.primaryPathForward = rectilinear.forward;
      parameters.primaryPathLensParameters = rectilinear.lensParameters;
    }

    template<typename Record>
    std::uint32_t assignGeometryRange(std::uint64_t& byteOffset, std::size_t count,
                                      std::uint32_t& countField, const char* label) {
      const std::string offsetLabel = std::string(label) + " byte offset";
      const std::string countLabel = std::string(label) + " count";
      const std::uint32_t result = checkedU32(byteOffset, offsetLabel.c_str());
      countField = checkedU32(count, countLabel.c_str());
      byteOffset =
        checkedAdd(byteOffset, checkedProduct(count, sizeof(Record), label), "geometry section");
      return result;
    }
  }

  bool GpuDiffusePathLoopLaunchPlan::generatesPrimaryPathsOnDevice() const {
    return parameters.primaryPathGenerationMode != gpuPrimaryPathGenerationModeHostPathStates;
  }

  GpuDiffusePathLoopLaunchPlan GpuDiffusePathLoopLaunchPlanner::plan(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const TracingAccumulationLayout& accumulationLayout,
    const GpuDiffusePathLoopSettings& settings) const {
    return planForInitialPathCount(scene, initialPathStates.size(), true, accumulationLayout,
                                   settings);
  }

  GpuDiffusePathLoopLaunchPlan GpuDiffusePathLoopLaunchPlanner::plan(
    const GpuTracingSceneSections& scene,
    const GpuDiffusePrimaryPathStateGeneration& primaryPathGeneration,
    const TracingAccumulationLayout& accumulationLayout,
    const GpuDiffusePathLoopSettings& settings) const {
    if (primaryPathGeneration.canGeneratePrimaryPathsOnDevice()) {
      GpuDiffusePathLoopLaunchPlan plan =
        planForInitialPathCount(scene, primaryPathGeneration.primaryPathDescriptor->pathCount(),
                                false, accumulationLayout, settings);
      copyPrimaryPathDescriptor(plan.parameters, *primaryPathGeneration.primaryPathDescriptor);
      return plan;
    }
    return this->plan(scene, primaryPathGeneration.pathStates, accumulationLayout, settings);
  }
}
