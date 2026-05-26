#include "core/formats/ldraw/LDrawGeometryCompiler.h"

#include "core/Exception.h"
#include "core/formats/ldraw/LDrawParseError.h"
#include "core/geometry/AttributeColorMap.h"
#include "core/geometry/Mesh.h"
#include "core/geometry/Polyline.h"
#include "core/math/Matrix.h"
#include "render/materials/Material.h"
#include "render/materials/MatteMaterial.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Curve.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"
#include "render/textures/ImageTexture.h"
#include "render/textures/mappings/UVMapping2D.h"

#include <algorithm>
#include <cmath>
#include <exception>
#include <iomanip>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <utility>

using namespace std;

namespace {
  struct BfcState {
    bool certified = false;
    bool counterClockwise = true;
    bool clip = true;
    bool invertNext = false;
  };

  struct TexmapState {
    std::optional<LDrawTexmap> active;
    std::optional<LDrawTexmap> next;
    bool fallback = false;
  };

  bool shouldReverseFace(bool counterClockwise, bool inheritedInverted) {
    return !counterClockwise != inheritedInverted;
  }

  bool isSupportedTexmap(const LDrawTexmap& texmap) {
    return texmap.projection == LDrawTexmapProjection::Planar && !texmap.textureFile.empty();
  }

  const LDrawTexmap* texmapForGeometry(const TexmapState& texmap) {
    if (texmap.next)
      return &*texmap.next;
    if (texmap.active)
      return &*texmap.active;
    return nullptr;
  }

  bool shouldCompileGeometry(const TexmapState& texmap) {
    const LDrawTexmap* current = texmapForGeometry(texmap);
    if (!current)
      return !texmap.fallback;
    const bool supported = isSupportedTexmap(*current);
    return texmap.fallback ? !supported : supported;
  }

  Vector2d planarUV(const LDrawTexmap& texmap, const Vector3d& point) {
    const Vector3d origin = texmap.points[0];
    const Vector3d uAxis = texmap.points[1] - origin;
    const Vector3d vAxis = texmap.points[2] - origin;
    const Vector3d delta = point - origin;
    const double uu = uAxis * uAxis;
    const double uv = uAxis * vAxis;
    const double vv = vAxis * vAxis;
    const double du = delta * uAxis;
    const double dv = delta * vAxis;
    const double determinant = uu * vv - uv * uv;
    if (std::abs(determinant) < 1e-12)
      return Vector2d::null;
    return Vector2d((du * vv - dv * uv) / determinant,
                    (dv * uu - du * uv) / determinant);
  }

  std::shared_ptr<render::Texturec> textureForTexmap(
    const LDrawTexmap& texmap,
    const LDrawFileResolver* resolver,
    std::unordered_map<std::string, std::shared_ptr<render::Texturec>>& textures,
    LDrawDiagnostics* diagnostics,
    const string& file) {
    const std::string path =
      resolver ? resolver->resolvePath(texmap.textureFile) : texmap.textureFile;
    if (path.empty()) {
      if (diagnostics) {
        LDrawDiagnostic diagnostic;
        diagnostic.severity = LDrawDiagnosticSeverity::Error;
        diagnostic.code = LDrawDiagnosticCode::MissingTexture;
        diagnostic.file = file;
        diagnostic.lineNumber = texmap.lineNumber;
        diagnostic.message = "unable to resolve LDraw TEXMAP texture";
        diagnostic.reference = texmap.textureFile;
        if (resolver)
          diagnostic.searchedRoots = resolver->searchRoots(texmap.textureFile);
        diagnostics->add(std::move(diagnostic));
      }
      return nullptr;
    }

    auto cached = textures.find(path);
    if (cached != textures.end())
      return cached->second;

    try {
      auto texture = render::ImageTexture::fromFile(
        new render::UVMapping2D, path, render::ImageTextureFilter::Nearest,
        render::ImageTextureWrap::Clamp);
      textures.emplace(path, texture);
      return texture;
    } catch (const std::exception& error) {
      if (diagnostics) {
        LDrawDiagnostic diagnostic;
        diagnostic.severity = LDrawDiagnosticSeverity::Error;
        diagnostic.code = LDrawDiagnosticCode::MissingTexture;
        diagnostic.file = file;
        diagnostic.lineNumber = texmap.lineNumber;
        diagnostic.message = std::string("unable to load LDraw TEXMAP texture: ") + error.what();
        diagnostic.reference = texmap.textureFile;
        diagnostics->add(std::move(diagnostic));
      }
      return nullptr;
    }
  }

  std::shared_ptr<render::Material> materialForPolygon(const LDrawColorTable& colors,
                                                       int color,
                                                       const LDrawColorContext& context,
                                                       const BfcState& bfc,
                                                       const LDrawTexmap* texmap,
                                                       const LDrawFileResolver* resolver,
                                                       std::unordered_map<
                                                         std::string,
                                                         std::shared_ptr<render::Texturec>>& textures,
                                                       LDrawDiagnostics* diagnostics,
                                                       const string& file,
                                                       int lineNumber) {
    auto material = colors.materialForCode(color, context, diagnostics, file, lineNumber);
    if (texmap && isSupportedTexmap(*texmap)) {
      if (auto texture = textureForTexmap(*texmap, resolver, textures, diagnostics, file)) {
        auto texturedMaterial = std::make_shared<render::MatteMaterial>(texture);
        texturedMaterial->setAmbientCoefficient(1.0);
        texturedMaterial->setDiffuseCoefficient(1.0);
        material = texturedMaterial;
      }
    }
    if (bfc.certified && bfc.clip) {
      material->setSidedness(render::Material::Sidedness::Front);
    } else {
      material->setSidedness(render::Material::Sidedness::TwoSided);
      if (diagnostics) {
        diagnostics->warning(
          LDrawDiagnosticCode::BfcAmbiguity, file, lineNumber,
          bfc.certified
            ? "BFC NOCLIP polygon is treated as two-sided geometry"
            : "BFC uncertified polygon is treated as two-sided geometry");
      }
    }
    return material;
  }

  void attachPolygonProvenance(render::Primitive& primitive,
                               const string& file,
                               const string& mpdBlock,
                               int lineNumber,
                               int color,
                               int buildStep,
                               const string& commandType) {
    primitive.setMetadataValue("source.format", "ldraw");
    primitive.setMetadataValue("ldraw.source", file);
    if (!mpdBlock.empty())
      primitive.setMetadataValue("ldraw.mpdBlock", mpdBlock);
    primitive.setMetadataValue("ldraw.lineStart", to_string(lineNumber));
    primitive.setMetadataValue("ldraw.lineEnd", to_string(lineNumber));
    primitive.setMetadataValue("ldraw.colorCode", to_string(color));
    primitive.setMetadataValue("ldraw.buildStep", to_string(buildStep));
    primitive.setMetadataValue("ldraw.command", commandType);
  }

  void attachReferenceProvenance(render::Primitive& primitive,
                                 const LDrawSubfileReference& reference,
                                 const string& parentFile,
                                 const string& parentMpdBlock,
                                 int buildStep) {
    primitive.setMetadataValue("source.format", "ldraw");
    primitive.setMetadataValue("ldraw.source", parentFile);
    if (!parentMpdBlock.empty())
      primitive.setMetadataValue("ldraw.mpdBlock", parentMpdBlock);
    primitive.setMetadataValue("ldraw.lineStart", to_string(reference.lineNumber));
    primitive.setMetadataValue("ldraw.lineEnd", to_string(reference.lineNumber));
    primitive.setMetadataValue("ldraw.colorCode", to_string(reference.color));
    primitive.setMetadataValue("ldraw.buildStep", to_string(buildStep));
    primitive.setMetadataValue("ldraw.command", "1");
    primitive.setMetadataValue("ldraw.referencedPart", reference.filename);
    primitive.setMetadataValue("ldraw.parentReferenceFile", parentFile);
    primitive.setMetadataValue("ldraw.parentReferenceLine", to_string(reference.lineNumber));
  }

  shared_ptr<render::MeshPrimitive> meshPrimitiveForTriangle(const LDrawTriangle& triangle,
                                                            const LDrawColorTable& colors,
                                                            const LDrawColorContext& context,
                                                            const BfcState& bfc,
                                                            const LDrawTexmap* texmap,
                                                            bool inheritedInverted,
                                                            const LDrawFileResolver* resolver,
                                                            std::unordered_map<
                                                              std::string,
                                                              std::shared_ptr<render::Texturec>>& textures,
                                                            LDrawDiagnostics* diagnostics,
                                                            const string& file,
                                                            const string& mpdBlock,
                                                            int buildStep,
                                                            LDrawGeometryCompiler::NormalMode normalMode) {
    Mesh mesh;
    for (const auto& point : triangle.points) {
      mesh.addVertex(point, Vector3d::null,
                     texmap && isSupportedTexmap(*texmap) ? planarUV(*texmap, point)
                                                          : Vector2d::null);
    }
    const bool reverse = shouldReverseFace(bfc.counterClockwise, inheritedInverted);
    mesh.addFace({0, 1, 2}, reverse);
    mesh.computeNormals();

    auto primitive = make_shared<render::MeshPrimitive>(
      std::move(mesh),
      normalMode == LDrawGeometryCompiler::NormalMode::Smooth
        ? render::MeshPrimitive::NormalMode::Smooth
        : render::MeshPrimitive::NormalMode::Flat);
    primitive->setMaterial(
      materialForPolygon(colors, triangle.color, context, bfc, texmap, resolver, textures,
                         diagnostics, file,
                         triangle.lineNumber));
    attachPolygonProvenance(*primitive, file, mpdBlock, triangle.lineNumber, triangle.color,
                            buildStep, "3");
    if (reverse && diagnostics) {
      diagnostics->warning(LDrawDiagnosticCode::BfcAmbiguity, file, triangle.lineNumber,
                           "BFC winding was reversed before compiling this triangle");
    }
    return primitive;
  }

  shared_ptr<render::MeshPrimitive> meshPrimitiveForQuad(const LDrawQuad& quad,
                                                        const LDrawColorTable& colors,
                                                        const LDrawColorContext& context,
                                                        const BfcState& bfc,
                                                        const LDrawTexmap* texmap,
                                                        bool inheritedInverted,
                                                        const LDrawFileResolver* resolver,
                                                        std::unordered_map<
                                                          std::string,
                                                          std::shared_ptr<render::Texturec>>& textures,
                                                        LDrawDiagnostics* diagnostics,
                                                        const string& file,
                                                        const string& mpdBlock,
                                                        int buildStep,
                                                        LDrawGeometryCompiler::NormalMode normalMode) {
    Mesh mesh;
    for (const auto& point : quad.points) {
      mesh.addVertex(point, Vector3d::null,
                     texmap && isSupportedTexmap(*texmap) ? planarUV(*texmap, point)
                                                          : Vector2d::null);
    }
    const bool reverse = shouldReverseFace(bfc.counterClockwise, inheritedInverted);
    mesh.addFace({0, 1, 2, 3}, reverse);
    mesh.computeNormals();

    auto primitive = make_shared<render::MeshPrimitive>(
      std::move(mesh),
      normalMode == LDrawGeometryCompiler::NormalMode::Smooth
        ? render::MeshPrimitive::NormalMode::Smooth
        : render::MeshPrimitive::NormalMode::Flat);
    primitive->setMaterial(
      materialForPolygon(colors, quad.color, context, bfc, texmap, resolver, textures,
                         diagnostics, file, quad.lineNumber));
    attachPolygonProvenance(*primitive, file, mpdBlock, quad.lineNumber, quad.color, buildStep,
                            "4");
    if (reverse && diagnostics) {
      diagnostics->warning(LDrawDiagnosticCode::BfcAmbiguity, file, quad.lineNumber,
                           "BFC winding was reversed before compiling this quad");
    }
    return primitive;
  }

  shared_ptr<render::Curve> curveForEdgeLine(const LDrawEdgeLine& edge,
                                             const LDrawColorTable& colors,
                                             const LDrawColorContext& context,
                                             LDrawDiagnostics* diagnostics,
                                             const string& file) {
    core::Polyline polyline({edge.points[0], edge.points[1]});
    polyline.setSegmentAttribute(0, "ldraw_edge", true);

    auto colorMap = core::AttributeColorMap::categorical("ldraw_edge");
    colorMap.setCategoryColor(
      true, colors.edgeColorForCode(edge.color, context, diagnostics, file, edge.lineNumber));

    auto curve =
      make_shared<render::Curve>(polyline, 0.0, render::Curve::TessellationMode::Ribbon);
    curve->setSegmentColorMap(colorMap);
    return curve;
  }

  Matrix4d transformForSubfileReference(const LDrawSubfileReference& reference) {
    const auto& m = reference.matrix;
    return Matrix4d(m[0], m[1], m[2], reference.translation.x(),
                    m[3], m[4], m[5], reference.translation.y(),
                    m[6], m[7], m[8], reference.translation.z(),
                    0.0, 0.0, 0.0, 1.0);
  }

  double determinantForSubfileReference(const LDrawSubfileReference& reference) {
    const auto& m = reference.matrix;
    return m[0] * (m[4] * m[8] - m[5] * m[7]) -
           m[1] * (m[3] * m[8] - m[5] * m[6]) +
           m[2] * (m[3] * m[7] - m[4] * m[6]);
  }

  void applyBfcMeta(const LDrawMetaCommand& command, BfcState& bfc) {
    if (command.keyword != "BFC")
      return;

    for (const auto& argument : command.arguments) {
      if (argument == "CERTIFY") {
        bfc.certified = true;
      } else if (argument == "NOCERTIFY") {
        bfc.certified = false;
      } else if (argument == "CCW") {
        bfc.counterClockwise = true;
      } else if (argument == "CW") {
        bfc.counterClockwise = false;
      } else if (argument == "CLIP") {
        bfc.clip = true;
      } else if (argument == "NOCLIP") {
        bfc.clip = false;
      } else if (argument == "INVERTNEXT") {
        bfc.invertNext = true;
      }
    }
  }

  void applyTexmapMeta(const LDrawTexmap& command, TexmapState& texmap,
                       LDrawDiagnostics* diagnostics, const string& file) {
    switch (command.command) {
    case LDrawTexmapCommand::Start:
      texmap.active = command;
      texmap.next.reset();
      texmap.fallback = false;
      break;
    case LDrawTexmapCommand::Next:
      texmap.next = command;
      texmap.fallback = false;
      break;
    case LDrawTexmapCommand::Fallback:
      texmap.fallback = true;
      texmap.next.reset();
      break;
    case LDrawTexmapCommand::End:
      texmap.active.reset();
      texmap.next.reset();
      texmap.fallback = false;
      break;
    }

    if ((command.command == LDrawTexmapCommand::Start ||
         command.command == LDrawTexmapCommand::Next) &&
        !isSupportedTexmap(command) && diagnostics) {
      diagnostics->warning(LDrawDiagnosticCode::UnsupportedTexmap, file, command.lineNumber,
                           "unsupported TEXMAP projection will use fallback geometry when present");
    }
  }

  string colorReferenceKey(const LDrawColorReference& reference) {
    ostringstream out;
    out << static_cast<int>(reference.kind) << ':';
    if (reference.kind == LDrawColorReferenceKind::DirectRgb) {
      out << setprecision(17) << reference.color.r() << ',' << reference.color.g() << ','
          << reference.color.b();
    } else {
      out << reference.code;
    }
    return out.str();
  }
}

LDrawGeometryCompiler::LDrawGeometryCompiler(shared_ptr<const LDrawFileResolver> resolver,
                                             int recursionLimit,
                                             NormalMode normalMode)
    : m_resolver(std::move(resolver)),
      m_options{recursionLimit, normalMode, true, true, MissingPartPolicy::Error} {
}

LDrawGeometryCompiler::LDrawGeometryCompiler(shared_ptr<const LDrawFileResolver> resolver,
                                             Options options)
    : m_resolver(std::move(resolver)),
      m_options(options) {
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
                               const LDrawColorContext& context) const {
  CompileState state;
  state.currentFile = "<input>";
  return compileCommands(commands, colors, context, state, false);
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
                               LDrawDiagnostics& diagnostics,
                               const LDrawColorContext& context) const {
  CompileState state;
  state.currentFile = "<input>";
  state.diagnostics = &diagnostics;
  return compileCommands(commands, colors, context, state, false);
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compileCommands(const LDrawParser::Commands& commands,
                                       const LDrawColorTable& colors,
                                       const LDrawColorContext& context,
                                       CompileState& state,
                                       bool inheritedInverted) const {
  auto result = make_shared<render::Composite>();
  BfcState bfc;
  int buildStep = 1;
  TexmapState texmap;

  for (const auto& command : commands) {
    if (holds_alternative<LDrawTriangle>(command)) {
      const LDrawTexmap* currentTexmap = texmapForGeometry(texmap);
      if (shouldCompileGeometry(texmap)) {
        result->add(meshPrimitiveForTriangle(
          get<LDrawTriangle>(command), colors, context, bfc, currentTexmap, inheritedInverted,
          m_resolver.get(), state.textures, state.diagnostics, state.currentFile,
          state.currentMpdBlock, buildStep, m_options.normalMode));
      }
      texmap.next.reset();
    } else if (holds_alternative<LDrawQuad>(command)) {
      const LDrawTexmap* currentTexmap = texmapForGeometry(texmap);
      if (shouldCompileGeometry(texmap)) {
        result->add(meshPrimitiveForQuad(
          get<LDrawQuad>(command), colors, context, bfc, currentTexmap, inheritedInverted,
          m_resolver.get(), state.textures, state.diagnostics, state.currentFile,
          state.currentMpdBlock, buildStep, m_options.normalMode));
      }
      texmap.next.reset();
    } else if (holds_alternative<LDrawSubfileReference>(command)) {
      if (!shouldCompileGeometry(texmap)) {
        texmap.next.reset();
        continue;
      }
      const auto& reference = get<LDrawSubfileReference>(command);
      const bool subfileInverted =
        (inheritedInverted != bfc.invertNext) != (determinantForSubfileReference(reference) < 0.0);
      auto subfile = compileSubfile(reference, colors, colors.contextForSubfile(reference.color, context),
                                    state, subfileInverted);
      const Matrix4d transform = transformForSubfileReference(reference);
      if (!m_options.preserveHierarchy && transform == Matrix4d()) {
        for (const auto& primitive : subfile->primitives()) {
          result->add(primitive);
        }
      } else {
        auto instance = make_shared<render::Instance>(subfile);
        instance->setMatrix(transform);
        attachReferenceProvenance(*instance, reference, state.currentFile, state.currentMpdBlock,
                                  buildStep);
        result->add(instance);
      }
      bfc.invertNext = false;
      texmap.next.reset();
    } else if (holds_alternative<LDrawMetaCommand>(command)) {
      const auto& meta = get<LDrawMetaCommand>(command);
      const bool wasBfc = meta.keyword == "BFC";
      applyBfcMeta(meta, bfc);
      if (meta.keyword == "STEP")
        ++buildStep;
      if (!meta.isComment() && !wasBfc && meta.keyword != "!COLOUR" && meta.keyword != "STEP" &&
          state.diagnostics) {
        state.diagnostics->warning(
          LDrawDiagnosticCode::UnsupportedMetaCommand, state.currentFile, meta.lineNumber,
          "unsupported meta command '" + meta.keyword + "' was ignored");
      }
    } else if (holds_alternative<LDrawTexmap>(command)) {
      applyTexmapMeta(get<LDrawTexmap>(command), texmap, state.diagnostics, state.currentFile);
    } else if (holds_alternative<LDrawEdgeLine>(command)) {
      const auto& edge = get<LDrawEdgeLine>(command);
      if (m_options.includeEdgeOverlays) {
        result->add(curveForEdgeLine(edge, colors, context, state.diagnostics, state.currentFile));
      }
    } else if (holds_alternative<LDrawOptionalLine>(command)) {
      const auto& optional = get<LDrawOptionalLine>(command);
      if (state.diagnostics) {
        state.diagnostics->warning(LDrawDiagnosticCode::SkippedGeometry, state.currentFile,
                                   optional.lineNumber,
                                   "type 5 optional line was skipped by the geometry compiler");
      }
    } else if (holds_alternative<LDrawUnknownCommand>(command)) {
      const auto& unknown = get<LDrawUnknownCommand>(command);
      if (state.diagnostics) {
        state.diagnostics->warning(
          LDrawDiagnosticCode::UnsupportedLineType, state.currentFile, unknown.lineNumber,
          "unsupported line type '" + unknown.lineType + "' was ignored");
      }
    }
  }

  return result;
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compileSubfile(const LDrawSubfileReference& reference,
                                      const LDrawColorTable& colors,
                                      const LDrawColorContext& context,
                                      CompileState& state,
                                      bool inheritedInverted) const {
  if (!m_resolver) {
    if (state.diagnostics) {
      LDrawDiagnostic diagnostic;
      diagnostic.severity = LDrawDiagnosticSeverity::Error;
      diagnostic.code = LDrawDiagnosticCode::MissingSubfile;
      diagnostic.file = state.currentFile;
      diagnostic.lineNumber = reference.lineNumber;
      diagnostic.message = "subfile reference requires an LDrawFileResolver";
      diagnostic.reference = reference.filename;
      state.diagnostics->add(std::move(diagnostic));
    }
    if (m_options.missingPartPolicy == MissingPartPolicy::Skip) {
      return make_shared<render::Composite>();
    }
    throw Exception("LDraw subfile reference requires an LDrawFileResolver: " + reference.filename,
                    __FILE__, __LINE__);
  }

  if (state.depth >= m_options.recursionLimit) {
    if (state.diagnostics) {
      state.diagnostics->error(LDrawDiagnosticCode::MissingSubfile, state.currentFile,
                               reference.lineNumber,
                               "subfile recursion limit exceeded while resolving '" +
                                 reference.filename + "'");
    }
    throw Exception("LDraw subfile recursion limit exceeded while resolving: " + reference.filename,
                    __FILE__, __LINE__);
  }

  const string fileKey = m_resolver->cacheKey(reference.filename);
  if (state.activeFiles.find(fileKey) != state.activeFiles.end()) {
    if (state.diagnostics) {
      state.diagnostics->error(LDrawDiagnosticCode::MissingSubfile, state.currentFile,
                               reference.lineNumber,
                               "subfile cycle detected while resolving '" + reference.filename +
                                 "'");
    }
    throw Exception("LDraw subfile cycle detected while resolving: " + reference.filename,
                    __FILE__, __LINE__);
  }

  const string compiledKey =
    fileKey + "|" + colorContextKey(context) + "|" + (inheritedInverted ? "inverted" : "normal");
  auto compiled = m_compiledSubfiles.find(compiledKey);
  if (compiled != m_compiledSubfiles.end()) {
    ++m_cacheStats.compiledSubfileHits;
    return compiled->second;
  }
  ++m_cacheStats.compiledSubfileMisses;

  auto parsed = m_parsedSubfiles.find(fileKey);
  if (parsed == m_parsedSubfiles.end()) {
    ++m_cacheStats.parsedSubfileMisses;
    auto input = m_resolver->open(reference.filename);
    if (!input) {
      if (state.diagnostics) {
        LDrawDiagnostic diagnostic;
        diagnostic.severity = LDrawDiagnosticSeverity::Error;
        diagnostic.code = LDrawDiagnosticCode::MissingSubfile;
        diagnostic.file = state.currentFile;
        diagnostic.lineNumber = reference.lineNumber;
        diagnostic.message = "resolver could not open subfile";
        diagnostic.reference = reference.filename;
        diagnostic.searchedRoots = m_resolver->searchRoots(reference.filename);
        state.diagnostics->add(std::move(diagnostic));
      }
      if (m_options.missingPartPolicy == MissingPartPolicy::Skip) {
        return make_shared<render::Composite>();
      }
      throw Exception("LDraw resolver could not open subfile: " + reference.filename,
                      __FILE__, __LINE__);
    }
    parsed = m_parsedSubfiles.emplace(fileKey, LDrawParser().parse(*input)).first;
  }

  state.activeFiles.insert(fileKey);
  ++state.depth;
  const string previousFile = state.currentFile;
  const string previousMpdBlock = state.currentMpdBlock;
  state.currentFile = reference.filename;
  state.currentMpdBlock = fileKey.rfind("mpd:", 0) == 0 ? reference.filename : string();
  auto result = compileCommands(parsed->second, colors, context, state, inheritedInverted);
  state.currentFile = previousFile;
  state.currentMpdBlock = previousMpdBlock;
  --state.depth;
  state.activeFiles.erase(fileKey);

  m_compiledSubfiles.emplace(compiledKey, result);
  return result;
}

shared_ptr<render::Composite> LDrawGeometryCompiler::compile(istream& input,
                                                             const LDrawColorTable& colors,
                                                             const LDrawColorContext& context) const {
  const auto document = LDrawParser().parseDocument(input);
  if (!document.isMultipart())
    return compile(document.mainFile().commands, colors, context);

  auto resolver = make_shared<LDrawMpdFileResolver>(document, m_resolver);
  LDrawGeometryCompiler compiler(resolver, m_options);
  CompileState state;
  state.currentFile = "<input>";
  state.currentMpdBlock = document.mainFile().filename;
  return compiler.compileCommands(document.mainFile().commands, colors, context, state, false);
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compile(istream& input, const LDrawColorTable& colors,
                               LDrawDiagnostics& diagnostics,
                               const LDrawColorContext& context) const {
  try {
    const auto document = LDrawParser().parseDocument(input);
    if (!document.isMultipart())
      return compile(document.mainFile().commands, colors, diagnostics, context);

    auto resolver = make_shared<LDrawMpdFileResolver>(document, m_resolver);
    LDrawGeometryCompiler compiler(resolver, m_options);
    CompileState state;
    state.currentFile = "<input>";
    state.currentMpdBlock = document.mainFile().filename;
    state.diagnostics = &diagnostics;
    return compiler.compileCommands(document.mainFile().commands, colors, context, state, false);
  } catch (const LDrawParseError& error) {
    const LDrawDiagnosticCode code =
      error.message().find("invalid integer for color") != string::npos
        ? LDrawDiagnosticCode::DirectColorParseFailure
        : LDrawDiagnosticCode::FatalParseError;
    diagnostics.error(code, "<input>", error.sourceLineNumber(), error.message());
    throw;
  }
}

string LDrawGeometryCompiler::colorContextKey(const LDrawColorContext& context) {
  return colorReferenceKey(context.currentColor) + "|" + colorReferenceKey(context.edgeColor);
}

LDrawGeometryCompiler::CacheStats LDrawGeometryCompiler::cacheStats() const {
  return m_cacheStats;
}

void LDrawGeometryCompiler::resetCacheStats() const {
  m_cacheStats = CacheStats();
}
