#include "engine/raster/detail/OpenGLRasterMesh.h"

#include "engine/raster/Rasterizer.h"
#include "engine/raster/detail/RasterPipelineTypes.h"
#include "engine/raster/detail/RasterShadowMaps.h"
#include "engine/raster/detail/RasterTriangleEmitter.h"
#include "render/State.h"
#include "render/cameras/Camera.h"
#include "render/lights/Light.h"
#include "render/primitives/Scene.h"
#include "render/viewplanes/ViewPlane.h"

#include <algorithm>
#include <cmath>
#include <utility>

namespace engine::raster::detail {
  namespace {
    float normalizedDeviceX(double screenX, const Recti& rect) {
      if (rect.width() <= 0) {
        return 0.0f;
      }
      return static_cast<float>(
        ((screenX - rect.left()) / static_cast<double>(rect.width())) * 2.0 - 1.0);
    }

    float normalizedDeviceY(double screenY, const Recti& rect) {
      if (rect.height() <= 0) {
        return 0.0f;
      }
      return static_cast<float>(
        1.0 - ((screenY - rect.top()) / static_cast<double>(rect.height())) * 2.0);
    }

    float normalizedDeviceDepth(const RasterVertex& vertex, double depthBias) {
      if (vertex.invW == 0.0) {
        return 0.0f;
      }
      const double depth = vertex.depthOverW / vertex.invW + depthBias;
      if (!std::isfinite(depth)) {
        return 0.0f;
      }
      const double normalized = depth / (depth + 1.0);
      return static_cast<float>(std::clamp(normalized * 2.0 - 1.0, -1.0, 1.0));
    }

    float clipW(const RasterVertex& vertex) {
      if (vertex.invW <= 0.0 || !std::isfinite(vertex.invW)) {
        return 1.0f;
      }
      return static_cast<float>(1.0 / vertex.invW);
    }

    float shaderMode(const RasterAlbedoShaderSource& source) {
      const RasterAlbedoShaderMode mode = source.mode;
      return static_cast<float>(static_cast<int>(mode));
    }

    float nonnegativeComponent(double value) {
      return static_cast<float>(std::max(0.0, value));
    }
  }

  bool OpenGLRasterMesh::empty() const {
    return m_indices.empty();
  }

  std::size_t OpenGLRasterMesh::triangleCount() const {
    return m_indices.size() / 3;
  }

  const OpenGLRasterMesh::Vertices& OpenGLRasterMesh::vertices() const {
    return m_vertices;
  }

  const OpenGLRasterMesh::Indices& OpenGLRasterMesh::indices() const {
    return m_indices;
  }

  const OpenGLRasterMesh::Batches& OpenGLRasterMesh::batches() const {
    return m_batches;
  }

  const OpenGLRasterMesh::DirectionalLights& OpenGLRasterMesh::directionalLights() const {
    return m_directionalLights;
  }

  const OpenGLRasterMesh::PointLights& OpenGLRasterMesh::pointLights() const {
    return m_pointLights;
  }

  void OpenGLRasterMesh::appendTriangle(const Vertex& v0, const Vertex& v1, const Vertex& v2,
                                        const RasterAlbedoShaderSource& albedo) {
    const auto base = static_cast<std::uint32_t>(m_vertices.size());
    const auto indexOffset = m_indices.size();
    m_vertices.push_back(v0);
    m_vertices.push_back(v1);
    m_vertices.push_back(v2);
    m_indices.push_back(base);
    m_indices.push_back(base + 1);
    m_indices.push_back(base + 2);
    if (!m_batches.empty() && m_batches.back().albedo == albedo) {
      m_batches.back().indexCount += 3;
      return;
    }
    m_batches.push_back({indexOffset, 3, albedo});
  }

  void OpenGLRasterMesh::addDirectionalLight(const DirectionalLight& light) {
    m_directionalLights.push_back(light);
  }

  void OpenGLRasterMesh::addPointLight(const PointLight& light) {
    m_pointLights.push_back(light);
  }

  OpenGLRasterMeshBuilder::OpenGLRasterMeshBuilder(
    const render::Scene* scene, std::shared_ptr<render::Camera> camera, int lod,
    const Recti& viewportRect, Rasterizer::CullMode cullMode, bool hasCullModeOverride,
    const std::atomic<bool>& cancelled, const ShadowMaps* shadowMaps, double depthBias)
      : m_scene(scene),
        m_camera(std::move(camera)),
        m_lod(lod),
        m_viewportRect(viewportRect),
        m_cullMode(cullMode),
        m_hasCullModeOverride(hasCullModeOverride),
        m_cancelled(cancelled),
        m_shadowMaps(shadowMaps),
        m_depthBias(std::isfinite(depthBias) ? depthBias : 0.0) {
  }

  OpenGLRasterMesh OpenGLRasterMeshBuilder::build() const {
    OpenGLRasterMesh mesh;
    if (!m_scene || !m_camera || !m_camera->viewPlane()) {
      return mesh;
    }

    m_camera->viewPlane()->setup(m_camera->matrix(), m_viewportRect);
    Rasterizer rasterizer(m_camera, std::shared_ptr<render::Scene>());
    rasterizer.setLod(m_lod);
    RasterTriangleEmitter emitter(m_scene, m_camera, m_lod, rasterizer, m_cancelled, m_cullMode,
                                  m_hasCullModeOverride, false);
    appendDirectionalLights(mesh);
    appendPointLights(mesh);
    emitter.forEachTriangle([&](const RasterTriangle& triangle) {
      const RasterAlbedoShaderSource albedo = triangle.rasterMaterial.shaderAlbedoSource();
      mesh.appendTriangle(vertexFor(triangle, triangle.vertices[0]),
                          vertexFor(triangle, triangle.vertices[1]),
                          vertexFor(triangle, triangle.vertices[2]), albedo);
    });

    return mesh;
  }

  void OpenGLRasterMeshBuilder::appendDirectionalLights(OpenGLRasterMesh& mesh) const {
    if (!m_scene || !usesFragmentShaderLighting()) {
      return;
    }

    for (const auto& light : m_scene->lights()) {
      const auto direction = light->directionalShadowMapDirection();
      if (!direction) {
        continue;
      }
      const Colord radiance = light->radiance();
      mesh.addDirectionalLight(
        {static_cast<float>(direction->x()), static_cast<float>(direction->y()),
         static_cast<float>(direction->z()), nonnegativeComponent(radiance.r()),
         nonnegativeComponent(radiance.g()), nonnegativeComponent(radiance.b())});
    }
  }

  void OpenGLRasterMeshBuilder::appendPointLights(OpenGLRasterMesh& mesh) const {
    if (!m_scene || !usesFragmentShaderLighting()) {
      return;
    }

    for (const auto& light : m_scene->lights()) {
      const auto position = light->positionalLightPosition();
      if (!position) {
        continue;
      }
      const Colord radiance = light->radiance();
      mesh.addPointLight({static_cast<float>(position->x()), static_cast<float>(position->y()),
                          static_cast<float>(position->z()), nonnegativeComponent(radiance.r()),
                          nonnegativeComponent(radiance.g()), nonnegativeComponent(radiance.b())});
    }
  }

  OpenGLRasterMesh::Vertex OpenGLRasterMeshBuilder::vertexFor(const RasterTriangle& triangle,
                                                              const RasterVertex& vertex) const {
    const Colord albedo = triangle.rasterMaterial.albedo(
      triangle.primitive, vertex.point, vertex.normal, vertex.uv, triangle.uvDx, triangle.uvDy);
    const double alpha = triangle.rasterMaterial.alpha(
      triangle.primitive, vertex.point, vertex.normal, vertex.uv, triangle.uvDx, triangle.uvDy);
    const Vector3d normal = lightingNormalFor(triangle, vertex);
    const Colord ambientLighting = ambientLightingFor(triangle);
    const Colord directLighting = directLightingFor(triangle, vertex, normal);
    const Colord specular = specularFor(triangle, vertex, normal);
    const RasterAlbedoShaderSource shaderSource = triangle.rasterMaterial.shaderAlbedoSource();
    return {normalizedDeviceX(vertex.x, m_viewportRect),
            normalizedDeviceY(vertex.y, m_viewportRect),
            normalizedDeviceDepth(vertex, m_depthBias),
            clipW(vertex),
            static_cast<float>(vertex.point.x()),
            static_cast<float>(vertex.point.y()),
            static_cast<float>(vertex.point.z()),
            static_cast<float>(normal.x()),
            static_cast<float>(normal.y()),
            static_cast<float>(normal.z()),
            static_cast<float>(std::clamp(albedo.r(), 0.0, 1.0)),
            static_cast<float>(std::clamp(albedo.g(), 0.0, 1.0)),
            static_cast<float>(std::clamp(albedo.b(), 0.0, 1.0)),
            static_cast<float>(std::clamp(alpha, 0.0, 1.0)),
            static_cast<float>(vertex.uv.x()),
            static_cast<float>(vertex.uv.y()),
            static_cast<float>(std::clamp(triangle.rasterMaterial.materialAlpha(), 0.0, 1.0)),
            nonnegativeComponent(triangle.rasterMaterial.diffuseCoefficient()),
            nonnegativeComponent(triangle.rasterMaterial.specularColor().r()),
            nonnegativeComponent(triangle.rasterMaterial.specularColor().g()),
            nonnegativeComponent(triangle.rasterMaterial.specularColor().b()),
            nonnegativeComponent(triangle.rasterMaterial.specularCoefficient()),
            nonnegativeComponent(triangle.rasterMaterial.specularExponent()),
            nonnegativeComponent(ambientLighting.r()),
            nonnegativeComponent(ambientLighting.g()),
            nonnegativeComponent(ambientLighting.b()),
            nonnegativeComponent(directLighting.r()),
            nonnegativeComponent(directLighting.g()),
            nonnegativeComponent(directLighting.b()),
            nonnegativeComponent(specular.r()),
            nonnegativeComponent(specular.g()),
            nonnegativeComponent(specular.b()),
            shaderMode(shaderSource)};
  }

  bool OpenGLRasterMeshBuilder::usesFragmentShaderLighting() const {
    return m_shadowMaps == nullptr;
  }

  bool OpenGLRasterMeshBuilder::shadesInFragmentShader(const render::Light& light) const {
    return usesFragmentShaderLighting() && (light.directionalShadowMapDirection().has_value() ||
                                            light.positionalLightPosition().has_value());
  }

  Vector3d OpenGLRasterMeshBuilder::lightingNormalFor(const RasterTriangle& triangle,
                                                      const RasterVertex& vertex) const {
    const Vector3d baseNormal = vertex.normal.normalized();
    return triangle.rasterMaterial.lightingNormal(triangle.primitive, vertex.point, baseNormal,
                                                  vertex.uv, triangle.uvDx, triangle.uvDy,
                                                  triangle.tangentFrame);
  }

  Colord OpenGLRasterMeshBuilder::ambientLightingFor(const RasterTriangle& triangle) const {
    if (!m_scene) {
      return Colord::white();
    }
    return m_scene->ambient() * triangle.rasterMaterial.ambientCoefficient();
  }

  Colord OpenGLRasterMeshBuilder::directLightingFor(const RasterTriangle& triangle,
                                                    const RasterVertex& vertex,
                                                    const Vector3d& normal) const {
    if (!m_scene) {
      return Colord::black();
    }

    Colord lighting = Colord::black();
    for (const auto& light : m_scene->lights()) {
      if (shadesInFragmentShader(*light)) {
        continue;
      }
      const Vector3d lightDir = light->direction(vertex.point);
      const double nDotL = std::max(0.0, normal * lightDir);
      if (nDotL > 0.0) {
        lighting += light->radiance() * triangle.rasterMaterial.diffuseCoefficient() * nDotL *
                    visibilityFor(*light, vertex, normal, lightDir);
      }
    }
    return lighting;
  }

  Colord OpenGLRasterMeshBuilder::specularFor(const RasterTriangle& triangle,
                                              const RasterVertex& vertex,
                                              const Vector3d& normal) const {
    if (!m_scene || !m_camera || !triangle.rasterMaterial.hasSpecular()) {
      return Colord::black();
    }

    const Vector3d viewDir = (-m_camera->rayForPixel(vertex.x, vertex.y).direction()).normalized();
    Colord specular = Colord::black();
    for (const auto& light : m_scene->lights()) {
      if (shadesInFragmentShader(*light)) {
        continue;
      }
      const Vector3d lightDir = light->direction(vertex.point);
      const double nDotL = std::max(0.0, normal * lightDir);
      if (nDotL <= 0.0) {
        continue;
      }

      const double visibility = visibilityFor(*light, vertex, normal, lightDir);
      if (visibility <= 0.0) {
        continue;
      }

      const Vector3d lobeDirection = (-lightDir + normal * 2.0 * nDotL).normalized();
      const double lobeDotView = std::max(0.0, lobeDirection * viewDir);
      if (lobeDotView > 0.0) {
        specular += triangle.rasterMaterial.specularColor() *
                    triangle.rasterMaterial.specularCoefficient() *
                    std::pow(lobeDotView, triangle.rasterMaterial.specularExponent()) *
                    light->radiance() * nDotL * visibility;
      }
    }
    return specular;
  }

  double OpenGLRasterMeshBuilder::visibilityFor(const render::Light& light,
                                                const RasterVertex& vertex, const Vector3d& normal,
                                                const Vector3d& lightDir) const {
    if (!m_scene || !m_shadowMaps) {
      return 1.0;
    }

    if (const DirectionalShadowMap* shadowMap = m_shadowMaps->forLight(&light)) {
      return shadowMap->visibility(vertex.point, normal, lightDir);
    }

    render::State state;
    return m_scene->intersects(Rayd(vertex.point, lightDir).epsilonShifted(), state) ? 0.0 : 1.0;
  }
}
