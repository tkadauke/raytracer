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

  [[nodiscard]] inline bool isComment() const {
    return keyword.empty();
  }
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

using LDrawCommand = std::variant<LDrawEmptyLine,
                                  LDrawMetaCommand,
                                  LDrawSubfileReference,
                                  LDrawEdgeLine,
                                  LDrawTriangle,
                                  LDrawQuad,
                                  LDrawOptionalLine,
                                  LDrawUnknownCommand>;
