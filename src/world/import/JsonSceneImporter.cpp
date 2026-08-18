#include "world/import/JsonSceneImporter.h"

#include "world/import/SceneImporterRegistry.h"
#include "world/objects/Scene.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QVariant>

#include <exception>

namespace world {

  QString JsonSceneImporter::name() const {
    return "json";
  }

  QStringList JsonSceneImporter::supportedExtensions() const {
    return {"rtjson"};
  }

  ImportOptionSchemas JsonSceneImporter::optionSchema() const {
    return {};
  }

  ImportResult JsonSceneImporter::importFile(const QString& filename,
                                             const ImportOptions&) const {
    const ImportSourceMetadata source(name(), "Raytracer scene JSON", filename);

    QFile file(filename);
    if (!file.open(QIODevice::ReadOnly)) {
      return ImportResult::failed({ImportDiagnostic::error("Unable to read import source", filename)},
                                  source);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
      return ImportResult::failed(
        {ImportDiagnostic::error(
          QString("Unable to parse import JSON: %1").arg(parseError.errorString()), filename)},
        source);
    }
    if (!document.isObject()) {
      return ImportResult::failed(
        {ImportDiagnostic::error("Import JSON must contain an object", filename)}, source);
    }

    try {
      auto scene = std::make_unique<Scene>();
      scene->setProperty("_sourceFile", filename);
      scene->read(document.object());
      scene->setProperty("_sourceFile", QVariant());
      scene->resolveElementReferences();
      return ImportResult(std::move(scene), source);
    } catch (const std::exception& error) {
      return ImportResult::failed({ImportDiagnostic::error(error.what(), filename)}, source);
    }
  }

}

static bool dummy = world::SceneImporterRegistry::self().registerClass<world::JsonSceneImporter>(
  "json");
