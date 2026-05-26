#pragma once

#include "core/Color.h"
#include "core/geometry/Mesh.h"
#include "core/math/Matrix.h"

#include <QByteArray>
#include <QString>

#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace core::threemf {

  enum class Unit { Micron, Millimeter, Centimeter, Inch, Foot, Meter };

  struct MaterialResource {
    int id{0};
    int index{0};
    QString name;
    Colord color{Colord::white()};
  };

  struct ObjectMesh {
    int id{0};
    QString name;
    Mesh mesh;
    std::vector<std::optional<MaterialResource>> faceMaterials;
  };

  struct BuildItem {
    int objectId{0};
    Matrix4d transform;
  };

  struct Model {
    Unit unit{Unit::Millimeter};
    std::map<int, ObjectMesh> objects;
    std::map<std::pair<int, int>, MaterialResource> materials;
    std::vector<BuildItem> buildItems;

    [[nodiscard]] double unitScaleInMeters() const;
    [[nodiscard]] QString unitName() const;
  };

  class ThreeMfModelError : public std::runtime_error {
  public:
    explicit ThreeMfModelError(const std::string& message);
  };

  /**
    * Parses the 3MF core model XML subset used by mesh-only interchange:
    * units, base material colors, object meshes, triangle material references,
    * build items, and 3MF affine transforms.
    */
  class ThreeMfModelParser {
  public:
    [[nodiscard]] Model parse(const QByteArray& xml) const;
  };

}
