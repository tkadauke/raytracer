#pragma once

#include "core/Color.h"
#include "core/formats/molecule/Molecule.h"

#include <memory>
#include <string>
#include <vector>

class Element;
class Group;
class PhongMaterial;

namespace world {

  struct MoleculeElementStyle {
    Colord color;
    double displayRadius{0.3};
    double covalentRadius{0.7};
  };

  struct MoleculeRenderOptions {
    double atomRadiusScale{0.3};
    double bondRadius{0.08};
    double bondInferenceScale{1.25};
    bool inferBondsWhenMissing{true};
  };

  [[nodiscard]] MoleculeElementStyle moleculeElementStyle(const std::string& element);

  /**
    * Builds a PhongMaterial with the shared molecule rendering defaults
    * (name, specular coefficient, exponent) and a ConstantColorTexture set
    * to @p color as its diffuse texture.
    *
    * When @p parent is null, the material and texture are constructed
    * without a Qt parent and the texture is registered as a scene-graph
    * child of the material via Element::addChild (matching how imported
    * molecule elements need to serialize/edit their material tree).
    *
    * When @p parent is given, the material is constructed with @p parent
    * as its Qt parent and the texture is parented to the material
    * directly, without being added as a scene-graph child (matching the
    * generated ball-and-stick builder, where the material/texture are
    * Qt-owned implementation detail, not user-editable scene nodes).
    */
  [[nodiscard]] std::unique_ptr<PhongMaterial>
  makeMoleculeMaterial(const Colord& color, Element* parent = nullptr);

  [[nodiscard]] std::vector<molecule::Bond>
  moleculeBondsForRendering(const molecule::Molecule& molecule,
                            const MoleculeRenderOptions& options = MoleculeRenderOptions());

  [[nodiscard]] std::unique_ptr<Group>
  buildBallAndStickMolecule(const molecule::Molecule& molecule,
                            const MoleculeRenderOptions& options = MoleculeRenderOptions());

}
