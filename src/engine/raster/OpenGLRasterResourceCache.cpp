#include "engine/raster/detail/OpenGLRasterResourceCache.h"

#include "engine/raster/detail/OpenGLRasterShaderSources.h"
#include "engine/raster/gl/createContext.h"
#include "render/textures/ImageTexture.h"

#include <QOpenGLContext>
#include <QOpenGLFunctions>

#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace engine::raster::detail {
  namespace {
    GLint glMinFilter(const render::ImageTexture& image) {
      switch (image.filter()) {
      case render::ImageTextureFilter::Nearest:
        return GL_NEAREST;
      case render::ImageTextureFilter::Bilinear:
        return GL_LINEAR;
      case render::ImageTextureFilter::Mipmap:
        return GL_LINEAR_MIPMAP_LINEAR;
      }
      return GL_LINEAR;
    }

    GLint glMagFilter(const render::ImageTexture& image) {
      return image.filter() == render::ImageTextureFilter::Nearest ? GL_NEAREST : GL_LINEAR;
    }

    GLint glWrapMode(const render::ImageTexture& image) {
      return image.wrap() == render::ImageTextureWrap::Clamp ? GL_CLAMP_TO_EDGE : GL_REPEAT;
    }

    std::vector<GLfloat> texturePixels(const std::vector<Colord>& colors) {
      std::vector<GLfloat> pixels;
      pixels.reserve(colors.size() * 4);
      for (const Colord& color : colors) {
        pixels.push_back(static_cast<GLfloat>(color.r()));
        pixels.push_back(static_cast<GLfloat>(color.g()));
        pixels.push_back(static_cast<GLfloat>(color.b()));
        pixels.push_back(1.0f);
      }
      return pixels;
    }
  }

  std::uint32_t OpenGLRasterImageTextureCache::textureFor(const render::ImageTexture& image,
                                                          QOpenGLFunctions* functions) {
    const auto cached = textures.find(&image);
    if (cached != textures.end()) {
      return cached->second;
    }

    GLuint texture = 0;
    functions->glGenTextures(1, &texture);
    if (texture == 0) {
      throw std::runtime_error("OpenGL raster backend could not allocate an image texture");
    }

    functions->glBindTexture(GL_TEXTURE_2D, texture);
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, glMinFilter(image));
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, glMagFilter(image));
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, glWrapMode(image));
    functions->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, glWrapMode(image));
    functions->glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    for (int level = 0; level != image.mipLevelCount(); ++level) {
      const std::vector<GLfloat> pixels = texturePixels(image.pixels(level));
      functions->glTexImage2D(GL_TEXTURE_2D, level, GL_RGBA, image.width(level),
                              image.height(level), 0, GL_RGBA, GL_FLOAT, pixels.data());
    }
    functions->glBindTexture(GL_TEXTURE_2D, 0);
    textures.emplace(&image, texture);
    return texture;
  }

  void OpenGLRasterImageTextureCache::releaseAll(QOpenGLFunctions* functions) {
    for (const auto& entry : textures) {
      GLuint texture = entry.second;
      functions->glDeleteTextures(1, &texture);
    }
    textures.clear();
  }

  bool OpenGLMeshCacheKey::sceneMatches(const std::shared_ptr<const render::Scene>& other) const {
    const auto locked = scene.lock();
    return locked && locked.get() == other.get();
  }

  bool OpenGLMeshCacheKey::matches(const OpenGLMeshCacheKey& other) const {
    const auto a = scene.lock();
    const auto b = other.scene.lock();
    if (!a || a.get() != b.get()) {
      return false;
    }
    if (visibilitySet.lock().get() != other.visibilitySet.lock().get() ||
        shadowMaps.lock().get() != other.shadowMaps.lock().get() || lod != other.lod ||
        viewportWidth != other.viewportWidth || viewportHeight != other.viewportHeight ||
        cullMode != other.cullMode || hasCullModeOverride != other.hasCullModeOverride ||
        depthBias != other.depthBias || cameraDependent != other.cameraDependent) {
      return false;
    }
    if (cameraDependent &&
        (cameraPosition != other.cameraPosition || cameraTarget != other.cameraTarget)) {
      return false;
    }
    return true;
  }

  bool OpenGLRasterAttributeLocations::resolved() const {
    return position >= 0 && worldPosition >= 0 && normal >= 0 && color >= 0 && uv >= 0 &&
           alphaScale >= 0 && materialDiffuse >= 0 && materialSpecularColor >= 0 &&
           materialSpecularCoefficient >= 0 && materialSpecularExponent >= 0 &&
           ambientLighting >= 0 && directLighting >= 0 && specular >= 0 && albedoMode >= 0;
  }

  OpenGLRasterResourceCache::OpenGLRasterResourceCache()
      : context(gl::createOffscreenContext()) {
  }

  OpenGLRasterResourceCache::MeshSlotResult
  OpenGLRasterResourceCache::acquireMeshSlot(const OpenGLMeshCacheKey& key) {
    // Hit pass: linear scan (n=4, small) for a matching key.
    for (std::size_t i = 0; i < meshCache.size(); ++i) {
      auto& entry = meshCache[i];
      if (entry.lastUsed != 0 && entry.key && entry.key->matches(key)) {
        entry.lastUsed = ++meshUseTick;
        return {&entry, static_cast<std::ptrdiff_t>(i), true};
      }
    }
    // Miss pass: pick the LRU slot (smallest lastUsed, with 0 = empty
    // counting as smallest). Empty slots win, then the oldest hit.
    std::size_t victim = 0;
    for (std::size_t i = 1; i < meshCache.size(); ++i) {
      if (meshCache[i].lastUsed < meshCache[victim].lastUsed) {
        victim = i;
      }
    }
    auto& entry = meshCache[victim];
    entry.key.reset();
    entry.mesh = OpenGLRasterMesh();
    entry.lastUsed = ++meshUseTick;
    return {&entry, static_cast<std::ptrdiff_t>(victim), false};
  }

  OpenGLRasterResourceCache::~OpenGLRasterResourceCache() {
    // The process-wide shared cache (`OpenGLRasterizer::sharedResources`)
    // is intentionally leaked and never runs this destructor — Qt's
    // own static state is destroyed before us by `__cxa_finalize_ranges`,
    // and any GL cleanup at that point crashes inside the per-context
    // resource lookup. This path therefore only runs for short-lived
    // caches (e.g. the fallback rasterizer creates a fresh cache when
    // it cannot migrate the shared context to its thread) whose owning
    // thread is still alive.
    const bool hasTextures = imageTextures && !imageTextures->textures.empty();
    const bool needsContext = program || hasTextures || vertexBuffer || indexBuffer;
    if (!needsContext) {
      return;
    }
    context->migrateToCurrentThread();
    if (context->makeCurrent()) {
      if (imageTextures) {
        QOpenGLFunctions* functions = QOpenGLContext::currentContext()->functions();
        imageTextures->releaseAll(functions);
      }
      vertexBuffer.reset();
      indexBuffer.reset();
      program.reset();
      context->doneCurrent();
    } else {
      // No usable context — leak the GL handles. The OS reclaims them.
      (void)vertexBuffer.release();
      (void)indexBuffer.release();
      (void)program.release();
      if (imageTextures) {
        imageTextures->textures.clear();
      }
    }
    imageTextures.reset();
  }

  void OpenGLRasterResourceCache::ensureProgram() {
    if (program && program->isLinked() && locations.resolved()) {
      return;
    }

    program = std::make_unique<gl::ShaderProgram>();
    if (!program->addShaderFromSourceCode(gl::ShaderProgram::ShaderStage::Vertex,
                                          kOpenGLRasterVertexShader)) {
      throw std::runtime_error("OpenGL raster backend could not compile vertex shader: " +
                               program->errorLog());
    }
    if (!program->addShaderFromSourceCode(gl::ShaderProgram::ShaderStage::Fragment,
                                          kOpenGLRasterFragmentShader)) {
      throw std::runtime_error("OpenGL raster backend could not compile fragment shader: " +
                               program->errorLog());
    }
    if (!program->link()) {
      throw std::runtime_error("OpenGL raster backend could not link shader program: " +
                               program->errorLog());
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

  void OpenGLRasterResourceCache::ensureMeshBuffers() {
    if (!vertexBuffer) {
      vertexBuffer = std::make_unique<gl::Buffer>(gl::Buffer::Target::Vertex);
      if (!vertexBuffer->create()) {
        vertexBuffer.reset();
        throw std::runtime_error("OpenGL raster backend could not create a vertex buffer object");
      }
    }
    if (!indexBuffer) {
      indexBuffer = std::make_unique<gl::Buffer>(gl::Buffer::Target::Index);
      if (!indexBuffer->create()) {
        indexBuffer.reset();
        throw std::runtime_error("OpenGL raster backend could not create an index buffer object");
      }
    }
  }
}
