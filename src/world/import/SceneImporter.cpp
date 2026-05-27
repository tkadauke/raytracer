#include "world/import/SceneImporter.h"

#include "world/import/ImportedSceneDefaults.h"
#include "world/objects/Scene.h"

namespace world {

  SceneImporter::~SceneImporter() = default;

  ImportOptionSchemas SceneImporter::editableSourceParameters(const QString&,
                                                              const ImportOptions&) const {
    return {};
  }

  bool SceneImporter::wrapDirectImportInSourceAsset() const {
    return false;
  }

  bool SceneImporter::configureImportedRoot(Element& importedRoot,
                                            const ImportOptions& options) const {
    (void)importedRoot;
    (void)options;
    return false;
  }

  bool SceneImporter::configureImportedScene(Scene& scene, Element& importedRoot,
                                             const ImportOptions& options) const {
    (void)importedRoot;
    (void)options;
    ImportedSceneDefaults defaults;
    defaults.applyTo(scene);
    (void)defaults.frameCamera(scene);
    return true;
  }
}
