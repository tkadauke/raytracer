#pragma once

#include "core/Color.h"
#include "core/formats/molecule/Molecule.h"

#include <memory>
#include <string>
#include <vector>

class Group;

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

  [[nodiscard]] std::vector<molecule::Bond>
  moleculeBondsForRendering(const molecule::Molecule& molecule,
                            const MoleculeRenderOptions& options = MoleculeRenderOptions());

  [[nodiscard]] std::unique_ptr<Group>
  buildBallAndStickMolecule(const molecule::Molecule& molecule,
                            const MoleculeRenderOptions& options = MoleculeRenderOptions());

}
