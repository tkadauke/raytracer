#include "world/objects/RenderIntentElement.h"

#include "engine/graph/RenderGraphTypes.h"
#include "engine/raster/RasterBackend.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"
#include "world/objects/Scene.h"

#include <utility>

RenderIntentElement::RenderIntentElement(Scene* parent)
    : Element(parent) {
  setName(QStringLiteral("Render Settings"));
  setGenerated(true);
}

bool RenderIntentElement::displayInSceneModel() const {
  return true;
}

bool RenderIntentElement::isPropertyVisible(const QString& propertyName) const {
  if (propertyName == QStringLiteral("name"))
    return false;
  if (propertyName == QStringLiteral("raytracerViewPlane") ||
      propertyName == QStringLiteral("raytracerThreads") ||
      propertyName == QStringLiteral("raytracerQueueSize"))
    return false;

  const auto executor = intent().defaultExecutorKind();
  if (isRaytracerProperty(propertyName))
    return executor == engine::graph::RenderExecutorKind::Raytracer;
  if (isRasterizerShadowProperty(propertyName))
    return executor == engine::graph::RenderExecutorKind::Rasterizer && previewShadows();
  if (isRasterizerProperty(propertyName))
    return executor == engine::graph::RenderExecutorKind::Rasterizer;
  if (isWireframeProperty(propertyName))
    return executor == engine::graph::RenderExecutorKind::Wireframe;

  return true;
}

QString RenderIntentElement::propertyDisplayName(const QString& propertyName) const {
  if (propertyName == QStringLiteral("saveIntent"))
    return QStringLiteral("Save Render Settings");
  if (propertyName == QStringLiteral("defaultEngine"))
    return QStringLiteral("Default Engine");
  if (propertyName == QStringLiteral("viewMode"))
    return QStringLiteral("View Mode");
  if (propertyName == QStringLiteral("cameraId"))
    return QStringLiteral("Camera ID");
  if (propertyName == QStringLiteral("shadingProfile"))
    return QStringLiteral("Shading Profile");
  if (propertyName == QStringLiteral("automaticFeatures"))
    return QStringLiteral("Automatic Features");
  if (propertyName == QStringLiteral("wireframeOverlay"))
    return QStringLiteral("Wireframe Overlay");
  if (propertyName == QStringLiteral("curveOverlay"))
    return QStringLiteral("Curve Overlay");
  if (propertyName == QStringLiteral("previewShadows"))
    return QStringLiteral("Shadow Maps");
  if (propertyName == QStringLiteral("postProcessAA"))
    return QStringLiteral("Postprocess AA");
  if (propertyName == QStringLiteral("raytracerSampler"))
    return QStringLiteral("Sampler");
  if (propertyName == QStringLiteral("raytracerSamplesPerPixel"))
    return QStringLiteral("Samples Per Pixel");
  if (propertyName == QStringLiteral("raytracerMaxRecursionDepth"))
    return QStringLiteral("Max Recursion Depth");
  if (propertyName == QStringLiteral("raytracerViewPlane"))
    return QStringLiteral("View Plane");
  if (propertyName == QStringLiteral("raytracerThreads"))
    return QStringLiteral("Threads");
  if (propertyName == QStringLiteral("raytracerQueueSize"))
    return QStringLiteral("Queue Size");
  if (propertyName == QStringLiteral("rasterizerLod"))
    return QStringLiteral("LOD");
  if (propertyName == QStringLiteral("rasterizerBackend"))
    return QStringLiteral("Backend");
  if (propertyName == QStringLiteral("rasterizerMSAASamples"))
    return QStringLiteral("MSAA Samples");
  if (propertyName == QStringLiteral("rasterizerMSAAShading"))
    return QStringLiteral("MSAA Shading");
  if (propertyName == QStringLiteral("rasterizerShadowMapSize"))
    return QStringLiteral("Map Size");
  if (propertyName == QStringLiteral("rasterizerShadowCascades"))
    return QStringLiteral("Cascades");
  if (propertyName == QStringLiteral("rasterizerShadowBias"))
    return QStringLiteral("Bias");
  if (propertyName == QStringLiteral("rasterizerShadowFilterRadius"))
    return QStringLiteral("Filter Radius");
  if (propertyName == QStringLiteral("rasterizerShadowFilter"))
    return QStringLiteral("Filter");
  if (propertyName == QStringLiteral("wireframeLod"))
    return QStringLiteral("LOD");

  return Element::propertyDisplayName(propertyName);
}

QString RenderIntentElement::propertyDescription(const QString& propertyName) const {
  if (propertyName == QStringLiteral("rasterizerBackend"))
    return QStringLiteral(
      "CPU is the reference software rasterizer. OpenGL is experimental: it records and probes "
      "the graph-selected GPU backend, but the first mesh draw path is still incomplete.");
  return Element::propertyDescription(propertyName);
}

QString RenderIntentElement::propertyGroup(const QString& propertyName) const {
  if (isRaytracerProperty(propertyName))
    return QStringLiteral("Raytracer");
  if (isRasterizerShadowProperty(propertyName))
    return QStringLiteral("Shadow Maps");
  if (isRasterizerProperty(propertyName))
    return QStringLiteral("Rasterizer");
  if (isWireframeProperty(propertyName))
    return QStringLiteral("Wireframe");
  if (propertyName == QStringLiteral("postProcessAA"))
    return QStringLiteral("Postprocess");
  if (propertyName == QStringLiteral("wireframeOverlay") ||
      propertyName == QStringLiteral("curveOverlay"))
    return QStringLiteral("Overlays");
  return QStringLiteral("General");
}

QStringList RenderIntentElement::propertyChoices(const QString& propertyName) const {
  if (propertyName == QStringLiteral("defaultEngine"))
    return {QStringLiteral("raytracer"), QStringLiteral("rasterizer"), QStringLiteral("wireframe")};
  if (propertyName == QStringLiteral("viewMode")) {
    QStringList choices{QStringLiteral("default"),     QStringLiteral("beauty"),
                        QStringLiteral("wireframe"),   QStringLiteral("depth"),
                        QStringLiteral("stencil"),     QStringLiteral("stencil_composite"),
                        QStringLiteral("normal"),      QStringLiteral("object_id"),
                        QStringLiteral("material_id"), QStringLiteral("world_position")};
    if (intent().defaultExecutorKind() == engine::graph::RenderExecutorKind::Rasterizer) {
      choices << QStringLiteral("raster_coverage_count")
              << QStringLiteral("raster_depth_test_count")
              << QStringLiteral("raster_depth_pass_count") << QStringLiteral("raster_shade_count")
              << QStringLiteral("raster_color_write_count");
    }
    return choices;
  }
  if (propertyName == QStringLiteral("postProcessAA"))
    return {QStringLiteral("none"), QStringLiteral("fxaa"), QStringLiteral("smaa"),
            QStringLiteral("taa")};
  if (propertyName == QStringLiteral("raytracerSampler"))
    return raytracerSamplerChoices();
  if (propertyName == QStringLiteral("raytracerViewPlane"))
    return raytracerViewPlaneChoices();
  if (propertyName == QStringLiteral("rasterizerBackend"))
    return {QStringLiteral("cpu"), QStringLiteral("opengl")};
  if (propertyName == QStringLiteral("rasterizerMSAAShading"))
    return {QStringLiteral("per_sample"), QStringLiteral("per_fragment")};
  if (propertyName == QStringLiteral("rasterizerShadowFilter"))
    return {QStringLiteral("pcf"), QStringLiteral("pcss")};
  return {};
}

QList<int> RenderIntentElement::propertyIntChoices(const QString& propertyName) const {
  if (propertyName == QStringLiteral("rasterizerMSAASamples"))
    return {1, 2, 4, 8};
  return {};
}

std::optional<QPair<int, int>>
RenderIntentElement::propertyIntRange(const QString& propertyName) const {
  if (propertyName == QStringLiteral("raytracerSamplesPerPixel") ||
      propertyName == QStringLiteral("raytracerMaxRecursionDepth"))
    return QPair<int, int>(1, 1024);
  if (propertyName == QStringLiteral("raytracerThreads"))
    return QPair<int, int>(1, 1024);
  if (propertyName == QStringLiteral("raytracerQueueSize"))
    return QPair<int, int>(1, 16777216);
  if (propertyName == QStringLiteral("rasterizerLod") ||
      propertyName == QStringLiteral("wireframeLod"))
    return QPair<int, int>(0, 10);
  if (propertyName == QStringLiteral("rasterizerShadowMapSize"))
    return QPair<int, int>(1, 8192);
  if (propertyName == QStringLiteral("rasterizerShadowCascades"))
    return QPair<int, int>(1, 4);
  if (propertyName == QStringLiteral("rasterizerShadowFilterRadius"))
    return QPair<int, int>(0, 16);
  return std::nullopt;
}

std::optional<QPair<double, double>>
RenderIntentElement::propertyDoubleRange(const QString& propertyName) const {
  if (propertyName == QStringLiteral("rasterizerShadowBias"))
    return QPair<double, double>(0.0, 100.0);
  return std::nullopt;
}

QString RenderIntentElement::propertyChoiceDisplayName(const QString& propertyName,
                                                       const QString& choice) const {
  if (propertyName == QStringLiteral("postProcessAA"))
    return choice == QStringLiteral("none") ? QStringLiteral("None") : choice.toUpper();
  if (propertyName == QStringLiteral("viewMode")) {
    if (choice == QStringLiteral("object_id"))
      return QStringLiteral("Object ID");
    if (choice == QStringLiteral("material_id"))
      return QStringLiteral("Material ID");
    if (choice == QStringLiteral("raster_coverage_count"))
      return QStringLiteral("Raster Coverage Count");
    if (choice == QStringLiteral("raster_depth_test_count"))
      return QStringLiteral("Raster Depth-Test Count");
    if (choice == QStringLiteral("raster_depth_pass_count"))
      return QStringLiteral("Raster Depth-Pass Count");
    if (choice == QStringLiteral("raster_shade_count"))
      return QStringLiteral("Raster Shade Count");
    if (choice == QStringLiteral("raster_color_write_count"))
      return QStringLiteral("Raster Color-Write Count");
  }
  if (propertyName == QStringLiteral("rasterizerBackend")) {
    const auto backend =
      engine::raster::RasterBackend::fromString(choice.toStdString(), "rasterizerBackend");
    return QString::fromLatin1(backend.displayName());
  }
  if (propertyName == QStringLiteral("rasterizerMSAAShading")) {
    if (choice == QStringLiteral("per_sample"))
      return QStringLiteral("Per Sample");
    if (choice == QStringLiteral("per_fragment"))
      return QStringLiteral("Per Fragment");
  }
  if (propertyName == QStringLiteral("rasterizerShadowFilter"))
    return choice.toUpper();
  return Element::propertyChoiceDisplayName(propertyName, choice);
}

bool RenderIntentElement::rebuildPropertyEditorAfterChange(const QString& propertyName) const {
  return propertyName == QStringLiteral("defaultEngine") ||
         propertyName == QStringLiteral("previewShadows");
}

bool RenderIntentElement::saveIntent() const {
  return scene() && scene()->hasRenderIntent();
}

void RenderIntentElement::setSaveIntent(bool enabled) {
  if (!scene())
    return;

  if (enabled) {
    scene()->setRenderIntent(intent());
  } else {
    scene()->clearRenderIntent();
  }
}

QString RenderIntentElement::defaultEngine() const {
  return toQString(engine::graph::toString(intent().defaultExecutor));
}

void RenderIntentElement::setDefaultEngine(const QString& engine) {
  auto value = intent();
  value.defaultExecutor = executorFromText(engine);
  if (value.defaultExecutor != engine::graph::RenderExecutorPreference::Rasterizer &&
      isRasterCounterView(value.defaultViewMode)) {
    value.defaultViewMode = engine::graph::RenderViewMode::Beauty;
  }
  setIntent(value);
}

QString RenderIntentElement::viewMode() const {
  return toQString(engine::graph::toString(intent().defaultViewMode));
}

void RenderIntentElement::setViewMode(const QString& mode) {
  auto value = intent();
  value.defaultViewMode = viewModeFromText(mode);
  if (isRasterCounterView(value.defaultViewMode)) {
    value.defaultExecutor = engine::graph::RenderExecutorPreference::Rasterizer;
  }
  setIntent(value);
}

QString RenderIntentElement::cameraId() const {
  const auto value = intent();
  if (!value.defaultCamera || !value.defaultCamera->sceneCameraId)
    return {};

  return toQString(*value.defaultCamera->sceneCameraId);
}

void RenderIntentElement::setCameraId(const QString& id) {
  auto value = intent();
  if (id.trimmed().isEmpty()) {
    value.defaultCamera.reset();
  } else {
    value.defaultCamera = engine::graph::RenderCameraRef{id.trimmed().toStdString(), std::nullopt};
  }
  setIntent(value);
}

QString RenderIntentElement::shadingProfile() const {
  return toQString(intent().defaultShadingProfile.name);
}

void RenderIntentElement::setShadingProfile(const QString& profile) {
  auto value = intent();
  value.defaultShadingProfile.name =
    profile.trimmed().isEmpty() ? "default" : profile.trimmed().toStdString();
  setIntent(value);
}

bool RenderIntentElement::automaticFeatures() const {
  return intent().enableAutomaticFeatures;
}

void RenderIntentElement::setAutomaticFeatures(bool enabled) {
  auto value = intent();
  value.enableAutomaticFeatures = enabled;
  setIntent(value);
}

bool RenderIntentElement::wireframeOverlay() const {
  return intent().enableWireframeOverlay;
}

void RenderIntentElement::setWireframeOverlay(bool enabled) {
  auto value = intent();
  value.enableWireframeOverlay = enabled;
  setIntent(value);
}

bool RenderIntentElement::curveOverlay() const {
  return intent().enableCurveOverlay;
}

void RenderIntentElement::setCurveOverlay(bool enabled) {
  auto value = intent();
  value.enableCurveOverlay = enabled;
  setIntent(value);
}

bool RenderIntentElement::previewShadows() const {
  return intent().enablePreviewShadows;
}

void RenderIntentElement::setPreviewShadows(bool enabled) {
  auto value = intent();
  value.enablePreviewShadows = enabled;
  setIntent(value);
}

QString RenderIntentElement::postProcessAA() const {
  return toQString(engine::graph::toString(intent().postProcessAA));
}

void RenderIntentElement::setPostProcessAA(const QString& mode) {
  auto value = intent();
  value.postProcessAA = postProcessAAFromText(mode);
  setIntent(value);
}

QString RenderIntentElement::raytracerSampler() const {
  return toQString(intent().engineOptions.raytracer().sampler().value_or("Regular"));
}

void RenderIntentElement::setRaytracerSampler(const QString& sampler) {
  auto value = intent();
  value.engineOptions.raytracer().setSampler(
    sampler.trimmed().isEmpty() ? std::string("Regular") : sampler.trimmed().toStdString());
  setIntent(value);
}

int RenderIntentElement::raytracerSamplesPerPixel() const {
  return intent().engineOptions.raytracer().samplesPerPixel().value_or(1);
}

void RenderIntentElement::setRaytracerSamplesPerPixel(int samples) {
  auto value = intent();
  value.engineOptions.raytracer().setSamplesPerPixel(samples);
  setIntent(value);
}

int RenderIntentElement::raytracerMaxRecursionDepth() const {
  return intent().engineOptions.raytracer().maximumRecursionDepth().value_or(10);
}

void RenderIntentElement::setRaytracerMaxRecursionDepth(int depth) {
  auto value = intent();
  value.engineOptions.raytracer().setMaximumRecursionDepth(depth);
  setIntent(value);
}

QString RenderIntentElement::raytracerViewPlane() const {
  return toQString(intent().engineOptions.raytracer().viewPlane().value_or("ViewPlane"));
}

void RenderIntentElement::setRaytracerViewPlane(const QString& viewPlane) {
  auto value = intent();
  value.engineOptions.raytracer().setViewPlane(
    viewPlane.trimmed().isEmpty() ? std::string("ViewPlane") : viewPlane.trimmed().toStdString());
  setIntent(value);
}

int RenderIntentElement::raytracerThreads() const {
  return intent().engineOptions.raytracer().maximumThreads().value_or(1);
}

void RenderIntentElement::setRaytracerThreads(int threads) {
  auto value = intent();
  value.engineOptions.raytracer().setMaximumThreads(threads);
  setIntent(value);
}

int RenderIntentElement::raytracerQueueSize() const {
  return intent().engineOptions.raytracer().queueSize().value_or(1);
}

void RenderIntentElement::setRaytracerQueueSize(int queueSize) {
  auto value = intent();
  value.engineOptions.raytracer().setQueueSize(queueSize);
  setIntent(value);
}

int RenderIntentElement::rasterizerLod() const {
  return intent().engineOptions.rasterizer().lod().value_or(0);
}

void RenderIntentElement::setRasterizerLod(int lod) {
  auto value = intent();
  value.engineOptions.rasterizer().setLod(lod);
  setIntent(value);
}

QString RenderIntentElement::rasterizerBackend() const {
  const auto backend =
    intent().engineOptions.rasterizer().backend().value_or(engine::raster::RasterBackend::cpu());
  return QString::fromLatin1(backend.id());
}

void RenderIntentElement::setRasterizerBackend(const QString& backend) {
  auto value = intent();
  value.engineOptions.rasterizer().setBackend(
    engine::raster::RasterBackend::fromString(backend.toStdString(), "rasterizerBackend"));
  setIntent(value);
}

int RenderIntentElement::rasterizerMSAASamples() const {
  return intent().engineOptions.rasterizer().msaaSamples().value_or(1);
}

void RenderIntentElement::setRasterizerMSAASamples(int samples) {
  auto value = intent();
  value.engineOptions.rasterizer().setMSAASamples(samples);
  setIntent(value);
}

QString RenderIntentElement::rasterizerMSAAShading() const {
  return toQString(intent().engineOptions.rasterizer().msaaShadingMode().value_or("per_sample"));
}

void RenderIntentElement::setRasterizerMSAAShading(const QString& mode) {
  auto value = intent();
  value.engineOptions.rasterizer().setMSAAShadingMode(normalizedText(mode).toStdString());
  setIntent(value);
}

int RenderIntentElement::rasterizerShadowMapSize() const {
  return intent().engineOptions.rasterizer().shadowMapSize().value_or(256);
}

void RenderIntentElement::setRasterizerShadowMapSize(int size) {
  auto value = intent();
  value.engineOptions.rasterizer().setShadowMapSize(size);
  setIntent(value);
}

int RenderIntentElement::rasterizerShadowCascades() const {
  return intent().engineOptions.rasterizer().shadowCascadeCount().value_or(1);
}

void RenderIntentElement::setRasterizerShadowCascades(int cascades) {
  auto value = intent();
  value.engineOptions.rasterizer().setShadowCascadeCount(cascades);
  setIntent(value);
}

double RenderIntentElement::rasterizerShadowBias() const {
  return intent().engineOptions.rasterizer().shadowBias().value_or(0.001);
}

void RenderIntentElement::setRasterizerShadowBias(double bias) {
  auto value = intent();
  value.engineOptions.rasterizer().setShadowBias(bias);
  setIntent(value);
}

int RenderIntentElement::rasterizerShadowFilterRadius() const {
  return intent().engineOptions.rasterizer().shadowFilterRadius().value_or(1);
}

void RenderIntentElement::setRasterizerShadowFilterRadius(int radius) {
  auto value = intent();
  value.engineOptions.rasterizer().setShadowFilterRadius(radius);
  setIntent(value);
}

QString RenderIntentElement::rasterizerShadowFilter() const {
  return toQString(intent().engineOptions.rasterizer().shadowFilterMode().value_or("pcf"));
}

void RenderIntentElement::setRasterizerShadowFilter(const QString& filter) {
  auto value = intent();
  value.engineOptions.rasterizer().setShadowFilterMode(normalizedText(filter).toStdString());
  setIntent(value);
}

int RenderIntentElement::wireframeLod() const {
  return intent().engineOptions.wireframe().lod().value_or(0);
}

void RenderIntentElement::setWireframeLod(int lod) {
  auto value = intent();
  value.engineOptions.wireframe().setLod(lod);
  setIntent(value);
}

Scene* RenderIntentElement::scene() const {
  return qobject_cast<Scene*>(parent());
}

engine::graph::RenderIntent RenderIntentElement::intent() const {
  if (!scene() || !scene()->hasRenderIntent())
    return engine::graph::RenderIntent();

  return scene()->renderIntent();
}

void RenderIntentElement::setIntent(engine::graph::RenderIntent intent) {
  if (scene())
    scene()->setRenderIntent(std::move(intent));
}

engine::graph::RenderExecutorPreference
RenderIntentElement::executorFromText(const QString& text) const {
  const QString value = normalizedText(text);
  if (value == QStringLiteral("rasterizer") || value == QStringLiteral("raster"))
    return engine::graph::RenderExecutorPreference::Rasterizer;
  if (value == QStringLiteral("wireframe"))
    return engine::graph::RenderExecutorPreference::Wireframe;
  return engine::graph::RenderExecutorPreference::Raytracer;
}

engine::graph::RenderViewMode RenderIntentElement::viewModeFromText(const QString& text) const {
  const QString value = normalizedText(text);
  if (value == QStringLiteral("default"))
    return engine::graph::RenderViewMode::Default;
  if (value == QStringLiteral("wireframe"))
    return engine::graph::RenderViewMode::Wireframe;
  if (value == QStringLiteral("depth"))
    return engine::graph::RenderViewMode::Depth;
  if (value == QStringLiteral("stencil"))
    return engine::graph::RenderViewMode::Stencil;
  if (value == QStringLiteral("stencil_composite"))
    return engine::graph::RenderViewMode::StencilComposite;
  if (value == QStringLiteral("normal"))
    return engine::graph::RenderViewMode::Normal;
  if (value == QStringLiteral("object_id"))
    return engine::graph::RenderViewMode::ObjectId;
  if (value == QStringLiteral("material_id"))
    return engine::graph::RenderViewMode::MaterialId;
  if (value == QStringLiteral("world_position"))
    return engine::graph::RenderViewMode::WorldPosition;
  if (value == QStringLiteral("raster_coverage_count"))
    return engine::graph::RenderViewMode::RasterCoverageCount;
  if (value == QStringLiteral("raster_depth_test_count"))
    return engine::graph::RenderViewMode::RasterDepthTestCount;
  if (value == QStringLiteral("raster_depth_pass_count"))
    return engine::graph::RenderViewMode::RasterDepthPassCount;
  if (value == QStringLiteral("raster_shade_count"))
    return engine::graph::RenderViewMode::RasterShadeCount;
  if (value == QStringLiteral("raster_color_write_count"))
    return engine::graph::RenderViewMode::RasterColorWriteCount;
  return engine::graph::RenderViewMode::Beauty;
}

engine::graph::RenderPostProcessAA
RenderIntentElement::postProcessAAFromText(const QString& text) const {
  const QString value = normalizedText(text);
  if (value == QStringLiteral("fxaa"))
    return engine::graph::RenderPostProcessAA::FXAA;
  if (value == QStringLiteral("smaa"))
    return engine::graph::RenderPostProcessAA::SMAA;
  if (value == QStringLiteral("taa"))
    return engine::graph::RenderPostProcessAA::TAA;
  return engine::graph::RenderPostProcessAA::None;
}

bool RenderIntentElement::isRasterCounterView(engine::graph::RenderViewMode viewMode) const {
  return viewMode == engine::graph::RenderViewMode::RasterCoverageCount ||
         viewMode == engine::graph::RenderViewMode::RasterDepthTestCount ||
         viewMode == engine::graph::RenderViewMode::RasterDepthPassCount ||
         viewMode == engine::graph::RenderViewMode::RasterShadeCount ||
         viewMode == engine::graph::RenderViewMode::RasterColorWriteCount;
}

QString RenderIntentElement::toQString(const std::string& value) const {
  return QString::fromStdString(value);
}

QString RenderIntentElement::toQString(const char* value) const {
  return QString::fromLatin1(value);
}

QString RenderIntentElement::normalizedText(const QString& text) const {
  QString result = text.trimmed().toLower();
  result.replace(QChar('-'), QChar('_'));
  result.replace(QChar(' '), QChar('_'));
  return result;
}

bool RenderIntentElement::isRaytracerProperty(const QString& propertyName) const {
  return propertyName.startsWith(QStringLiteral("raytracer"));
}

bool RenderIntentElement::isRasterizerProperty(const QString& propertyName) const {
  return propertyName == QStringLiteral("previewShadows") ||
         propertyName.startsWith(QStringLiteral("rasterizer"));
}

bool RenderIntentElement::isRasterizerShadowProperty(const QString& propertyName) const {
  return propertyName.startsWith(QStringLiteral("rasterizerShadow"));
}

bool RenderIntentElement::isWireframeProperty(const QString& propertyName) const {
  return propertyName.startsWith(QStringLiteral("wireframe")) &&
         propertyName != QStringLiteral("wireframeOverlay");
}

QStringList RenderIntentElement::raytracerSamplerChoices() const {
  QStringList choices;
  for (const auto& id : render::SamplerFactory::self().identifiers()) {
    QString choice = QString::fromStdString(id);
    choices << choice.replace(QStringLiteral("Sampler"), QString());
  }
  return choices;
}

QStringList RenderIntentElement::raytracerViewPlaneChoices() const {
  QStringList choices;
  for (const auto& id : render::ViewPlaneFactory::self().identifiers()) {
    choices << QString::fromStdString(id);
  }
  return choices;
}
