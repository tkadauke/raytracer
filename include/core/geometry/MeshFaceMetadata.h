#pragma once

namespace core {

  /**
    * Per-face provenance used by rasterization to decide whether material
    * sidedness is safe to turn into automatic face culling.
    */
  struct MeshFaceMetadata {
    enum class WindingReliability { Reliable, Unknown, Corrected };

    WindingReliability windingReliability{WindingReliability::Reliable};

    [[nodiscard]] bool safeForInferredCulling() const {
      return windingReliability == WindingReliability::Reliable;
    }
  };

}
