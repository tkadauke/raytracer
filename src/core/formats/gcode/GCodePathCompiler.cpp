#include "core/formats/gcode/GCodePathCompiler.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <optional>
#include <vector>

std::string moveTypeName(GCodePathMoveType moveType) {
  return moveType == GCodePathMoveType::Extrusion ? "extrusion" : "travel";
}

namespace {
  constexpr double PointTolerance = 1e-12;

  bool samePoint(const Vector3d& lhs, const Vector3d& rhs) {
    return std::abs(lhs.x() - rhs.x()) <= PointTolerance &&
           std::abs(lhs.y() - rhs.y()) <= PointTolerance &&
           std::abs(lhs.z() - rhs.z()) <= PointTolerance;
  }

  bool zeroLength(const GCodeMotion& motion) {
    return samePoint(motion.start, motion.end);
  }

  GCodePathMoveType moveTypeFor(const GCodeMotion& motion) {
    return motion.isExtruding() ? GCodePathMoveType::Extrusion : GCodePathMoveType::Travel;
  }

  int normalizedLayerIndex(const GCodeMotion& motion) {
    return motion.layerIndex < 0 ? 0 : motion.layerIndex;
  }

  struct PathKey {
    int layerIndex = -1;
    int tool = 0;
    GCodePathMoveType moveType = GCodePathMoveType::Travel;
    std::string featureType;

    [[nodiscard]] bool operator==(const PathKey& other) const {
      return layerIndex == other.layerIndex && tool == other.tool && moveType == other.moveType &&
             featureType == other.featureType;
    }
  };

  struct TemperatureState {
    std::map<int, double> toolTemperatures;
    std::optional<double> bedTemperature;
  };

  void applyTemperatureCommand(const GCodeTemperatureCommand& command, TemperatureState& state) {
    if (command.target == GCodeTemperatureTarget::Tool)
      state.toolTemperatures[command.tool] = command.temperature;
    else
      state.bedTemperature = command.temperature;
  }

  std::optional<double> activeToolTemperature(const TemperatureState& state, int tool) {
    const auto found = state.toolTemperatures.find(tool);
    if (found == state.toolTemperatures.end())
      return std::nullopt;
    return found->second;
  }

  void setSegmentAttributes(core::Polyline& polyline, std::size_t segmentIndex,
                            const GCodeMotion& motion, GCodePathMoveType moveType,
                            const TemperatureState& temperatures) {
    polyline.setSegmentAttribute(segmentIndex, "move_type", moveTypeName(moveType));
    polyline.setSegmentAttribute(segmentIndex, "speed", motion.feedRate);
    polyline.setSegmentAttribute(segmentIndex, "feed_rate", motion.feedRate);
    polyline.setSegmentAttribute(segmentIndex, "extrusion_amount", motion.extrusionDelta);
    polyline.setSegmentAttribute(segmentIndex, "line_number", motion.lineNumber);
    polyline.setSegmentAttribute(segmentIndex, "tool", motion.tool);
    polyline.setSegmentAttribute(segmentIndex, "layer_index", normalizedLayerIndex(motion));
    if (!motion.featureType.empty())
      polyline.setSegmentAttribute(segmentIndex, "feature_type", motion.featureType);
    if (const auto temperature = activeToolTemperature(temperatures, motion.tool))
      polyline.setSegmentAttribute(segmentIndex, "temperature", *temperature);
    if (temperatures.bedTemperature)
      polyline.setSegmentAttribute(segmentIndex, "bed_temperature", *temperatures.bedTemperature);
  }

  std::optional<double> zForLayer(const GCodeProgram& program, int layerIndex) {
    for (auto it = program.layers.rbegin(); it != program.layers.rend(); ++it) {
      if (it->index == layerIndex)
        return it->z;
    }
    return std::nullopt;
  }

  std::optional<std::string> commentForLayer(const GCodeProgram& program, int layerIndex) {
    for (auto it = program.layers.rbegin(); it != program.layers.rend(); ++it) {
      if (it->index == layerIndex && !it->comment.empty())
        return it->comment;
    }
    return std::nullopt;
  }
}

GCodePathProgram GCodePathCompiler::compile(const GCodeProgram& program) const {
  GCodePathProgram result;
  std::map<int, std::size_t> layerOffsets;
  std::optional<PathKey> activeKey;
  GCodePath* activePath = nullptr;
  std::size_t nextTemperature = 0;
  TemperatureState temperatures;

  for (const auto& motion : program.motions) {
    while (nextTemperature < program.temperatures.size() &&
           program.temperatures[nextTemperature].lineNumber < motion.lineNumber) {
      applyTemperatureCommand(program.temperatures[nextTemperature], temperatures);
      ++nextTemperature;
    }

    if (zeroLength(motion))
      continue;

    const int layerIndex = normalizedLayerIndex(motion);
    const auto moveType = moveTypeFor(motion);
    const PathKey key{layerIndex, motion.tool, moveType, motion.featureType};

    auto layerIt = layerOffsets.find(layerIndex);
    if (layerIt == layerOffsets.end()) {
      GCodePathLayer layer;
      layer.index = layerIndex;
      layer.z = zForLayer(program, layerIndex).value_or(motion.end.z());
      layer.lineStart = motion.lineNumber;
      layer.lineEnd = motion.lineNumber;
      layer.comment = commentForLayer(program, layerIndex).value_or(std::string());
      result.layers.push_back(std::move(layer));
      layerIt = layerOffsets.emplace(layerIndex, result.layers.size() - 1).first;
    }

    auto& layer = result.layers[layerIt->second];
    layer.lineStart =
      layer.lineStart == 0 ? motion.lineNumber : std::min(layer.lineStart, motion.lineNumber);
    layer.lineEnd = std::max(layer.lineEnd, motion.lineNumber);

    const bool canAppend = activePath != nullptr && activeKey && *activeKey == key &&
                           samePoint(activePath->polyline.points().back(), motion.start);
    if (!canAppend) {
      GCodePath path;
      path.tool = motion.tool;
      path.layerIndex = layerIndex;
      path.layerZ = layer.z;
      path.lineStart = motion.lineNumber;
      path.lineEnd = motion.lineNumber;
      path.moveType = moveType;
      path.featureType = motion.featureType;
      path.polyline.setAttribute("source_format", std::string("gcode"));
      path.polyline.setAttribute("move_type", moveTypeName(moveType));
      path.polyline.setAttribute("tool", motion.tool);
      path.polyline.setAttribute("layer_index", layerIndex);
      path.polyline.setAttribute("layer_z", layer.z);
      if (!motion.featureType.empty())
        path.polyline.setAttribute("feature_type", motion.featureType);
      path.polyline.addPoint(motion.start);
      layer.paths.push_back(std::move(path));
      activePath = &layer.paths.back();
      activeKey = key;
    }

    activePath->polyline.addPoint(motion.end);
    const std::size_t segmentIndex = activePath->polyline.segmentCount() - 1;
    setSegmentAttributes(activePath->polyline, segmentIndex, motion, moveType, temperatures);
    activePath->lineEnd = motion.lineNumber;
  }

  return result;
}
