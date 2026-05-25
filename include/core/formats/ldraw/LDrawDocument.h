#pragma once

#include "core/formats/ldraw/LDrawParser.h"

#include <filesystem>

struct LDrawResolvedDocument {
  std::filesystem::path path;
  LDrawParser::Commands commands;
};
