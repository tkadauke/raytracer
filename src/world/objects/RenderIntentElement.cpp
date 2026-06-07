#include "world/objects/RenderIntentElement.h"

#include "core/math/Constants.h"
#include "engine/graph/RenderGraphTypes.h"
#include "engine/raster/RasterBackend.h"
#include "render/samplers/SamplerFactory.h"
#include "render/viewplanes/ViewPlaneFactory.h"
#include "world/objects/Scene.h"

#include <cmath>
#include <utility>

namespace {
  constexpr double kWavefrontPreviewActiveFraction = 0.05;
  constexpr double kWavefrontPreviewRmsDelta = 0.02;
  constexpr double kWavefrontFinalActiveFraction = 0.0;
  constexpr double kWavefrontFinalRmsDelta = 0.0;
  constexpr double kWavefrontConvergencePresetEpsilon = 1e-12;
  constexpr int kWavefrontAdaptiveMinimumSamples = 2;
  constexpr double kWavefrontAdaptiveStddevThreshold = 0.05;
}

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
  if (propertyName == QStringLiteral("raytracerIntegrator") &&
      intent().defaultExecutor == engine::graph::RenderExecutorPreference::PathTracer)
    return false;
  if (isRaytracerProperty(propertyName))
    return executor == engine::graph::RenderExecutorKind::Raytracer ||
           executor == engine::graph::RenderExecutorKind::Wavefront;
  if (isWavefrontProperty(propertyName)) {
    if (executor != engine::graph::RenderExecutorKind::Wavefront)
      return false;
    if (propertyName == QStringLiteral("wavefrontConvergence"))
      return true;
    if (propertyName == QStringLiteral("wavefrontAdaptiveSampling"))
      return true;
    if (propertyName == QStringLiteral("wavefrontAdaptiveMinimumSamples") ||
        propertyName == QStringLiteral("wavefrontAdaptiveStddevThreshold"))
      return wavefrontAdaptiveSampling();
    if (propertyName == QStringLiteral("wavefrontDenoiser"))
      return true;
    if (propertyName == QStringLiteral("wavefrontDenoiseRadius"))
      return wavefrontDenoiser() != QStringLiteral("none");
    if (propertyName == QStringLiteral("wavefrontDenoiseColorSigma"))
      return wavefrontDenoiser() == QStringLiteral("bilateral");
    if (propertyName == QStringLiteral("wavefrontConvergenceQuality"))
      return wavefrontConvergence();
    return wavefrontConvergence();
  }
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
  if (propertyName == QStringLiteral("raytracerIntegrator"))
    return QStringLiteral("Integrator");
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
  if (propertyName == QStringLiteral("wavefrontConvergence"))
    return QStringLiteral("Convergence Stop");
  if (propertyName == QStringLiteral("wavefrontConvergenceQuality"))
    return QStringLiteral("Convergence Quality");
  if (propertyName == QStringLiteral("wavefrontConvergenceActiveFraction"))
    return QStringLiteral("Active Fraction");
  if (propertyName == QStringLiteral("wavefrontConvergenceRmsDelta"))
    return QStringLiteral("RMS Delta");
  if (propertyName == QStringLiteral("wavefrontAdaptiveSampling"))
    return QStringLiteral("Adaptive Sampling");
  if (propertyName == QStringLiteral("wavefrontAdaptiveMinimumSamples"))
    return QStringLiteral("Minimum Samples");
  if (propertyName == QStringLiteral("wavefrontAdaptiveStddevThreshold"))
    return QStringLiteral("Stddev Threshold");
  if (propertyName == QStringLiteral("wavefrontDenoiser"))
    return QStringLiteral("Denoiser");
  if (propertyName == QStringLiteral("wavefrontDenoiseRadius"))
    return QStringLiteral("Denoise Radius");
  if (propertyName == QStringLiteral("wavefrontDenoiseColorSigma"))
    return QStringLiteral("Color Sigma");
  if (propertyName == QStringLiteral("rasterizerLod"))
    return QStringLiteral("LOD");
  if (propertyName == QStringLiteral("rasterizerTessellationQuality"))
    return QStringLiteral("Tessellation Quality");
  if (propertyName == QStringLiteral("rasterizerMaxScreenSpaceError"))
    return QStringLiteral("Max Screen-Space Error");
  if (propertyName == QStringLiteral("rasterizerBackend"))
    return QStringLiteral("Backend");
  if (propertyName == QStringLiteral("rasterizerVisibilityCulling"))
    return QStringLiteral("Visibility Culling");
  if (propertyName == QStringLiteral("rasterizerDepthPrepass"))
    return QStringLiteral("Depth Prepass");
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
  if (propertyName == QStringLiteral("rasterizerVisibilityCulling"))
    return QStringLiteral(
      "Requests a graph-visible CPU culling pass. The current baseline records an all-visible "
      "resource; later frustum culling will skip offscreen raster work.");
  if (propertyName == QStringLiteral("rasterizerTessellationQuality"))
    return QStringLiteral(
      "Preview allows larger projected tessellation error for dense scenes. Final keeps the "
      "requested LOD unless an explicit screen-space error override is set.");
  if (propertyName == QStringLiteral("rasterizerMaxScreenSpaceError"))
    return QStringLiteral(
      "Advanced override in pixels for choosing cheaper tessellation variants from projected "
      "primitive size. Zero keeps the requested LOD.");
  if (propertyName == QStringLiteral("rasterizerDepthPrepass"))
    return QStringLiteral(
      "Requests an optional measured opaque depth prepass. Auto currently suppresses cheap or "
      "unsupported passes and records the decision in raster metrics.");
  if (propertyName == QStringLiteral("wavefrontConvergence"))
    return QStringLiteral(
      "Stops wavefront path batches early when active sample count and per-depth radiance-delta "
      "metrics fall below the selected limits.");
  if (propertyName == QStringLiteral("wavefrontConvergenceQuality"))
    return QStringLiteral(
      "Preset convergence limits. Preview stops earlier, Balanced uses the engine defaults, "
      "Final only stops once paths are fully inactive, and Custom exposes the raw thresholds.");
  if (propertyName == QStringLiteral("wavefrontConvergenceActiveFraction"))
    return QStringLiteral(
      "Fraction of primary samples allowed to remain active before convergence can stop.");
  if (propertyName == QStringLiteral("wavefrontConvergenceRmsDelta"))
    return QStringLiteral("Per-depth RMS radiance delta threshold for convergence.");
  if (propertyName == QStringLiteral("wavefrontAdaptiveSampling"))
    return QStringLiteral(
      "Stops stable pixels after an initial sample batch and spends remaining samples only on "
      "pixels whose per-pixel radiance standard deviation remains above the threshold.");
  if (propertyName == QStringLiteral("wavefrontAdaptiveMinimumSamples"))
    return QStringLiteral(
      "Initial samples per pixel used before the adaptive standard-deviation test.");
  if (propertyName == QStringLiteral("wavefrontAdaptiveStddevThreshold"))
    return QStringLiteral(
      "Per-pixel sample radiance standard-deviation threshold. Pixels above this value receive "
      "the remaining configured samples.");
  if (propertyName == QStringLiteral("wavefrontDenoiser"))
    return QStringLiteral(
      "Optional HDR denoising pass for low-sample wavefront renders. Bilateral is a "
      "color-edge-preserving spatial filter; Box is a simple blur mostly useful for debugging.");
  if (propertyName == QStringLiteral("wavefrontDenoiseRadius"))
    return QStringLiteral("Denoiser radius in pixels.");
  if (propertyName == QStringLiteral("wavefrontDenoiseColorSigma"))
    return QStringLiteral("Bilateral color difference threshold; lower values preserve stronger "
                          "color edges.");
  return Element::propertyDescription(propertyName);
}

QString RenderIntentElement::propertyGroup(const QString& propertyName) const {
  if (isWavefrontProperty(propertyName))
    return QStringLiteral("Wavefront");
  if (isRaytracerProperty(propertyName) &&
      intent().defaultExecutor == engine::graph::RenderExecutorPreference::PathTracer)
    return QStringLiteral("Path Tracer");
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
    return {QStringLiteral("raytracer"), QStringLiteral("pathtracer"), QStringLiteral("wavefront"),
            QStringLiteral("rasterizer"), QStringLiteral("wireframe")};
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
    if (intent().defaultExecutorKind() == engine::graph::RenderExecutorKind::Wavefront) {
      choices << QStringLiteral("sample_stddev");
    }
    return choices;
  }
  if (propertyName == QStringLiteral("postProcessAA"))
    return {QStringLiteral("none"), QStringLiteral("fxaa"), QStringLiteral("smaa"),
            QStringLiteral("taa")};
  if (propertyName == QStringLiteral("raytracerSampler"))
    return raytracerSamplerChoices();
  if (propertyName == QStringLiteral("raytracerIntegrator"))
    return {QStringLiteral("whitted"), QStringLiteral("pathtracer")};
  if (propertyName == QStringLiteral("raytracerViewPlane"))
    return raytracerViewPlaneChoices();
  if (propertyName == QStringLiteral("wavefrontConvergenceQuality"))
    return {QStringLiteral("off"), QStringLiteral("preview"), QStringLiteral("balanced"),
            QStringLiteral("final"), QStringLiteral("custom")};
  if (propertyName == QStringLiteral("wavefrontDenoiser"))
    return {QStringLiteral("none"), QStringLiteral("box"), QStringLiteral("bilateral")};
  if (propertyName == QStringLiteral("rasterizerBackend"))
    return {QStringLiteral("cpu"), QStringLiteral("opengl")};
  if (propertyName == QStringLiteral("rasterizerVisibilityCulling"))
    return {QStringLiteral("off"), QStringLiteral("on"), QStringLiteral("auto")};
  if (propertyName == QStringLiteral("rasterizerDepthPrepass"))
    return {QStringLiteral("off"), QStringLiteral("on"), QStringLiteral("auto")};
  if (propertyName == QStringLiteral("rasterizerTessellationQuality"))
    return {QStringLiteral("preview"), QStringLiteral("balanced"), QStringLiteral("final")};
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
  if (propertyName == QStringLiteral("wavefrontDenoiseRadius"))
    return QPair<int, int>(0, 32);
  if (propertyName == QStringLiteral("wavefrontAdaptiveMinimumSamples"))
    return QPair<int, int>(1, 1024);
  return std::nullopt;
}

std::optional<QPair<double, double>>
RenderIntentElement::propertyDoubleRange(const QString& propertyName) const {
  if (propertyName == QStringLiteral("rasterizerShadowBias"))
    return QPair<double, double>(0.0, 100.0);
  if (propertyName == QStringLiteral("rasterizerMaxScreenSpaceError"))
    return QPair<double, double>(0.0, 128.0);
  if (propertyName == QStringLiteral("wavefrontConvergenceActiveFraction"))
    return QPair<double, double>(0.0, 1.0);
  if (propertyName == QStringLiteral("wavefrontConvergenceRmsDelta"))
    return QPair<double, double>(0.0, 10.0);
  if (propertyName == QStringLiteral("wavefrontAdaptiveStddevThreshold"))
    return QPair<double, double>(0.0, 10.0);
  if (propertyName == QStringLiteral("wavefrontDenoiseColorSigma"))
    return QPair<double, double>(0.001, 10.0);
  return std::nullopt;
}

QString RenderIntentElement::propertyChoiceDisplayName(const QString& propertyName,
                                                       const QString& choice) const {
  if (propertyName == QStringLiteral("defaultEngine") && choice == QStringLiteral("wavefront"))
    return QStringLiteral("Wavefront");
  if (propertyName == QStringLiteral("defaultEngine") && choice == QStringLiteral("pathtracer"))
    return QStringLiteral("Path Tracer");
  if (propertyName == QStringLiteral("postProcessAA"))
    return choice == QStringLiteral("none") ? QStringLiteral("None") : choice.toUpper();
  if (propertyName == QStringLiteral("raytracerIntegrator")) {
    if (choice == QStringLiteral("whitted"))
      return QStringLiteral("Whitted");
    if (choice == QStringLiteral("pathtracer"))
      return QStringLiteral("Path Tracer");
  }
  if (propertyName == QStringLiteral("wavefrontConvergenceQuality")) {
    if (choice == QStringLiteral("off"))
      return QStringLiteral("Off");
    if (choice == QStringLiteral("preview"))
      return QStringLiteral("Preview");
    if (choice == QStringLiteral("balanced"))
      return QStringLiteral("Balanced");
    if (choice == QStringLiteral("final"))
      return QStringLiteral("Final");
    if (choice == QStringLiteral("custom"))
      return QStringLiteral("Custom");
  }
  if (propertyName == QStringLiteral("wavefrontDenoiser")) {
    if (choice == QStringLiteral("none"))
      return QStringLiteral("None");
    if (choice == QStringLiteral("box"))
      return QStringLiteral("Box");
    if (choice == QStringLiteral("bilateral"))
      return QStringLiteral("Bilateral");
  }
  if (propertyName == QStringLiteral("viewMode")) {
    if (choice == QStringLiteral("object_id"))
      return QStringLiteral("Object ID");
    if (choice == QStringLiteral("material_id"))
      return QStringLiteral("Material ID");
    if (choice == QStringLiteral("sample_stddev"))
      return QStringLiteral("Sample Stddev");
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
  if (propertyName == QStringLiteral("rasterizerTessellationQuality")) {
    if (choice == QStringLiteral("preview"))
      return QStringLiteral("Preview");
    if (choice == QStringLiteral("balanced"))
      return QStringLiteral("Balanced");
    if (choice == QStringLiteral("final"))
      return QStringLiteral("Final");
  }
  if (propertyName == QStringLiteral("rasterizerVisibilityCulling")) {
    if (choice == QStringLiteral("off"))
      return QStringLiteral("Off");
    if (choice == QStringLiteral("on"))
      return QStringLiteral("On");
    if (choice == QStringLiteral("auto"))
      return QStringLiteral("Auto");
  }
  if (propertyName == QStringLiteral("rasterizerDepthPrepass")) {
    if (choice == QStringLiteral("off"))
      return QStringLiteral("Off");
    if (choice == QStringLiteral("on"))
      return QStringLiteral("On");
    if (choice == QStringLiteral("auto"))
      return QStringLiteral("Auto");
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
         propertyName == QStringLiteral("previewShadows") ||
         propertyName == QStringLiteral("wavefrontConvergence") ||
         propertyName == QStringLiteral("wavefrontConvergenceQuality") ||
         propertyName == QStringLiteral("wavefrontAdaptiveSampling") ||
         propertyName == QStringLiteral("wavefrontDenoiser");
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
  if (value.defaultExecutorKind() != engine::graph::RenderExecutorKind::Wavefront &&
      isWavefrontDiagnosticView(value.defaultViewMode)) {
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

QString RenderIntentElement::raytracerIntegrator() const {
  return toQString(intent().engineOptions.raytracer().integrator().value_or("whitted"));
}

void RenderIntentElement::setRaytracerIntegrator(const QString& integrator) {
  auto value = intent();
  value.engineOptions.raytracer().setIntegrator(integrator.trimmed().isEmpty()
                                                  ? std::string("whitted")
                                                  : normalizedText(integrator).toStdString());
  setIntent(value);
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

bool RenderIntentElement::wavefrontConvergence() const {
  return intent().engineOptions.raytracer().convergenceEnabled().value_or(false);
}

void RenderIntentElement::setWavefrontConvergence(bool enabled) {
  auto value = intent();
  value.engineOptions.raytracer().setConvergenceEnabled(enabled);
  setIntent(value);
}

QString RenderIntentElement::wavefrontConvergenceQuality() const {
  if (!wavefrontConvergence())
    return QStringLiteral("off");

  return wavefrontConvergenceQualityFor(wavefrontConvergenceActiveFraction(),
                                        wavefrontConvergenceRmsDelta());
}

void RenderIntentElement::setWavefrontConvergenceQuality(const QString& quality) {
  auto value = intent();
  applyWavefrontConvergenceQuality(value, normalizedText(quality));
  setIntent(value);
}

double RenderIntentElement::wavefrontConvergenceActiveFraction() const {
  return intent().engineOptions.raytracer().convergenceActiveSampleFractionThreshold().value_or(
    RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD);
}

void RenderIntentElement::setWavefrontConvergenceActiveFraction(double fraction) {
  auto value = intent();
  value.engineOptions.raytracer().setConvergenceActiveSampleFractionThreshold(fraction);
  setIntent(value);
}

double RenderIntentElement::wavefrontConvergenceRmsDelta() const {
  return intent().engineOptions.raytracer().convergenceRadianceDeltaRmsThreshold().value_or(
    RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD);
}

void RenderIntentElement::setWavefrontConvergenceRmsDelta(double threshold) {
  auto value = intent();
  value.engineOptions.raytracer().setConvergenceRadianceDeltaRmsThreshold(threshold);
  setIntent(value);
}

bool RenderIntentElement::wavefrontAdaptiveSampling() const {
  return intent().engineOptions.raytracer().adaptiveSamplingEnabled().value_or(false);
}

void RenderIntentElement::setWavefrontAdaptiveSampling(bool enabled) {
  auto value = intent();
  auto& raytracer = value.engineOptions.raytracer();
  raytracer.setAdaptiveSamplingEnabled(enabled);
  if (enabled) {
    if (!raytracer.adaptiveMinimumSamples())
      raytracer.setAdaptiveMinimumSamples(kWavefrontAdaptiveMinimumSamples);
    if (!raytracer.adaptiveStddevThreshold())
      raytracer.setAdaptiveStddevThreshold(kWavefrontAdaptiveStddevThreshold);
  }
  setIntent(value);
}

int RenderIntentElement::wavefrontAdaptiveMinimumSamples() const {
  return intent().engineOptions.raytracer().adaptiveMinimumSamples().value_or(
    kWavefrontAdaptiveMinimumSamples);
}

void RenderIntentElement::setWavefrontAdaptiveMinimumSamples(int samples) {
  auto value = intent();
  value.engineOptions.raytracer().setAdaptiveMinimumSamples(samples);
  setIntent(value);
}

double RenderIntentElement::wavefrontAdaptiveStddevThreshold() const {
  return intent().engineOptions.raytracer().adaptiveStddevThreshold().value_or(
    kWavefrontAdaptiveStddevThreshold);
}

void RenderIntentElement::setWavefrontAdaptiveStddevThreshold(double threshold) {
  auto value = intent();
  value.engineOptions.raytracer().setAdaptiveStddevThreshold(threshold);
  setIntent(value);
}

QString RenderIntentElement::wavefrontDenoiser() const {
  const auto options = intent().engineOptions.raytracer();
  if (options.denoiser())
    return toQString(*options.denoiser());
  if (options.denoiseColorSigma())
    return QStringLiteral("bilateral");
  if (options.denoiseRadius())
    return QStringLiteral("box");
  return QStringLiteral("none");
}

void RenderIntentElement::setWavefrontDenoiser(const QString& denoiser) {
  auto value = intent();
  value.engineOptions.raytracer().setDenoiser(normalizedText(denoiser).toStdString());
  setIntent(value);
}

int RenderIntentElement::wavefrontDenoiseRadius() const {
  return intent().engineOptions.raytracer().denoiseRadius().value_or(
    wavefrontDenoiser() == QStringLiteral("bilateral") ? 2 : 1);
}

void RenderIntentElement::setWavefrontDenoiseRadius(int radius) {
  auto value = intent();
  value.engineOptions.raytracer().setDenoiseRadius(radius);
  setIntent(value);
}

double RenderIntentElement::wavefrontDenoiseColorSigma() const {
  return intent().engineOptions.raytracer().denoiseColorSigma().value_or(0.1);
}

void RenderIntentElement::setWavefrontDenoiseColorSigma(double sigma) {
  auto value = intent();
  value.engineOptions.raytracer().setDenoiseColorSigma(sigma);
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

QString RenderIntentElement::rasterizerTessellationQuality() const {
  return toQString(intent().engineOptions.rasterizer().tessellationQuality().value_or("balanced"));
}

void RenderIntentElement::setRasterizerTessellationQuality(const QString& quality) {
  auto value = intent();
  value.engineOptions.rasterizer().setTessellationQuality(normalizedText(quality).toStdString());
  setIntent(value);
}

double RenderIntentElement::rasterizerMaxScreenSpaceError() const {
  return intent().engineOptions.rasterizer().maximumScreenSpaceError().value_or(0.0);
}

void RenderIntentElement::setRasterizerMaxScreenSpaceError(double pixels) {
  auto value = intent();
  value.engineOptions.rasterizer().setMaximumScreenSpaceError(pixels);
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

QString RenderIntentElement::rasterizerVisibilityCulling() const {
  const auto mode = intent().engineOptions.rasterizer().visibilityCulling();
  return mode ? visibilityCullingText(*mode) : QStringLiteral("off");
}

void RenderIntentElement::setRasterizerVisibilityCulling(const QString& mode) {
  auto value = intent();
  value.engineOptions.rasterizer().setVisibilityCulling(normalizedText(mode).toStdString());
  setIntent(value);
}

QString RenderIntentElement::rasterizerDepthPrepass() const {
  return toQString(intent().engineOptions.rasterizer().depthPrepass().value_or("off"));
}

void RenderIntentElement::setRasterizerDepthPrepass(const QString& mode) {
  auto value = intent();
  value.engineOptions.rasterizer().setDepthPrepass(normalizedText(mode).toStdString());
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
  if (value == QStringLiteral("pathtracer") || value == QStringLiteral("path_tracer"))
    return engine::graph::RenderExecutorPreference::PathTracer;
  if (value == QStringLiteral("wavefront"))
    return engine::graph::RenderExecutorPreference::Wavefront;
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
  if (value == QStringLiteral("sample_stddev") || value == QStringLiteral("sample_radiance_stddev"))
    return engine::graph::RenderViewMode::SampleStddev;
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

bool RenderIntentElement::isWavefrontDiagnosticView(engine::graph::RenderViewMode viewMode) const {
  return viewMode == engine::graph::RenderViewMode::SampleStddev;
}

QString RenderIntentElement::wavefrontConvergenceQualityFor(double activeFraction,
                                                            double rmsDelta) const {
  if (wavefrontConvergenceThresholdsMatch(activeFraction, rmsDelta, kWavefrontPreviewActiveFraction,
                                          kWavefrontPreviewRmsDelta)) {
    return QStringLiteral("preview");
  }
  if (wavefrontConvergenceThresholdsMatch(activeFraction, rmsDelta,
                                          RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD,
                                          RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD)) {
    return QStringLiteral("balanced");
  }
  if (wavefrontConvergenceThresholdsMatch(activeFraction, rmsDelta, kWavefrontFinalActiveFraction,
                                          kWavefrontFinalRmsDelta)) {
    return QStringLiteral("final");
  }
  return QStringLiteral("custom");
}

bool RenderIntentElement::wavefrontConvergenceThresholdsMatch(double activeFraction,
                                                              double rmsDelta,
                                                              double expectedActiveFraction,
                                                              double expectedRmsDelta) const {
  return std::abs(activeFraction - expectedActiveFraction) <= kWavefrontConvergencePresetEpsilon &&
         std::abs(rmsDelta - expectedRmsDelta) <= kWavefrontConvergencePresetEpsilon;
}

void RenderIntentElement::applyWavefrontConvergenceQuality(engine::graph::RenderIntent& intent,
                                                           const QString& quality) const {
  auto& raytracer = intent.engineOptions.raytracer();
  if (quality == QStringLiteral("off")) {
    raytracer.setConvergenceEnabled(false);
    return;
  }

  raytracer.setConvergenceEnabled(true);
  if (quality == QStringLiteral("preview")) {
    raytracer.setConvergenceActiveSampleFractionThreshold(kWavefrontPreviewActiveFraction);
    raytracer.setConvergenceRadianceDeltaRmsThreshold(kWavefrontPreviewRmsDelta);
    return;
  }
  if (quality == QStringLiteral("final")) {
    raytracer.setConvergenceActiveSampleFractionThreshold(kWavefrontFinalActiveFraction);
    raytracer.setConvergenceRadianceDeltaRmsThreshold(kWavefrontFinalRmsDelta);
    return;
  }
  if (quality == QStringLiteral("custom"))
    return;

  raytracer.setConvergenceActiveSampleFractionThreshold(
    RAYTRACER_WAVEFRONT_ACTIVE_SAMPLE_FRACTION_THRESHOLD);
  raytracer.setConvergenceRadianceDeltaRmsThreshold(
    RAYTRACER_WAVEFRONT_RADIANCE_DELTA_RMS_THRESHOLD);
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

bool RenderIntentElement::isWavefrontProperty(const QString& propertyName) const {
  return propertyName.startsWith(QStringLiteral("wavefront"));
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

QString
RenderIntentElement::visibilityCullingText(engine::graph::RenderVisibilityCulling mode) const {
  switch (mode) {
  case engine::graph::RenderVisibilityCulling::Off:
    return QStringLiteral("off");
  case engine::graph::RenderVisibilityCulling::On:
    return QStringLiteral("on");
  case engine::graph::RenderVisibilityCulling::Auto:
    return QStringLiteral("auto");
  }
  return QStringLiteral("off");
}
