#include "world/import/GCodeSceneImporter.h"

#include "core/formats/gcode/GCodePathCompiler.h"
#include "core/formats/gcode/GCodeParser.h"
#include "core/geometry/AttributeColorMap.h"
#include "render/primitives/Curve.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/Group.h"
#include "world/objects/Scene.h"

#include <QFileInfo>
#include <QJsonObject>

#include <fstream>
#include <map>
#include <memory>

namespace {
  QString moveTypeName(GCodePathMoveType moveType) {
    return moveType == GCodePathMoveType::Extrusion ? QStringLiteral("extrusion")
                                                    : QStringLiteral("travel");
  }

  QString featureName(const std::string& featureType) {
    return featureType.empty() ? QStringLiteral("Unclassified")
                               : QString::fromStdString(featureType);
  }

  QJsonObject sourceMetadata(const QString& sourcePath) {
    return QJsonObject{
      {"sourceFormat", "gcode"},
      {"sourcePath", sourcePath},
    };
  }

  Group* childGroup(Group& parent, std::map<QString, Group*>& groups, const QString& key,
                    const QString& name, const QJsonObject& metadata) {
    auto found = groups.find(key);
    if (found != groups.end())
      return found->second;

    auto group = std::make_unique<Group>();
    group->setName(name);
    group->setMetadata(metadata);
    Group* raw = group.get();
    parent.addChild(std::move(group));
    groups.emplace(key, raw);
    return raw;
  }

  std::shared_ptr<render::Curve> curveForPath(const GCodePath& path) {
    auto curve =
      std::make_shared<render::Curve>(path.polyline, 0.0, render::Curve::TessellationMode::Ribbon);
    auto colorMap = core::AttributeColorMap::categorical("move_type");
    colorMap.setCategoryColor(std::string("extrusion"), Colord(0.95, 0.45, 0.1));
    colorMap.setCategoryColor(std::string("travel"), Colord(0.1, 0.45, 0.95));
    curve->setSegmentColorMap(colorMap);
    curve->setMetadataValue("source.format", "gcode");
    curve->setMetadataValue("gcode.moveType", moveTypeName(path.moveType).toStdString());
    curve->setMetadataValue("gcode.tool", std::to_string(path.tool));
    curve->setMetadataValue("gcode.layerIndex", std::to_string(path.layerIndex));
    curve->setMetadataValue("gcode.lineStart", std::to_string(path.lineStart));
    curve->setMetadataValue("gcode.lineEnd", std::to_string(path.lineEnd));
    if (!path.featureType.empty())
      curve->setMetadataValue("gcode.featureType", path.featureType);
    return curve;
  }
}

namespace world {

  QString GCodeSceneImporter::name() const {
    return QStringLiteral("gcode");
  }

  QStringList GCodeSceneImporter::supportedExtensions() const {
    return {QStringLiteral("gcode"), QStringLiteral("gco"), QStringLiteral("gc")};
  }

  ImportOptionSchemas GCodeSceneImporter::optionSchema() const {
    return {};
  }

  ImportResult GCodeSceneImporter::importFile(const QString& filename, const ImportOptions&) const {
    ImportSourceMetadata source;
    source.importerName = name();
    source.formatName = QStringLiteral("G-code toolpath");
    source.sourcePath = filename;

    std::ifstream input(filename.toStdString());
    if (!input) {
      return ImportResult::failed(
        {ImportDiagnostic::error("Unable to read G-code source", filename)}, source);
    }

    const GCodeProgram program = GCodeParser().parse(input);
    auto scene = std::make_unique<Scene>();
    scene->setName(QFileInfo(filename).baseName());

    auto root = std::make_unique<Group>();
    root->setName(QStringLiteral("G-code Toolpath"));
    root->setMetadata(sourceMetadata(filename));

    const GCodePathProgram paths = GCodePathCompiler().compile(program);
    for (const auto& layer : paths.layers) {
      auto layerGroup = std::make_unique<Group>();
      layerGroup->setName(QStringLiteral("Layer %1").arg(layer.index));
      layerGroup->setStepIndex(layer.index);
      layerGroup->setLayerIndex(layer.index);
      layerGroup->setLabel(QStringLiteral("Layer %1").arg(layer.index));
      layerGroup->setMetadataValue("sourceFormat", "gcode");
      layerGroup->setMetadataValue("z", layer.z);
      layerGroup->setMetadataValue("lineStart", layer.lineStart);
      layerGroup->setMetadataValue("lineEnd", layer.lineEnd);
      if (!layer.comment.empty())
        layerGroup->setMetadataValue("comment", QString::fromStdString(layer.comment));

      std::map<QString, Group*> toolGroups;
      std::map<QString, Group*> featureGroups;
      for (const auto& path : layer.paths) {
        const QString toolKey = QString::number(path.tool);
        Group* toolGroup =
          childGroup(*layerGroup, toolGroups, toolKey, QStringLiteral("Tool T%1").arg(path.tool),
                     QJsonObject{{"sourceFormat", "gcode"}, {"tool", path.tool}});

        const QString feature = featureName(path.featureType);
        const QString featureKey = toolKey + QStringLiteral("/") + feature;
        Group* featureGroup =
          childGroup(*toolGroup, featureGroups, featureKey, feature,
                     QJsonObject{{"sourceFormat", "gcode"}, {"featureType", feature}});

        auto surface = std::make_unique<CompiledPrimitive>(curveForPath(path));
        surface->setName(QStringLiteral("%1 path").arg(moveTypeName(path.moveType)));
        surface->setMetadata(QJsonObject{
          {"sourceFormat", "gcode"},
          {"moveType", moveTypeName(path.moveType)},
          {"tool", path.tool},
          {"layerIndex", path.layerIndex},
          {"lineStart", path.lineStart},
          {"lineEnd", path.lineEnd},
          {"featureType", feature},
        });
        featureGroup->addChild(std::move(surface));
      }

      root->addChild(std::move(layerGroup));
    }

    scene->addChild(std::move(root));
    ImportResult result(std::move(scene), source);
    for (const auto& diagnostic : program.diagnostics.entries()) {
      result.addDiagnostic(ImportDiagnostic::warning(QString::fromStdString(diagnostic.message),
                                                     filename, diagnostic.lineNumber));
    }
    return result;
  }

}

static bool dummy =
  world::SceneImporterRegistry::self().registerClass<world::GCodeSceneImporter>("gcode");
