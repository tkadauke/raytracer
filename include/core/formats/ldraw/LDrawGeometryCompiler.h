#pragma once

#include "core/formats/ldraw/LDrawColorTable.h"
#include "core/formats/ldraw/LDrawFileResolver.h"
#include "core/formats/ldraw/LDrawParser.h"
#include "render/textures/Texture.h"

#include <cstddef>
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
  enum class NormalMode { Flat, Smooth };

  struct CacheStats {
    std::size_t parsedSubfileMisses = 0;
    std::size_t compiledSubfileMisses = 0;
    std::size_t compiledSubfileHits = 0;
  };

  explicit LDrawGeometryCompiler(std::shared_ptr<const LDrawFileResolver> resolver = nullptr,
                                 int recursionLimit = 64,
                                 NormalMode normalMode = NormalMode::Flat);

  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
          const LDrawColorContext& context = LDrawColorContext()) const;
  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
          LDrawDiagnostics& diagnostics,
          const LDrawColorContext& context = LDrawColorContext()) const;

  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(std::istream& input, const LDrawColorTable& colors,
          const LDrawColorContext& context = LDrawColorContext()) const;
  [[nodiscard]] std::shared_ptr<render::Composite>
  compile(std::istream& input, const LDrawColorTable& colors, LDrawDiagnostics& diagnostics,
          const LDrawColorContext& context = LDrawColorContext()) const;

  [[nodiscard]] CacheStats cacheStats() const;
  void resetCacheStats() const;

private:
  struct CompileState {
    int depth = 0;
    std::unordered_set<std::string> activeFiles;
    std::string currentFile;
    std::string currentMpdBlock;
    LDrawDiagnostics* diagnostics = nullptr;
    std::unordered_map<std::string, std::shared_ptr<render::Texturec>> textures;
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
  NormalMode m_normalMode;
  mutable std::unordered_map<std::string, LDrawParser::Commands> m_parsedSubfiles;
  mutable std::unordered_map<std::string, std::shared_ptr<render::Composite>> m_compiledSubfiles;
  mutable CacheStats m_cacheStats;
};
