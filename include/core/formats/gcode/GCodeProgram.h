#pragma once

#include "core/formats/gcode/GCodeDiagnostic.h"
#include "core/math/Vector.h"

#include <string>
#include <vector>

struct GCodePosition {
  double x = 0.0;
  double y = 0.0;
  double z = 0.0;
  double e = 0.0;
};

struct GCodeMotion {
  int lineNumber = 0;
  bool rapid = false;
  Vector3d start;
  Vector3d end;
  double startExtruder = 0.0;
  double endExtruder = 0.0;
  double extrusionDelta = 0.0;
  double feedRate = 0.0;
  int layerIndex = -1;
  int tool = 0;
  std::string featureType;
  std::string comment;

  [[nodiscard]] bool isExtruding() const {
    return extrusionDelta > 0.0;
  }

  [[nodiscard]] bool isTravel() const {
    return !isExtruding();
  }
};

enum class GCodeTemperatureTarget { Tool, Bed };

struct GCodeTemperatureCommand {
  int lineNumber = 0;
  GCodeTemperatureTarget target = GCodeTemperatureTarget::Tool;
  bool wait = false;
  int tool = -1;
  double temperature = 0.0;
};

struct GCodeToolChange {
  int lineNumber = 0;
  int tool = 0;
};

struct GCodeLayer {
  int lineNumber = 0;
  int index = -1;
  double z = 0.0;
  std::string comment;
};

struct GCodeComment {
  int lineNumber = 0;
  std::string text;
};

struct GCodeMetadata {
  int lineNumber = 0;
  std::string key;
  std::string value;
};

struct GCodeProgram {
  std::vector<GCodeMotion> motions;
  std::vector<GCodeTemperatureCommand> temperatures;
  std::vector<GCodeToolChange> toolChanges;
  std::vector<GCodeLayer> layers;
  std::vector<GCodeComment> comments;
  std::vector<GCodeMetadata> metadata;
  GCodeDiagnostics diagnostics;
};
