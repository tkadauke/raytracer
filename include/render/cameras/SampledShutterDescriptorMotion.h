#pragma once

#include "render/GpuPrimaryPathDescriptor.h"
#include "core/math/Matrix.h"
#include "core/math/Vector.h"

#include <optional>

namespace render {
  class Camera;

  namespace detail {
    struct SampledShutterLookAtDescriptorMotion {
      Vector3d positionAtOpen;
      Vector3d positionAtClose;
      Vector3d targetAtOpen;
      Vector3d targetAtClose;

      [[nodiscard]] Vector3d positionDelta() const;
      [[nodiscard]] Vector3d targetDelta() const;
    };

    struct SampledShutterDescriptorMotion {
      Matrix4d matrixAtOpen;
      Matrix4d matrixAtClose;
    };

    // Shared motion record for lens-based cameras (ThinLens, TiltShift, Orthographic): identical
    // fields and planeMatrix() logic appear in all three camera types.
    struct LensDescriptorMotion {
      std::uint32_t motionMode{gpuPrimaryPathMotionModeOriginDelta};
      Matrix4d matrixAtOpen;
      Vector3d originOrPosition;
      Vector3d motionOriginOrPositionDelta{Vector3d::null};
      Vector3d target{Vector3d::null};
      Vector3d targetDelta{Vector3d::null};

      [[nodiscard]] Matrix4d planeMatrix() const {
        if (motionMode == gpuPrimaryPathMotionModeLookAt) {
          return Matrix4d();
        }
        return matrixAtOpen;
      }
    };

    // Motion record for point-source cameras (FishEye, Equirectangular) whose origin is
    // simply the camera position (matrix translation vector) rather than an eye offset.
    struct PointSourceDescriptorMotion {
      Matrix4d matrix;
      std::uint32_t motionMode{gpuPrimaryPathMotionModeOriginDelta};
      Vector3d motionOriginDelta{Vector3d::null};
      Vector3d target{Vector3d::null};
      Vector3d targetDelta{Vector3d::null};
    };

    [[nodiscard]] std::optional<SampledShutterLookAtDescriptorMotion>
    sampledLookAtShutterMotion(const Camera& camera);

    [[nodiscard]] std::optional<SampledShutterDescriptorMotion>
    sampledStableBasisShutterMotion(const Camera& camera);

    // Resolves the full shutter motion descriptor for a lens-based camera (ThinLensCamera,
    // TiltShiftCamera) given the eye-to-plane distance and the camera's fixed-shutter matrix
    // (result of Camera::fixedShutterGpuCameraMatrix(), passed from the protected camera context).
    [[nodiscard]] std::optional<LensDescriptorMotion>
    lensDescriptorMotion(const Camera& camera, double distance,
                         const std::optional<Matrix4d>& fixedShutterMatrix);

    // Resolves the full shutter motion descriptor for a point-source camera (FishEyeCamera,
    // EquirectangularCamera) where the ray origin equals the camera position.
    // fixedShutterMatrix is the result of Camera::fixedShutterGpuCameraMatrix().
    [[nodiscard]] std::optional<PointSourceDescriptorMotion>
    pointSourceDescriptorMotion(const Camera& camera,
                                const std::optional<Matrix4d>& fixedShutterMatrix);
  }
}
