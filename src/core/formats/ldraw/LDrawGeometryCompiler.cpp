#include "core/formats/ldraw/LDrawGeometryCompiler.h"

#include "core/Exception.h"
#include "core/geometry/Mesh.h"
#include "core/math/Matrix.h"
#include "render/primitives/Composite.h"
#include "render/primitives/Instance.h"
#include "render/primitives/MeshPrimitive.h"

#include <iomanip>
#include <memory>
#include <sstream>
#include <string>
#include <utility>

using namespace std;

namespace {
  shared_ptr<render::MeshPrimitive> meshPrimitiveForTriangle(const LDrawTriangle& triangle,
                                                            const LDrawColorTable& colors,
                                                            const LDrawColorContext& context) {
    Mesh mesh;
    for (const auto& point : triangle.points)
      mesh.addVertex(point, Vector3d::null);
    mesh.addFace({0, 1, 2});
    mesh.computeNormals();

    auto primitive = make_shared<render::MeshPrimitive>(std::move(mesh),
                                                        render::MeshPrimitive::NormalMode::Flat);
    primitive->setMaterial(colors.materialForCode(triangle.color, context));
    return primitive;
  }

  shared_ptr<render::MeshPrimitive> meshPrimitiveForQuad(const LDrawQuad& quad,
                                                        const LDrawColorTable& colors,
                                                        const LDrawColorContext& context) {
    Mesh mesh;
    for (const auto& point : quad.points)
      mesh.addVertex(point, Vector3d::null);
    mesh.addFace({0, 1, 2, 3});
    mesh.computeNormals();

    auto primitive = make_shared<render::MeshPrimitive>(std::move(mesh),
                                                        render::MeshPrimitive::NormalMode::Flat);
    primitive->setMaterial(colors.materialForCode(quad.color, context));
    return primitive;
  }

  Matrix4d transformForSubfileReference(const LDrawSubfileReference& reference) {
    const auto& m = reference.matrix;
    return Matrix4d(m[0], m[1], m[2], reference.translation.x(),
                    m[3], m[4], m[5], reference.translation.y(),
                    m[6], m[7], m[8], reference.translation.z(),
                    0.0, 0.0, 0.0, 1.0);
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
                                             int recursionLimit)
    : m_resolver(std::move(resolver)),
      m_recursionLimit(recursionLimit) {
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
                               const LDrawColorContext& context) const {
  CompileState state;
  return compileCommands(commands, colors, context, state);
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compileCommands(const LDrawParser::Commands& commands,
                                       const LDrawColorTable& colors,
                                       const LDrawColorContext& context,
                                       CompileState& state) const {
  auto result = make_shared<render::Composite>();

  for (const auto& command : commands) {
    if (holds_alternative<LDrawTriangle>(command)) {
      result->add(meshPrimitiveForTriangle(get<LDrawTriangle>(command), colors, context));
    } else if (holds_alternative<LDrawQuad>(command)) {
      result->add(meshPrimitiveForQuad(get<LDrawQuad>(command), colors, context));
    } else if (holds_alternative<LDrawSubfileReference>(command)) {
      const auto& reference = get<LDrawSubfileReference>(command);
      auto instance = make_shared<render::Instance>(
        compileSubfile(reference, colors, colors.contextForSubfile(reference.color, context), state));
      instance->setMatrix(transformForSubfileReference(reference));
      result->add(instance);
    }
  }

  return result;
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compileSubfile(const LDrawSubfileReference& reference,
                                      const LDrawColorTable& colors,
                                      const LDrawColorContext& context,
                                      CompileState& state) const {
  if (!m_resolver) {
    throw Exception("LDraw subfile reference requires an LDrawFileResolver: " + reference.filename,
                    __FILE__, __LINE__);
  }

  if (state.depth >= m_recursionLimit) {
    throw Exception("LDraw subfile recursion limit exceeded while resolving: " + reference.filename,
                    __FILE__, __LINE__);
  }

  const string fileKey = m_resolver->cacheKey(reference.filename);
  if (state.activeFiles.find(fileKey) != state.activeFiles.end()) {
    throw Exception("LDraw subfile cycle detected while resolving: " + reference.filename,
                    __FILE__, __LINE__);
  }

  const string compiledKey = fileKey + "|" + colorContextKey(context);
  auto compiled = m_compiledSubfiles.find(compiledKey);
  if (compiled != m_compiledSubfiles.end())
    return compiled->second;

  auto parsed = m_parsedSubfiles.find(fileKey);
  if (parsed == m_parsedSubfiles.end()) {
    auto input = m_resolver->open(reference.filename);
    if (!input) {
      throw Exception("LDraw resolver could not open subfile: " + reference.filename,
                      __FILE__, __LINE__);
    }
    parsed = m_parsedSubfiles.emplace(fileKey, LDrawParser().parse(*input)).first;
  }

  state.activeFiles.insert(fileKey);
  ++state.depth;
  auto result = compileCommands(parsed->second, colors, context, state);
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
  LDrawGeometryCompiler compiler(resolver, m_recursionLimit);
  return compiler.compile(document.mainFile().commands, colors, context);
}

string LDrawGeometryCompiler::colorContextKey(const LDrawColorContext& context) {
  return colorReferenceKey(context.currentColor) + "|" + colorReferenceKey(context.edgeColor);
}
