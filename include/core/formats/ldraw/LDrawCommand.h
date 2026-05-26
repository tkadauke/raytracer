#pragma once

#include "core/math/Vector.h"

#include <array>
#include <string>
#include <variant>
#include <vector>

struct LDrawEmptyLine {
  int lineNumber = 0;
};

struct LDrawMetaCommand {
  int lineNumber = 0;
  std::string text;
  std::string keyword;
  std::vector<std::string> arguments;

  [[nodiscard]] bool isComment() const;
  [[nodiscard]] bool isGeometryDirective() const;
  [[nodiscard]] bool isInformational() const;
};

enum class LDrawTexmapCommand { Start, Next, Fallback, End };

enum class LDrawTexmapProjection { Planar, Cylindrical, Spherical, Unknown };

struct LDrawTexmap {
  int lineNumber = 0;
  LDrawTexmapCommand command = LDrawTexmapCommand::Start;
  LDrawTexmapProjection projection = LDrawTexmapProjection::Unknown;
  Vector3d points[3];
  std::string textureFile;
  std::string text;
};

struct LDrawSubfileReference {
  int lineNumber = 0;
  int color = 0;
  Vector3d translation;
  std::array<double, 9> matrix{};
  std::string filename;
};

struct LDrawEdgeLine {
  int lineNumber = 0;
  int color = 0;
  Vector3d points[2];
};

struct LDrawTriangle {
  int lineNumber = 0;
  int color = 0;
  Vector3d points[3];
};

struct LDrawQuad {
  int lineNumber = 0;
  int color = 0;
  Vector3d points[4];
};

struct LDrawOptionalLine {
  int lineNumber = 0;
  int color = 0;
  Vector3d points[4];
};

struct LDrawUnknownCommand {
  int lineNumber = 0;
  std::string lineType;
  std::string text;
};

using LDrawCommand =
  std::variant<LDrawEmptyLine, LDrawMetaCommand, LDrawTexmap, LDrawSubfileReference, LDrawEdgeLine,
               LDrawTriangle, LDrawQuad, LDrawOptionalLine, LDrawUnknownCommand>;
