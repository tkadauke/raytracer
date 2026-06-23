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
  }

  GpuDiffusePathLoopLaunchPlan GpuDiffusePathLoopLaunchPlanner::plan(
    const GpuTracingSceneSections& scene,
    const std::vector<GpuDiffusePathStateRecord>& initialPathStates,
    const TracingAccumulationLayout& accumulationLayout,
    const GpuDiffusePathLoopSettings& settings) const {
    if (settings.maxDepth == 0) {
      throw std::invalid_argument("GPU diffuse path-loop launch requires positive max depth");
    }

    accumulationLayout.validate();
    const std::uint64_t initialPathCount = initialPathStates.size();
    const std::uint64_t maxDepth = settings.maxDepth;

    GpuDiffusePathLoopLaunchPlan plan;
    plan.parameters.layoutVersion = gpuDiffusePathLoopLaunchLayoutVersion;
    plan.parameters.maxDepth = settings.maxDepth;
    plan.parameters.russianRouletteDepth = settings.russianRouletteDepth;
    plan.parameters.directLightSamples = settings.directLightSamples;
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
    plan.parameters.bvhNodeCount = checkedU32(scene.geometry.bvh.size(), "BVH node count");
    plan.parameters.primitiveCount =
      checkedU32(scene.geometry.primitives.size(), "primitive count");
    plan.parameters.transformCount =
      checkedU32(scene.geometry.transforms.size(), "transform count");

    plan.sceneUpload = scene.uploadBytes();
    plan.buffers.sceneUploadBytes = plan.sceneUpload.size();
    plan.parameters.sceneUploadBytes = checkedU32(plan.sceneUpload.size(), "scene upload bytes");
    plan.buffers.initialPathStateBytes = pathStateBytes(initialPathCount, "initial path state");
    plan.buffers.activePathStateBytes = pathStateBytes(initialPathCount, "active path state");
    plan.buffers.nextPathStateBytes = pathStateBytes(initialPathCount, "next path state");
    plan.buffers.stepRecordBytes =
      checkedProduct(checkedProduct(initialPathCount, maxDepth, "path-step record count"),
                     sizeof(GpuDiffusePathStepRecord), "path-step record");
    plan.buffers.retainedIndexBytes =
      checkedProduct(initialPathCount, sizeof(std::uint32_t), "retained path index");
    plan.buffers.accumulationBytes = accumulationLayout.totalBytes();

    plan.buffers.totalUploadBytes =
      checkedAdd(plan.buffers.sceneUploadBytes, plan.buffers.initialPathStateBytes,
                 "GPU diffuse path-loop upload");

    std::uint64_t residentBytes = plan.buffers.sceneUploadBytes;
    residentBytes = checkedAdd(residentBytes, plan.buffers.activePathStateBytes,
                               "GPU diffuse path-loop resident");
    residentBytes =
      checkedAdd(residentBytes, plan.buffers.nextPathStateBytes, "GPU diffuse path-loop resident");
    residentBytes =
      checkedAdd(residentBytes, plan.buffers.stepRecordBytes, "GPU diffuse path-loop resident");
    residentBytes =
      checkedAdd(residentBytes, plan.buffers.retainedIndexBytes, "GPU diffuse path-loop resident");
    residentBytes =
      checkedAdd(residentBytes, plan.buffers.accumulationBytes, "GPU diffuse path-loop resident");
    plan.buffers.totalResidentBytes = residentBytes;

    return plan;
  }
}
