#pragma once

#include "render/cameras/SampledShutterDescriptorMotion.h"
#include "render/GpuFloat4.h"
#include "render/GpuPrimaryPathDescriptor.h"
#include "render/samplers/Sampler.h"
#include "render/viewplanes/ViewPlane.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace render::detail {
  inline std::uint32_t checkedU32(std::uint64_t value, const char* label) {
    if (value > std::numeric_limits<std::uint32_t>::max()) {
      throw std::overflow_error(std::string(label) + " exceeds GPU 32-bit count range");
    }
    return static_cast<std::uint32_t>(value);
  }

  // Shared first guard clause for every camera's gpuPrimaryPathDescriptor()
  // override: a GPU descriptor needs a view plane with a configured sampler
  // emitting at least one sample per pixel.
  inline bool hasValidGpuPrimaryPathSampler(const std::shared_ptr<ViewPlane>& plane) {
    return plane && plane->sampler() && plane->sampler()->numSamples() > 0;
  }

  // Shared second guard clause: the clamped renderable rect must not be
  // empty (e.g. a fully letterboxed FitExact rect at this render size).
  inline bool isEmptyGpuPrimaryPathRect(const Recti& actual) {
    return actual.width() <= 0 || actual.height() <= 0;
  }

  inline void checkGpuPathCount(const Recti& actual, int numSamples) {
    const std::uint64_t pixelCount =
      static_cast<std::uint64_t>(actual.width()) * static_cast<std::uint64_t>(actual.height());
    const std::uint64_t pathCount = pixelCount * static_cast<std::uint64_t>(numSamples);
    if (pixelCount != 0 && pathCount / pixelCount != static_cast<std::uint64_t>(numSamples)) {
      throw std::overflow_error("GPU camera primary path count overflows");
    }
    (void)checkedU32(pathCount, "GPU camera primary path count");
  }

  inline void fillGpuDescriptorViewport(render::GpuRectilinearPrimaryPathDescriptor& rec,
                                        const Recti& rect, const Recti& actual, int numSamples,
                                        std::uint32_t sampleSeed) {
    rec.requestedLeft = rect.left();
    rec.requestedTop = rect.top();
    rec.requestedWidth =
      checkedU32(static_cast<std::uint64_t>(rect.width()), "GPU camera requested width");
    rec.requestedHeight =
      checkedU32(static_cast<std::uint64_t>(rect.height()), "GPU camera requested height");
    rec.actualLeft = actual.left();
    rec.actualTop = actual.top();
    rec.actualWidth =
      checkedU32(static_cast<std::uint64_t>(actual.width()), "GPU camera actual width");
    rec.actualHeight =
      checkedU32(static_cast<std::uint64_t>(actual.height()), "GPU camera actual height");
    rec.samplesPerPixel =
      checkedU32(static_cast<std::uint64_t>(numSamples), "GPU camera samples per pixel");
    rec.sampleSeed = sampleSeed;
  }

  inline Vector3d eyeOriginForMatrix(const Matrix4d& matrix, double distance) {
    return matrix.transformPoint(Vector3d(0.0, 0.0, -distance));
  }

  // Fill the topLeft/right/down rectilinear plane basis from a view plane's
  // pixel grid. Used by all cameras that project through a flat view plane.
  inline void fillGpuDescriptorPlane(GpuRectilinearPrimaryPathDescriptor& rec,
                                     const ViewPlane& plane) {
    rec.topLeft = gpuFloat4(plane.pixelAt(0.0, 0.0), 1.0f);
    rec.right = gpuFloat4(plane.pixelAt(1.0, 0.0) - plane.pixelAt(0.0, 0.0), 0.0f);
    rec.down = gpuFloat4(plane.pixelAt(0.0, 1.0) - plane.pixelAt(0.0, 0.0), 0.0f);
  }

  // Fill the right/down/forward rectilinear basis from the camera matrix's
  // local axes. Used by cameras that encode orientation via a matrix (fish-eye,
  // spherical, equirectangular) rather than a pixel-grid plane.
  inline void fillGpuDescriptorMatrixBasis(GpuRectilinearPrimaryPathDescriptor& rec,
                                           const Matrix4d& matrix) {
    rec.right = gpuFloat4(matrix.transformDirection(Vector3d(1.0, 0.0, 0.0)), 0.0f);
    rec.down = gpuFloat4(matrix.transformDirection(Vector3d(0.0, 1.0, 0.0)), 0.0f);
    rec.forward = gpuFloat4(matrix.transformDirection(Vector3d(0.0, 0.0, 1.0)), 0.0f);
  }

  // Fill the motion fields plus the matrix basis from a point-source camera's
  // resolved motion descriptor. Used by the point-source cameras (fish-eye,
  // equirectangular) whose ray origin is simply the camera position; each
  // still sets `descriptor.mode`/`lensParameters` itself afterward.
  inline void fillGpuDescriptorPointSourceMotion(GpuRectilinearPrimaryPathDescriptor& rec,
                                                 const PointSourceDescriptorMotion& motion) {
    rec.motionMode = motion.motionMode;
    rec.originOrDirection = gpuFloat4(motion.matrix.translationVector(), 1.0f);
    rec.motionOriginDelta = gpuFloat4(motion.motionOriginDelta, 0.0f);
    rec.motionTarget = gpuFloat4(motion.target, 1.0f);
    rec.motionTargetDelta = gpuFloat4(motion.targetDelta, 0.0f);
    fillGpuDescriptorMatrixBasis(rec, motion.matrix);
  }
}
