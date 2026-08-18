#include "world/import/StlSceneImporter.h"

#include "core/Exception.h"
#include "core/formats/stl/StlFile.h"
#include "core/geometry/Mesh.h"
#include "render/primitives/MeshPrimitive.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"

#include <QFileInfo>
#include <QJsonObject>

#include <fstream>
#include <memory>

namespace {
  world::ImportSourceMetadata sourceFor(const QString& filename) {
    return world::ImportSourceMetadata("stl", "STL mesh", filename);
  }

  QString warningSource(const QString& filename) {
    return filename;
  }
}

namespace world {

  QString StlSceneImporter::name() const {
    return "stl";
  }

  QStringList StlSceneImporter::supportedExtensions() const {
    return {"stl"};
  }

  ImportOptionSchemas StlSceneImporter::optionSchema() const {
    return {};
  }

  ImportResult StlSceneImporter::importFile(const QString& filename,
                                            const ImportOptions&) const {
    auto source = sourceFor(filename);

    std::ifstream input(filename.toStdString(), std::ios::binary);
    if (!input) {
      return ImportResult::failed({ImportDiagnostic::error("Unable to read import source", filename)},
                                  source);
    }

    Mesh mesh;
    try {
      core::formats::stl::StlFile file(input, mesh);
      source.properties = {
        {"encoding", file.encoding() == core::formats::stl::StlEncoding::Ascii ? "ascii"
                                                                                : "binary"},
        {"triangleCount", static_cast<int>(file.triangleCount())},
      };
    } catch (const Exception& error) {
      return ImportResult::failed(
        {ImportDiagnostic::error(QString::fromStdString(error.message()), filename)}, source);
    }

    auto root = std::make_unique<Group>();
    root->setName(QFileInfo(filename).completeBaseName());
    root->setMetadataValue("sourceFormat", "STL");
    root->setMetadataValue("sourcePath", filename);
    root->setMetadataValue("units", "scene units (STL is unitless)");
    root->setMetadataValue("normalMode", "flat");
    root->setMetadataValue("material", "default material (STL carries no material data)");
    root->setMetadataValue("triangleCount", static_cast<int>(mesh.faces().size()));

    auto primitive = std::make_shared<render::MeshPrimitive>(
      std::move(mesh), render::MeshPrimitive::NormalMode::Flat);
    auto compiled = std::make_unique<CompiledPrimitive>(primitive);
    compiled->setId(root->id() + ":compiled-geometry");
    compiled->setName(root->name().isEmpty() ? QString("STL Geometry") : root->name() + " Geometry");
    root->addChild(std::move(compiled));

    ImportResult result(std::move(root), source);
    result.addDiagnostic(ImportDiagnostic::warning(
      "STL is unitless; coordinates are imported as scene units.", warningSource(filename)));
    result.addDiagnostic(ImportDiagnostic::warning(
      "STL carries no material data; imported mesh uses the default material.",
      warningSource(filename)));

    auto provenance = ImportProvenance::fromSource(result.source());
    provenance.originalUnits = "unitless";
    provenance.category = QJsonObject{{"sourceFormat", "STL"}};
    result.setRootProvenance(provenance);
    return result;
  }

}

static bool dummy = world::SceneImporterRegistry::self().registerClass<world::StlSceneImporter>(
  "stl");
