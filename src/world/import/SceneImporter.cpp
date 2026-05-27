#include "world/import/SceneImporter.h"

namespace world {

  SceneImporter::~SceneImporter() = default;

  ImportOptionSchemas SceneImporter::editableSourceParameters(const QString&,
                                                              const ImportOptions&) const {
    return {};
  }

  bool SceneImporter::wrapDirectImportInSourceAsset() const {
    return false;
  }

  bool SceneImporter::configureImportedScene(Scene& scene, Element& importedRoot,
                                             const ImportOptions& options) const {
    (void)scene;
    (void)importedRoot;
    (void)options;
    return false;
  }
}
