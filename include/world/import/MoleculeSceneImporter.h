#pragma once

#include "core/formats/molecule/Molecule.h"
#include "world/import/SceneImporter.h"

#include <memory>

class Group;

namespace world {

  /**
    * Converts parsed molecular coordinates into editable world scene hierarchy.
    *
    * The compiler preserves the source model -> chain -> residue hierarchy as
    * generic Groups and emits simple atom spheres as visible leaves.
    */
  class MoleculeSceneCompiler {
  public:
    [[nodiscard]] std::unique_ptr<Group> compile(const molecule::Molecule& molecule,
                                                 const ImportSourceMetadata& source,
                                                 double atomRadius = 0.25) const;
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
