#pragma once

#include "core/formats/molecule/Molecule.h"
#include "world/import/SceneImporter.h"

#include <QString>

#include <memory>

class Group;

namespace world {

  /**
    * Converts parsed molecular coordinates into editable world scene hierarchy.
    *
    * The compiler preserves the source model -> chain -> residue hierarchy as
    * generic Groups and emits representation-specific visible leaves.
    */
  struct MoleculeSceneCompileOptions {
    QString representation{QStringLiteral("ball-and-stick")};
    QString colorScheme{QStringLiteral("element")};
    double atomRadius{0.25};
    double spaceFillingScale{1.0};
    double bondRadius{0.08};
    bool inferBondsWhenMissing{true};
    QString backboneMode{QStringLiteral("overlay")};
    double backboneWidth{0.35};
  };

  class MoleculeSceneCompiler {
  public:
    [[nodiscard]] std::unique_ptr<Group>
    compile(const molecule::Molecule& molecule, const ImportSourceMetadata& source,
            const MoleculeSceneCompileOptions& options = MoleculeSceneCompileOptions()) const;
  };

  /**
    * Imports PDB and PDBx/mmCIF coordinate files as grouped atom spheres.
    */
  class MoleculeSceneImporter : public SceneImporter {
  public:
    [[nodiscard]] QString name() const override;
    [[nodiscard]] QStringList supportedExtensions() const override;
    [[nodiscard]] ImportOptionSchemas optionSchema() const override;
    [[nodiscard]] ImportResult
    importFile(const QString& filename,
               const ImportOptions& options = ImportOptions()) const override;
  };

}
