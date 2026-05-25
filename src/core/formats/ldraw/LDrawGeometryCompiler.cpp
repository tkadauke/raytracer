#include "core/formats/ldraw/LDrawGeometryCompiler.h"

#include "core/geometry/Mesh.h"
#include "render/primitives/Composite.h"
#include "render/primitives/MeshPrimitive.h"

#include <memory>
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
}

shared_ptr<render::Composite>
LDrawGeometryCompiler::compile(const LDrawParser::Commands& commands, const LDrawColorTable& colors,
                               const LDrawColorContext& context) const {
  auto result = make_shared<render::Composite>();

  for (const auto& command : commands) {
    if (holds_alternative<LDrawTriangle>(command)) {
      result->add(meshPrimitiveForTriangle(get<LDrawTriangle>(command), colors, context));
    } else if (holds_alternative<LDrawQuad>(command)) {
      result->add(meshPrimitiveForQuad(get<LDrawQuad>(command), colors, context));
    }
  }

  return result;
}

shared_ptr<render::Composite> LDrawGeometryCompiler::compile(istream& input,
                                                             const LDrawColorTable& colors,
                                                             const LDrawColorContext& context) const {
  return compile(LDrawParser().parse(input), colors, context);
}
