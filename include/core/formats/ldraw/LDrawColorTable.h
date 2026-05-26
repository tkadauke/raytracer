#pragma once

#include "core/Color.h"
#include "core/formats/ldraw/LDrawCommand.h"
#include "core/formats/ldraw/LDrawDiagnostic.h"

#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace render {
  class Material;
}

enum class LDrawColorFinish {
  Plastic,
  Rubber,
  Chrome,
  Metal,
  MatteMetallic,
  Pearlescent,
  Glitter,
  Speckle,
  CustomMaterial
};

enum class LDrawColorReferenceKind { Code, DirectRgb };

struct LDrawColorReference {
  LDrawColorReferenceKind kind = LDrawColorReferenceKind::Code;
  int code = 0;
  Colord color;

  static LDrawColorReference fromCode(int colorCode);
  static LDrawColorReference fromDirectRgb(const Colord& rgb);
};

struct LDrawColorDefinition {
  int lineNumber = 0;
  std::string name;
  int code = 0;
  Colord value;
  LDrawColorReference edge = LDrawColorReference::fromCode(0);
  int alpha = 255;
  std::optional<int> luminance;
  LDrawColorFinish finish = LDrawColorFinish::Plastic;
  std::vector<std::string> finishTokens;

  [[nodiscard]] bool transparent() const;
};

struct LDrawColorContext {
  LDrawColorReference currentColor = LDrawColorReference::fromCode(7);
  LDrawColorReference edgeColor = LDrawColorReference::fromCode(0);
};

class LDrawColorTable {
public:
  void parse(std::istream& input);
  bool loadLibraryConfig(const std::string& libraryRoot);
  bool parseMetaCommand(const LDrawMetaCommand& command);
  LDrawColorDefinition parseColourRecord(const std::string& text, int lineNumber) const;

  void add(const LDrawColorDefinition& definition);
  const LDrawColorDefinition* find(int code) const;

  [[nodiscard]] LDrawColorReference resolveReference(int code,
                                                     const LDrawColorContext& context) const;
  [[nodiscard]] LDrawColorReference resolveEdgeReference(int code,
                                                         const LDrawColorContext& context) const;
  [[nodiscard]] Colord colorForCode(int code,
                                    const LDrawColorContext& context = LDrawColorContext()) const;
  [[nodiscard]] Colord colorForCode(int code, const LDrawColorContext& context,
                                    LDrawDiagnostics* diagnostics, const std::string& file,
                                    int lineNumber) const;
  [[nodiscard]] Colord
  edgeColorForCode(int code, const LDrawColorContext& context = LDrawColorContext()) const;
  [[nodiscard]] Colord edgeColorForCode(int code, const LDrawColorContext& context,
                                        LDrawDiagnostics* diagnostics, const std::string& file,
                                        int lineNumber) const;
  [[nodiscard]] std::shared_ptr<render::Material>
  materialForCode(int code, const LDrawColorContext& context = LDrawColorContext()) const;
  [[nodiscard]] std::shared_ptr<render::Material>
  materialForCode(int code, const LDrawColorContext& context, LDrawDiagnostics* diagnostics,
                  const std::string& file, int lineNumber) const;
  [[nodiscard]] LDrawColorContext contextForSubfile(int colorCode,
                                                    const LDrawColorContext& parent) const;

  static bool isDirectRgbCode(int code);
  static Colord directRgbColor(int code);

private:
  std::shared_ptr<render::Material>
  materialForDefinition(const LDrawColorDefinition& definition) const;
  std::shared_ptr<render::Material> materialForColor(const Colord& color) const;
  LDrawColorReference edgeReferenceFor(const LDrawColorReference& current,
                                       const LDrawColorContext& context) const;

  std::unordered_map<int, LDrawColorDefinition> m_definitions;
};
