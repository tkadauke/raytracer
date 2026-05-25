#pragma once

#include "core/formats/ldraw/LDrawParser.h"

#include <filesystem>

struct LDrawDocument {
  std::filesystem::path path;
  LDrawParser::Commands commands;
};
