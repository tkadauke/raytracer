#include "world/objects/RenderIntentElement.h"

#include "engine/graph/RenderGraphTypes.h"
#include "world/objects/Scene.h"

#include <optional>
#include <utility>

RenderIntentElement::RenderIntentElement(Scene* parent)
    : Element(parent) {
  setName(QStringLiteral("Render Intent"));
  setGenerated(true);
}

bool RenderIntentElement::displayInSceneModel() const {
  return true;
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
  setIntent(value);
}

QString RenderIntentElement::viewMode() const {
  return toQString(engine::graph::toString(intent().defaultViewMode));
}

void RenderIntentElement::setViewMode(const QString& mode) {
  auto value = intent();
  value.defaultViewMode = viewModeFromText(mode);
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
