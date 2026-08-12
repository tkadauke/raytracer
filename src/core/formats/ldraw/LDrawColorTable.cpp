#include "core/formats/ldraw/LDrawColorTable.h"

#include "LDrawParseHelpers.h"
#include "core/formats/ldraw/LDrawParser.h"
#include "render/materials/MatteMaterial.h"
#include "render/materials/PhongMaterial.h"
#include "render/materials/ReflectiveMaterial.h"
#include "render/materials/TransparentMaterial.h"
#include "render/textures/ConstantColorTexture.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <variant>

using namespace std;

namespace {
  Colord parseHexColor(const string& text, int lineNumber, const string& fieldName) {
    if (text.size() != 7 || text[0] != '#')
      LDRAW_THROW_PARSE_ERROR(lineNumber, fieldName + " expects #RRGGBB, got '" + text + "'");

    char* end = nullptr;
    errno = 0;
    const long value = strtol(text.c_str() + 1, &end, 16);
    if (errno != 0 || end == text.c_str() + 1 || *end != '\0' || value < 0 || value > 0xffffff)
      LDRAW_THROW_PARSE_ERROR(lineNumber, "invalid color for " + fieldName + ": '" + text + "'");

    return Colord::fromPackedRGB(static_cast<unsigned int>(value));
  }

  vector<string> split(const string& text) {
    istringstream stream(text);
    vector<string> result;
    string token;
    while (stream >> token)
      result.push_back(token);
    return result;
  }

  shared_ptr<render::ConstantColorTexture> texture(const Colord& color) {
    return make_shared<render::ConstantColorTexture>(color);
  }
}

LDrawColorReference LDrawColorReference::fromCode(int colorCode) {
  LDrawColorReference reference;
  reference.kind = LDrawColorReferenceKind::Code;
  reference.code = colorCode;
  return reference;
}

LDrawColorReference LDrawColorReference::fromDirectRgb(const Colord& rgb) {
  LDrawColorReference reference;
  reference.kind = LDrawColorReferenceKind::DirectRgb;
  reference.color = rgb;
  return reference;
}

bool LDrawColorDefinition::transparent() const {
  return alpha < 255;
}

void LDrawColorTable::parse(istream& input) {
  for (const auto& command : LDrawParser().parse(input)) {
    if (holds_alternative<LDrawMetaCommand>(command))
      parseMetaCommand(get<LDrawMetaCommand>(command));
  }
}

bool LDrawColorTable::loadLibraryConfig(const string& libraryRoot) {
  if (libraryRoot.empty())
    return false;

  const std::filesystem::path configPath = std::filesystem::path(libraryRoot) / "LDConfig.ldr";
  ifstream input(configPath);
  if (!input)
    return false;

  parse(input);
  return true;
}

bool LDrawColorTable::parseMetaCommand(const LDrawMetaCommand& command) {
  if (command.keyword != "!COLOUR")
    return false;

  add(parseColourRecord(command.text, command.lineNumber));
  return true;
}

LDrawColorDefinition LDrawColorTable::parseColourRecord(const string& text, int lineNumber) const {
  const auto tokens = split(text);
  const size_t begin = !tokens.empty() && tokens[0] == "0" ? 1 : 0;
  if (tokens.size() < begin + 8 || tokens[begin] != "!COLOUR")
    LDRAW_THROW_PARSE_ERROR(lineNumber, "!COLOUR expects name, CODE, VALUE, and EDGE fields");

  LDrawColorDefinition definition;
  definition.lineNumber = lineNumber;
  definition.name = tokens[begin + 1];
  bool hasCode = false;
  bool hasValue = false;
  bool hasEdge = false;

  for (size_t i = begin + 2; i < tokens.size();) {
    const string& token = tokens[i];
    if (token == "CODE" || token == "ALPHA" || token == "LUMINANCE") {
      if (i + 1 >= tokens.size())
        LDRAW_THROW_PARSE_ERROR(lineNumber, token + " missing value");
      const int value = LDRAW_PARSE_INT(tokens[i + 1], lineNumber, token);
      if (token == "CODE") {
        definition.code = value;
        hasCode = true;
      } else if (token == "ALPHA") {
        if (value < 0 || value > 255)
          LDRAW_THROW_PARSE_ERROR(lineNumber, "ALPHA expects 0..255");
        definition.alpha = value;
      } else {
        if (value < 0 || value > 255)
          LDRAW_THROW_PARSE_ERROR(lineNumber, "LUMINANCE expects 0..255");
        definition.luminance = value;
      }
      i += 2;
    } else if (token == "VALUE") {
      if (i + 1 >= tokens.size())
        LDRAW_THROW_PARSE_ERROR(lineNumber, "VALUE missing value");
      definition.value = parseHexColor(tokens[i + 1], lineNumber, "VALUE");
      hasValue = true;
      i += 2;
    } else if (token == "EDGE") {
      if (i + 1 >= tokens.size())
        LDRAW_THROW_PARSE_ERROR(lineNumber, "EDGE missing value");
      if (!tokens[i + 1].empty() && tokens[i + 1][0] == '#')
        definition.edge =
          LDrawColorReference::fromDirectRgb(parseHexColor(tokens[i + 1], lineNumber, "EDGE"));
      else
        definition.edge =
          LDrawColorReference::fromCode(LDRAW_PARSE_INT(tokens[i + 1], lineNumber, "EDGE"));
      hasEdge = true;
      i += 2;
    } else {
      definition.finishTokens.assign(tokens.begin() + static_cast<long>(i), tokens.end());
      if (token == "RUBBER")
        definition.finish = LDrawColorFinish::Rubber;
      else if (token == "CHROME")
        definition.finish = LDrawColorFinish::Chrome;
      else if (token == "METAL")
        definition.finish = LDrawColorFinish::Metal;
      else if (token == "MATTE_METALLIC")
        definition.finish = LDrawColorFinish::MatteMetallic;
      else if (token == "PEARLESCENT")
        definition.finish = LDrawColorFinish::Pearlescent;
      else if (token == "MATERIAL" && i + 1 < tokens.size() && tokens[i + 1] == "GLITTER")
        definition.finish = LDrawColorFinish::Glitter;
      else if (token == "MATERIAL" && i + 1 < tokens.size() && tokens[i + 1] == "SPECKLE")
        definition.finish = LDrawColorFinish::Speckle;
      else if (token == "MATERIAL")
        definition.finish = LDrawColorFinish::CustomMaterial;
      break;
    }
  }

  if (!hasCode || !hasValue || !hasEdge)
    LDRAW_THROW_PARSE_ERROR(lineNumber, "!COLOUR missing required CODE, VALUE, or EDGE field");

  return definition;
}

void LDrawColorTable::add(const LDrawColorDefinition& definition) {
  m_definitions[definition.code] = definition;
}

const LDrawColorDefinition* LDrawColorTable::find(int code) const {
  const auto it = m_definitions.find(code);
  if (it == m_definitions.end())
    return nullptr;
  return &it->second;
}

LDrawColorReference LDrawColorTable::resolveReference(int code,
                                                      const LDrawColorContext& context) const {
  if (code == 16)
    return context.currentColor;
  if (code == 24)
    return context.edgeColor;
  if (isDirectRgbCode(code))
    return LDrawColorReference::fromDirectRgb(directRgbColor(code));
  return LDrawColorReference::fromCode(code);
}

LDrawColorReference LDrawColorTable::resolveEdgeReference(int code,
                                                          const LDrawColorContext& context) const {
  if (code == 24)
    return edgeReferenceFor(context.currentColor, context);
  return resolveReference(code, context);
}

Colord LDrawColorTable::colorForCode(int code, const LDrawColorContext& context) const {
  return colorForCode(code, context, nullptr, {}, 0);
}

Colord LDrawColorTable::colorForCode(int code, const LDrawColorContext& context,
                                     LDrawDiagnostics* diagnostics, const string& file,
                                     int lineNumber) const {
  const auto reference = resolveReference(code, context);
  if (reference.kind == LDrawColorReferenceKind::DirectRgb)
    return reference.color;

  if (const auto* definition = find(reference.code))
    return definition->value;

  if (diagnostics) {
    ostringstream message;
    message << "color code " << reference.code << " is not defined; using neutral gray";
    diagnostics->warning(LDrawDiagnosticCode::ColorFallback, file, lineNumber, message.str());
  }
  return Colord::fromRGB(128, 128, 128);
}

Colord LDrawColorTable::edgeColorForCode(int code, const LDrawColorContext& context) const {
  return edgeColorForCode(code, context, nullptr, {}, 0);
}

Colord LDrawColorTable::edgeColorForCode(int code, const LDrawColorContext& context,
                                         LDrawDiagnostics* diagnostics, const string& file,
                                         int lineNumber) const {
  const auto reference = resolveEdgeReference(code, context);
  if (reference.kind == LDrawColorReferenceKind::DirectRgb)
    return reference.color;

  if (const auto* definition = find(reference.code))
    return definition->value;

  if (diagnostics) {
    ostringstream message;
    message << "edge color code " << reference.code << " is not defined; using neutral gray";
    diagnostics->warning(LDrawDiagnosticCode::ColorFallback, file, lineNumber, message.str());
  }
  return Colord::fromRGB(128, 128, 128);
}

shared_ptr<render::Material>
LDrawColorTable::materialForCode(int code, const LDrawColorContext& context) const {
  return materialForCode(code, context, nullptr, {}, 0);
}

shared_ptr<render::Material> LDrawColorTable::materialForCode(int code,
                                                              const LDrawColorContext& context,
                                                              LDrawDiagnostics* diagnostics,
                                                              const string& file,
                                                              int lineNumber) const {
  const auto reference = resolveReference(code, context);
  if (reference.kind == LDrawColorReferenceKind::DirectRgb)
    return materialForColor(reference.color);

  if (const auto* definition = find(reference.code))
    return materialForDefinition(*definition);

  if (diagnostics) {
    ostringstream message;
    message << "color code " << reference.code << " is not defined; using neutral gray";
    diagnostics->warning(LDrawDiagnosticCode::ColorFallback, file, lineNumber, message.str());
  }
  return materialForColor(Colord::fromRGB(128, 128, 128));
}

LDrawColorContext LDrawColorTable::contextForSubfile(int colorCode,
                                                     const LDrawColorContext& parent) const {
  LDrawColorContext child;
  child.currentColor = resolveReference(colorCode, parent);
  child.edgeColor = colorCode == 16 || colorCode == 24
                      ? parent.edgeColor
                      : edgeReferenceFor(child.currentColor, parent);
  return child;
}

bool LDrawColorTable::isDirectRgbCode(int code) {
  return (code & 0xff000000) == 0x02000000;
}

Colord LDrawColorTable::directRgbColor(int code) {
  return Colord::fromPackedRGB(static_cast<unsigned int>(code));
}

shared_ptr<render::Material>
LDrawColorTable::materialForDefinition(const LDrawColorDefinition& definition) const {
  if (definition.transparent()) {
    auto material = make_shared<render::TransparentMaterial>(texture(definition.value));
    material->setTransmissionCoefficient(1.0 - static_cast<double>(definition.alpha) / 255.0);
    material->setReflectionCoefficient(0.1);
    material->setRefractionIndex(1.5);
    material->setSpecularCoefficient(0.2);
    return material;
  }

  switch (definition.finish) {
  case LDrawColorFinish::Rubber: {
    auto material = make_shared<render::MatteMaterial>(texture(definition.value));
    material->setDiffuseCoefficient(0.85);
    return material;
  }
  case LDrawColorFinish::Chrome: {
    auto material =
      make_shared<render::ReflectiveMaterial>(texture(definition.value), Colord::white());
    material->setReflectionCoefficient(0.8);
    material->setSpecularCoefficient(1.0);
    material->setExponent(96);
    return material;
  }
  case LDrawColorFinish::Metal:
  case LDrawColorFinish::MatteMetallic:
  case LDrawColorFinish::Pearlescent:
  case LDrawColorFinish::Glitter:
  case LDrawColorFinish::Speckle:
  case LDrawColorFinish::CustomMaterial: {
    auto material =
      make_shared<render::ReflectiveMaterial>(texture(definition.value), Colord::white());
    material->setReflectionCoefficient(definition.finish == LDrawColorFinish::MatteMetallic ? 0.25
                                                                                            : 0.45);
    material->setSpecularCoefficient(0.6);
    material->setExponent(definition.finish == LDrawColorFinish::MatteMetallic ? 24 : 48);
    return material;
  }
  case LDrawColorFinish::Plastic:
    return materialForColor(definition.value);
  }

  return materialForColor(definition.value);
}

shared_ptr<render::Material> LDrawColorTable::materialForColor(const Colord& color) const {
  auto material = make_shared<render::PhongMaterial>(texture(color));
  material->setSpecularCoefficient(0.35);
  material->setExponent(32);
  return material;
}

LDrawColorReference LDrawColorTable::edgeReferenceFor(const LDrawColorReference& current,
                                                      const LDrawColorContext& context) const {
  if (current.kind == LDrawColorReferenceKind::DirectRgb)
    return LDrawColorReference::fromDirectRgb(current.color * 0.2);

  if (const auto* definition = find(current.code))
    return definition->edge;

  return context.edgeColor;
}
