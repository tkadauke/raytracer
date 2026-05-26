#pragma once

#include "core/formats/gcode/GCodeProgram.h"

#include <istream>
#include <optional>
#include <string>

class GCodeParser {
public:
  GCodeProgram parse(std::istream& input) const;
  void parseLine(const std::string& line, int lineNumber, GCodeProgram& program) const;

private:
  struct State {
    bool absolutePositioning = true;
    bool absoluteExtrusion = true;
    GCodePosition position;
    double feedRate = 0.0;
    int layerIndex = -1;
    std::string featureType;
    std::optional<double> lastLayerZ;
    int activeTool = -1;
  };

  mutable State m_state;
};
