#pragma once

#include "core/geometry/Mesh.h"

#include <memory>
#include <utility>

namespace core {

  /**
    * Shared owned mesh payload for imported assets.
    *
    * Importers can keep one MeshAsset in their asset cache and create multiple
    * runtime primitives or instances from it without exposing raw Mesh pointers.
    */
  class MeshAsset {
  public:
    explicit MeshAsset(Mesh mesh)
        : m_mesh(std::make_shared<Mesh>(std::move(mesh))) {
    }

    explicit MeshAsset(std::shared_ptr<const Mesh> mesh)
        : m_mesh(std::move(mesh)) {
    }

    [[nodiscard]] const std::shared_ptr<const Mesh>& mesh() const {
      return m_mesh;
    }

  private:
    std::shared_ptr<const Mesh> m_mesh;
  };

}
