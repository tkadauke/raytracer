#pragma once

#include "core/formats/gcode/GCodeProgram.h"
#include "core/geometry/Polyline.h"

#include <string>
#include <vector>

enum class GCodePathMoveType { Travel, Extrusion };

std::string moveTypeName(GCodePathMoveType moveType);

struct GCodePath {
  int tool = 0;
  int layerIndex = -1;
  double layerZ = 0.0;
  int lineStart = 0;
  int lineEnd = 0;
  GCodePathMoveType moveType = GCodePathMoveType::Travel;
  std::string featureType;
  core::Polyline polyline;
};

struct GCodePathLayer {
  int index = -1;
  double z = 0.0;
  int lineStart = 0;
  int lineEnd = 0;
  std::string comment;
  std::vector<GCodePath> paths;
};

struct GCodePathProgram {
  std::vector<GCodePathLayer> layers;
};

class GCodePathCompiler {
public:
  [[nodiscard]] GCodePathProgram compile(const GCodeProgram& program) const;
};
