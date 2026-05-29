#include "engine/raster/detail/OpenGLRasterResourceCache.h"

#include "engine/raster/detail/OpenGLRasterShaderSources.h"

#include <QOpenGLShader>
#include <QOpenGLShaderProgram>

#include <stdexcept>

namespace engine::raster::detail {
  bool OpenGLRasterAttributeLocations::resolved() const {
    return position >= 0 && worldPosition >= 0 && normal >= 0 && color >= 0 && uv >= 0 &&
           alphaScale >= 0 && materialDiffuse >= 0 && materialSpecularColor >= 0 &&
           materialSpecularCoefficient >= 0 && materialSpecularExponent >= 0 &&
           ambientLighting >= 0 && directLighting >= 0 && specular >= 0 && albedoMode >= 0;
  }

  OpenGLRasterResourceCache::OpenGLRasterResourceCache() = default;

  OpenGLRasterResourceCache::~OpenGLRasterResourceCache() {
    if (!program) {
      return;
    }
    if (context.makeCurrent()) {
      program.reset();
      context.doneCurrent();
    } else {
      program.reset();
    }
  }

  void OpenGLRasterResourceCache::ensureProgram() {
    if (program && program->isLinked() && locations.resolved()) {
      return;
    }

    program = std::make_unique<QOpenGLShaderProgram>();
    if (!program->addShaderFromSourceCode(QOpenGLShader::Vertex, kOpenGLRasterVertexShader)) {
      throw std::runtime_error("OpenGL raster backend could not compile vertex shader: " +
                               program->log().toStdString());
    }
    if (!program->addShaderFromSourceCode(QOpenGLShader::Fragment, kOpenGLRasterFragmentShader)) {
      throw std::runtime_error("OpenGL raster backend could not compile fragment shader: " +
                               program->log().toStdString());
    }
    if (!program->link()) {
      throw std::runtime_error("OpenGL raster backend could not link shader program: " +
                               program->log().toStdString());
    }

    locations.position = program->attributeLocation("position");
    locations.worldPosition = program->attributeLocation("worldPosition");
    locations.normal = program->attributeLocation("normal");
    locations.color = program->attributeLocation("color");
    locations.uv = program->attributeLocation("uv");
    locations.alphaScale = program->attributeLocation("alphaScale");
    locations.materialDiffuse = program->attributeLocation("materialDiffuse");
    locations.materialSpecularColor = program->attributeLocation("materialSpecularColor");
    locations.materialSpecularCoefficient =
      program->attributeLocation("materialSpecularCoefficient");
    locations.materialSpecularExponent = program->attributeLocation("materialSpecularExponent");
    locations.ambientLighting = program->attributeLocation("ambientLighting");
    locations.directLighting = program->attributeLocation("directLighting");
    locations.specular = program->attributeLocation("specular");
    locations.albedoMode = program->attributeLocation("albedoMode");

    if (!locations.resolved()) {
      program.reset();
      throw std::runtime_error("OpenGL raster backend shader attributes are unavailable");
    }
  }
}
