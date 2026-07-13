#include "world/import/ThreeMfSceneImporter.h"

#include "core/formats/threemf/ThreeMfModel.h"
#include "core/formats/threemf/ThreeMfPackage.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/textures/ConstantColorTexture.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QXmlStreamReader>

#include <memory>
#include <sstream>
#include <utility>

namespace world {
  namespace {
    QString modelPartName(const core::threemf::ThreeMfPackage& package) {
      if (package.contains("_rels/.rels")) {
        QXmlStreamReader xml(package.part("_rels/.rels"));
        while (xml.readNextStartElement()) {
          if (xml.name() == QStringLiteral("Relationships")) {
            while (xml.readNextStartElement()) {
              if (xml.name() == QStringLiteral("Relationship")) {
                const QString type = xml.attributes().value("Type").toString();
                const QString target = xml.attributes().value("Target").toString();
                if (type.contains("/3dmodel") && !target.isEmpty())
                  return core::threemf::normalizedPartName(target);
              }
              xml.skipCurrentElement();
            }
          } else {
            xml.skipCurrentElement();
          }
        }
      }

      if (package.contains("3D/3dmodel.model"))
        return "3D/3dmodel.model";

      for (const QString& part : package.partNames()) {
        if (part.endsWith(".model", Qt::CaseInsensitive))
          return part;
      }

      throw core::threemf::ThreeMfPackageError("3MF package does not contain a model part");
    }

    std::shared_ptr<render::Material>
    materialFor(const std::optional<core::threemf::MaterialResource>& resource) {
      if (!resource)
        return nullptr;

      auto material = std::make_shared<render::MatteMaterial>(
        std::make_shared<render::ConstantColorTexture>(resource->color));
      material->setAmbientCoefficient(1.0);
      material->setDiffuseCoefficient(0.75);
      return material;
    }

    render::MeshPrimitive::FaceMaterials faceMaterialsFor(const core::threemf::ObjectMesh& object) {
      render::MeshPrimitive::FaceMaterials materials;
      materials.reserve(object.faceMaterials.size());
      for (const auto& resource : object.faceMaterials)
        materials.push_back(materialFor(resource));
      return materials;
    }

    Matrix4d unitScaleMatrix(double scale) {
      return Matrix4d(Matrix3d::scale(scale, scale, scale));
    }

    std::shared_ptr<render::Primitive> primitiveFor(const core::threemf::ObjectMesh& object,
                                                    const core::threemf::BuildItem& item,
                                                    double unitScale) {
      auto mesh = std::make_shared<render::MeshPrimitive>(
        object.mesh, faceMaterialsFor(object), render::MeshPrimitive::NormalMode::Smooth);
      auto instance = std::make_shared<render::Instance>(mesh);
      instance->setMatrix(unitScaleMatrix(unitScale) * item.transform);
      return instance;
    }

    std::unique_ptr<Group> groupForBuildItem(const QString& filename,
                                             const core::threemf::Model& model,
                                             const core::threemf::BuildItem& item, int buildIndex) {
      const auto object = model.objects.find(item.objectId);
      if (object == model.objects.end()) {
        std::ostringstream message;
        message << "3MF build item references missing object " << item.objectId;
        throw core::threemf::ThreeMfModelError(message.str());
      }

      auto group = std::make_unique<Group>();
      group->setName(object->second.name.isEmpty() ? QString("3MF Object %1").arg(item.objectId)
                                                   : object->second.name);
      group->setMetadataValue("sourceFormat", "3MF");
      group->setMetadataValue("sourceFile", filename);
      group->setMetadataValue("sourceId",
                              QString("build/%1/object/%2").arg(buildIndex).arg(item.objectId));
      group->setMetadataValue("objectId", item.objectId);
      group->setMetadataValue("buildIndex", buildIndex);
      group->setMetadataValue("originalUnits", model.unitName());

      auto compiled = std::make_unique<CompiledPrimitive>(
        primitiveFor(object->second, item, model.unitScaleInMeters()));
      compiled->setId(QString("3mf-object-%1-build-%2").arg(item.objectId).arg(buildIndex));
      compiled->setName(group->name() + " Mesh");
      group->addChild(std::move(compiled));
      return group;
    }

    ImportSourceMetadata sourceMetadata(const QString& filename) {
      ImportSourceMetadata source;
      source.importerName = "3mf";
      source.formatName = "3MF core package";
      source.sourcePath = filename;
      source.properties = {{"container", "zip"}, {"model", "core"}};
      return source;
    }
  }

  QString ThreeMfSceneImporter::name() const {
    return "3mf";
  }

  QStringList ThreeMfSceneImporter::supportedExtensions() const {
    return {"3mf"};
  }

  ImportOptionSchemas ThreeMfSceneImporter::optionSchema() const {
    return {};
  }

  ImportResult ThreeMfSceneImporter::importFile(const QString& filename,
                                                const ImportOptions&) const {
    const ImportSourceMetadata source = sourceMetadata(filename);

    try {
      const auto package = core::threemf::ThreeMfPackage::read(filename);
      const QString modelPart = modelPartName(package);
      const auto model = core::threemf::ThreeMfModelParser().parse(package.part(modelPart));

      auto root = std::make_unique<Group>();
      root->setName(QFileInfo(filename).completeBaseName());
      root->setMetadataValue("sourceFormat", "3MF");
      root->setMetadataValue("sourcePath", filename);
      root->setMetadataValue("modelPart", modelPart);
      root->setMetadataValue("originalUnits", model.unitName());
      root->setMetadataValue("unitScaleInMeters", model.unitScaleInMeters());

      int buildIndex = 0;
      for (const auto& item : model.buildItems)
        root->addChild(groupForBuildItem(filename, model, item, buildIndex++));

      ImportResult result(std::move(root), source);
      result.setRootProvenance(ImportProvenance::fromSource(source));
      return result;
    } catch (const std::exception& error) {
      return ImportResult::failed({ImportDiagnostic::error(error.what(), filename)}, source);
    }
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::ThreeMfSceneImporter>("3mf");
