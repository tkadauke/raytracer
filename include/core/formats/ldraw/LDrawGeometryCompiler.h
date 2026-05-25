#pragma once

#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawParser.h"

#include <iosfwd>
#include <memory>

namespace render {
  class Composite;
}

/**
  * Converts parsed inline LDraw polygon commands to renderable runtime
  * geometry. This compiler intentionally handles only type 3 triangles and
  * type 4 quads; subfile references, MPD blocks, and edge overlays are left for
  * later import phases.
  */
class LDrawGeometryCompiler {
public:
  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
          const LDrawColorContext& context = LDrawColorContext()) const;

  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(std::istream& input, const LDrawColorTable& colors,
          const LDrawColorContext& context = LDrawColorContext()) const;
};
