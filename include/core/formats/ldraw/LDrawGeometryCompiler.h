#pragma once

#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawParser.h"

#include <iosfwd>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

namespace render {
  class Composite;
}

/**
  * Converts parsed LDraw polygon commands and type-1 subfile references to
  * renderable runtime geometry. Subfiles are resolved through the configured
  * resolver, parsed once per filename, compiled once per effective color
  * context, and then instanced with the type-1 transform matrix.
  */
class LDrawGeometryCompiler {
public:
  explicit LDrawGeometryCompiler(std::shared_ptr<const LDrawFileResolver> resolver = nullptr,
                                 int recursionLimit = 64);

  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
          const LDrawColorContext& context = LDrawColorContext()) const;

  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(std::istream& input, const LDrawColorTable& colors,
          const LDrawColorContext& context = LDrawColorContext()) const;

private:
  struct CompileState {
    int depth = 0;
    std::unordered_set<std::string> activeFiles;
  };

  [[nodiscard]] std::shared_ptr<render::Composite>
  compileCommands(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
                  const LDrawColorContext& context, CompileState& state,
                  bool inheritedInverted) const;

  [[nodiscard]] std::shared_ptr<render::Composite>
  compileSubfile(const LDrawSubfileReference& reference, const LDrawColorTable& colors,
                 const LDrawColorContext& context, CompileState& state,
                 bool inheritedInverted) const;

  [[nodiscard]] static std::string colorContextKey(const LDrawColorContext& context);

  std::shared_ptr<const LDrawFileResolver> m_resolver;
  int m_recursionLimit;
  mutable std::unordered_map<std::string, LDrawParser::Commands> m_parsedSubfiles;
  mutable std::unordered_map<std::string, std::shared_ptr<render::Composite>> m_compiledSubfiles;
};
