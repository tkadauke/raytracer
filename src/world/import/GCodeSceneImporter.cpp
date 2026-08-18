#include "world/import/GCodeSceneImporter.h"

#include "core/formats/gcode/GCodePathCompiler.h"
#include "core/formats/gcode/GCodeParser.h"
#include "core/util/QStringUtil.h"
#include "core/geometry/AttributeColorMap.h"
#include "render/primitives/Curve.h"
#include "world/import/SceneImporterRegistry.h"
#include "world/objects/CompiledPrimitive.h"
#include "world/objects/DirectionalLight.h"
#include "world/objects/Group.h"
#include "world/objects/PinholeCamera.h"
#include "world/objects/Scene.h"

#include <QFileInfo>
#include <QJsonObject>
#include <QVariant>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>

namespace {
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

  enum class VisualizationMode { MoveType, Layer, Tool, Speed, Temperature, ExtrusionTravel };

  struct ScalarRange {
    double minimum = std::numeric_limits<double>::infinity();
    double maximum = -std::numeric_limits<double>::infinity();

    void include(double value) {
      minimum = std::min(minimum, value);
      maximum = std::max(maximum, value);
    }

    [[nodiscard]] bool valid() const {
      return std::isfinite(minimum) && std::isfinite(maximum);
    }
  };

  struct GCodeImportSettings {
    VisualizationMode visualizationMode = VisualizationMode::MoveType;
    bool hideTravel = false;
    std::optional<int> layer;
    bool cumulativeLayers = false;
  };

  struct GCodePathStats {
    ScalarRange speed;
    ScalarRange temperature;
  };

  QString normalizedOption(QString value) {
    value = value.trimmed().toLower();
    value.remove('_');
    value.remove('-');
    value.remove(' ');
    return value;
  }

  bool optionBool(const world::ImportOptions& options, const QString& name, bool fallback) {
    const QVariant value = options.value(name, fallback);
    if (value.typeId() == QMetaType::Bool)
      return value.toBool();
    const QString text = value.toString().trimmed().toLower();
    if (text == "true" || text == "1" || text == "yes" || text == "on")
      return true;
    if (text == "false" || text == "0" || text == "no" || text == "off")
      return false;
    return fallback;
  }

  std::optional<int> optionInt(const world::ImportOptions& options, const QString& name) {
    if (!options.contains(name))
      return std::nullopt;
    bool ok = false;
    const int value = options.value(name).toInt(&ok);
    if (!ok)
      return std::nullopt;
    return value;
  }

  VisualizationMode visualizationModeFor(const world::ImportOptions& options) {
    const QString normalized =
      normalizedOption(options.value("visualization", "move_type").toString());
    if (normalized == "layer" || normalized == "layers")
      return VisualizationMode::Layer;
    if (normalized == "tool" || normalized == "tools")
      return VisualizationMode::Tool;
    if (normalized == "speed" || normalized == "feedrate")
      return VisualizationMode::Speed;
    if (normalized == "temperature" || normalized == "temp" || normalized == "nozzletemperature")
      return VisualizationMode::Temperature;
    if (normalized == "extrusiontravel" || normalized == "movetype" || normalized == "move")
      return VisualizationMode::ExtrusionTravel;
    return VisualizationMode::MoveType;
  }

  GCodeImportSettings settingsFor(const world::ImportOptions& options) {
    GCodeImportSettings settings;
    settings.visualizationMode = visualizationModeFor(options);
    settings.hideTravel = optionBool(options, "hide_travel", false);
    settings.layer = optionInt(options, "layer");
    settings.cumulativeLayers = optionBool(options, "cumulative_layers", false);
    return settings;
  }

  bool layerVisible(const GCodePathLayer& layer, const GCodeImportSettings& settings) {
    if (!settings.layer)
      return true;
    return settings.cumulativeLayers ? layer.index <= *settings.layer
                                     : layer.index == *settings.layer;
  }

  void includePathStats(const GCodePath& path, GCodePathStats& stats) {
    for (std::size_t i = 0; i != path.polyline.segmentCount(); ++i) {
      if (const auto* speed = path.polyline.segmentAttributeAs<double>(i, "speed"))
        stats.speed.include(*speed);
      if (const auto* temperature = path.polyline.segmentAttributeAs<double>(i, "temperature")) {
        stats.temperature.include(*temperature);
      }
    }
  }

  GCodePathStats statsFor(const GCodePathProgram& paths) {
    GCodePathStats stats;
    for (const auto& layer : paths.layers) {
      for (const auto& path : layer.paths) {
        includePathStats(path, stats);
      }
    }
    return stats;
  }

  BoundingBoxd boundsFor(const GCodePathProgram& paths) {
    BoundingBoxd bounds;
    for (const auto& layer : paths.layers) {
      for (const auto& path : layer.paths) {
        bounds.include(path.polyline.bounds());
      }
    }
    return bounds;
  }

  void addDefaultView(Scene& scene, const GCodePathProgram& paths) {
    const BoundingBoxd bounds = boundsFor(paths);
    const Vector3d center = bounds.isValid() ? bounds.center() : Vector3d::null;
    const Vector3d size = bounds.isValid() ? bounds.size() : Vector3d(20.0, 20.0, 1.0);
    const double distance = std::max({size.x(), size.y(), size.z(), 10.0}) * 2.4;

    auto camera = std::make_unique<PinholeCamera>();
    camera->setId("gcode-camera");
    camera->setName(QStringLiteral("G-code Camera"));
    camera->setPosition(center + Vector3d(0.0, 0.0, -distance));
    camera->setTarget(center);
    camera->setDistance(distance);
    camera->setZoom(0.1);
    scene.addChild(std::move(camera));

    auto light = std::make_unique<DirectionalLight>();
    light->setId("gcode-light");
    light->setName(QStringLiteral("G-code Light"));
    light->setDirection(Vector3d(-0.3, -0.5, -1.0));
    scene.addChild(std::move(light));
  }

  double rangeMaximum(const ScalarRange& range, double fallback) {
    return range.valid() ? range.maximum : fallback;
  }

  double rangeMinimum(const ScalarRange& range, double fallback) {
    return range.valid() ? range.minimum : fallback;
  }

  core::AttributeColorMap colorMapForMode(VisualizationMode mode, const GCodePathStats& stats) {
    if (mode == VisualizationMode::Layer)
      return core::AttributeColorMap::categorical("layer_index");
    if (mode == VisualizationMode::Tool)
      return core::AttributeColorMap::categorical("tool");
    if (mode == VisualizationMode::Speed) {
      return core::AttributeColorMap::scalar("speed", rangeMinimum(stats.speed, 0.0),
                                             rangeMaximum(stats.speed, 1.0),
                                             Colord(0.05, 0.25, 0.95), Colord(1.0, 0.15, 0.05));
    }
    if (mode == VisualizationMode::Temperature) {
      return core::AttributeColorMap::scalar("temperature", rangeMinimum(stats.temperature, 180.0),
                                             rangeMaximum(stats.temperature, 240.0),
                                             Colord(0.0, 0.55, 1.0), Colord(1.0, 0.2, 0.0));
    }

    auto colorMap = core::AttributeColorMap::categorical("move_type");
    colorMap.setCategoryColor(std::string("extrusion"), Colord(0.95, 0.45, 0.1));
    colorMap.setCategoryColor(std::string("travel"), Colord(0.1, 0.45, 0.95));
    return colorMap;
  }

  std::shared_ptr<render::Curve> curveForPath(const GCodePath& path, const GCodePathStats& stats,
                                              VisualizationMode visualizationMode) {
    auto curve =
      std::make_shared<render::Curve>(path.polyline, 0.25, render::Curve::TessellationMode::Tube);
    curve->setSegmentColorMap(colorMapForMode(visualizationMode, stats));
    curve->setMetadataValue("source.format", "gcode");
    curve->setMetadataValue("gcode.moveType", moveTypeName(path.moveType));
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
    return {
      ImportOptionSchema{QStringLiteral("visualization"),
                         ImportOptionType::Choice,
                         QStringLiteral("Visualization"),
                         QStringLiteral("G-code path color mode"),
                         QStringLiteral("move_type"),
                         false,
                         {QStringLiteral("move_type"), QStringLiteral("layer"),
                          QStringLiteral("tool"), QStringLiteral("speed"),
                          QStringLiteral("temperature"), QStringLiteral("extrusion_travel")}},
      ImportOptionSchema{QStringLiteral("hide_travel"),
                         ImportOptionType::Boolean,
                         QStringLiteral("Hide travel moves"),
                         QStringLiteral("Skip non-extruding travel moves during import"),
                         false,
                         false,
                         {}},
      ImportOptionSchema{QStringLiteral("layer"),
                         ImportOptionType::Integer,
                         QStringLiteral("Layer"),
                         QStringLiteral("Only import a single G-code print layer"),
                         QVariant(),
                         false,
                         {}},
      ImportOptionSchema{QStringLiteral("cumulative_layers"),
                         ImportOptionType::Boolean,
                         QStringLiteral("Cumulative layers"),
                         QStringLiteral("Import layers cumulatively through the selected layer"),
                         false,
                         false,
                         {}}};
  }

  ImportResult GCodeSceneImporter::importFile(const QString& filename,
                                              const ImportOptions& options) const {
    const ImportSourceMetadata source(name(), QStringLiteral("G-code toolpath"), filename);

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

    const GCodeImportSettings settings = settingsFor(options);
    const GCodePathProgram paths = GCodePathCompiler().compile(program);
    const GCodePathStats stats = statsFor(paths);
    addDefaultView(*scene, paths);
    for (const auto& layer : paths.layers) {
      if (!layerVisible(layer, settings))
        continue;

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
        if (settings.hideTravel && path.moveType == GCodePathMoveType::Travel)
          continue;

        const QString toolKey = QString::number(path.tool);
        Group* toolGroup =
          childGroup(*layerGroup, toolGroups, toolKey, QStringLiteral("Tool T%1").arg(path.tool),
                     QJsonObject{{"sourceFormat", "gcode"}, {"tool", path.tool}});

        const QString feature = featureName(path.featureType);
        const QString featureKey = toolKey + QStringLiteral("/") + feature;
        Group* featureGroup =
          childGroup(*toolGroup, featureGroups, featureKey, feature,
                     QJsonObject{{"sourceFormat", "gcode"}, {"featureType", feature}});

        auto surface = std::make_unique<CompiledPrimitive>(
          curveForPath(path, stats, settings.visualizationMode));
        surface->setName(QStringLiteral("%1 path").arg(qstr(moveTypeName(path.moveType))));
        surface->setMetadata(QJsonObject{
          {"sourceFormat", "gcode"},
          {"moveType", qstr(moveTypeName(path.moveType))},
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
