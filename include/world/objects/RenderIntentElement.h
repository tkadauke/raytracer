#pragma once

#include "engine/graph/RenderGraphTypes.h"
#include "world/objects/Element.h"

#include <string>

class Scene;

/**
  * Property-editor adapter for the scene-owned render intent.
  *
  * This is a generated scene child so the Elements tree can select it, but it
  * is serialized through Scene's top-level renderIntent block rather than as a
  * normal child object.
  */
class RenderIntentElement : public Element {
  Q_OBJECT
  Q_PROPERTY(bool saveIntent READ saveIntent WRITE setSaveIntent)
  Q_PROPERTY(QString defaultEngine READ defaultEngine WRITE setDefaultEngine)
  Q_PROPERTY(QString viewMode READ viewMode WRITE setViewMode)
  Q_PROPERTY(QString cameraId READ cameraId WRITE setCameraId)
  Q_PROPERTY(QString shadingProfile READ shadingProfile WRITE setShadingProfile)
  Q_PROPERTY(bool automaticFeatures READ automaticFeatures WRITE setAutomaticFeatures)
  Q_PROPERTY(bool wireframeOverlay READ wireframeOverlay WRITE setWireframeOverlay)
  Q_PROPERTY(bool curveOverlay READ curveOverlay WRITE setCurveOverlay)
  Q_PROPERTY(bool previewShadows READ previewShadows WRITE setPreviewShadows)
  Q_PROPERTY(QString postProcessAA READ postProcessAA WRITE setPostProcessAA)
  Q_PROPERTY(QString raytracerSampler READ raytracerSampler WRITE setRaytracerSampler)
  Q_PROPERTY(
    int raytracerSamplesPerPixel READ raytracerSamplesPerPixel WRITE setRaytracerSamplesPerPixel)
  Q_PROPERTY(int raytracerMaxRecursionDepth READ raytracerMaxRecursionDepth WRITE
               setRaytracerMaxRecursionDepth)
  Q_PROPERTY(QString raytracerViewPlane READ raytracerViewPlane WRITE setRaytracerViewPlane)
  Q_PROPERTY(int raytracerThreads READ raytracerThreads WRITE setRaytracerThreads)
  Q_PROPERTY(int raytracerQueueSize READ raytracerQueueSize WRITE setRaytracerQueueSize)
  Q_PROPERTY(int rasterizerLod READ rasterizerLod WRITE setRasterizerLod)
  Q_PROPERTY(QString rasterizerBackend READ rasterizerBackend WRITE setRasterizerBackend)
  Q_PROPERTY(int rasterizerMSAASamples READ rasterizerMSAASamples WRITE setRasterizerMSAASamples)
  Q_PROPERTY(
    QString rasterizerMSAAShading READ rasterizerMSAAShading WRITE setRasterizerMSAAShading)
  Q_PROPERTY(
    int rasterizerShadowMapSize READ rasterizerShadowMapSize WRITE setRasterizerShadowMapSize)
  Q_PROPERTY(
    int rasterizerShadowCascades READ rasterizerShadowCascades WRITE setRasterizerShadowCascades)
  Q_PROPERTY(double rasterizerShadowBias READ rasterizerShadowBias WRITE setRasterizerShadowBias)
  Q_PROPERTY(int rasterizerShadowFilterRadius READ rasterizerShadowFilterRadius WRITE
               setRasterizerShadowFilterRadius)
  Q_PROPERTY(
    QString rasterizerShadowFilter READ rasterizerShadowFilter WRITE setRasterizerShadowFilter)
  Q_PROPERTY(int wireframeLod READ wireframeLod WRITE setWireframeLod)

public:
  explicit RenderIntentElement(Scene* parent = nullptr);

  bool displayInSceneModel() const override;
  bool isPropertyVisible(const QString& propertyName) const override;
  QString propertyDisplayName(const QString& propertyName) const override;
  QString propertyDescription(const QString& propertyName) const override;
  QString propertyGroup(const QString& propertyName) const override;
  QStringList propertyChoices(const QString& propertyName) const override;
  QList<int> propertyIntChoices(const QString& propertyName) const override;
  std::optional<QPair<int, int>> propertyIntRange(const QString& propertyName) const override;
  std::optional<QPair<double, double>>
  propertyDoubleRange(const QString& propertyName) const override;
  QString propertyChoiceDisplayName(const QString& propertyName,
                                    const QString& choice) const override;
  bool rebuildPropertyEditorAfterChange(const QString& propertyName) const override;

  bool saveIntent() const;
  void setSaveIntent(bool enabled);

  QString defaultEngine() const;
  void setDefaultEngine(const QString& engine);

  QString viewMode() const;
  void setViewMode(const QString& mode);

  QString cameraId() const;
  void setCameraId(const QString& id);

  QString shadingProfile() const;
  void setShadingProfile(const QString& profile);

  bool automaticFeatures() const;
  void setAutomaticFeatures(bool enabled);

  bool wireframeOverlay() const;
  void setWireframeOverlay(bool enabled);

  bool curveOverlay() const;
  void setCurveOverlay(bool enabled);

  bool previewShadows() const;
  void setPreviewShadows(bool enabled);

  QString postProcessAA() const;
  void setPostProcessAA(const QString& mode);

  QString raytracerSampler() const;
  void setRaytracerSampler(const QString& sampler);

  int raytracerSamplesPerPixel() const;
  void setRaytracerSamplesPerPixel(int samples);

  int raytracerMaxRecursionDepth() const;
  void setRaytracerMaxRecursionDepth(int depth);

  QString raytracerViewPlane() const;
  void setRaytracerViewPlane(const QString& viewPlane);

  int raytracerThreads() const;
  void setRaytracerThreads(int threads);

  int raytracerQueueSize() const;
  void setRaytracerQueueSize(int queueSize);

  int rasterizerLod() const;
  void setRasterizerLod(int lod);

  QString rasterizerBackend() const;
  void setRasterizerBackend(const QString& backend);

  int rasterizerMSAASamples() const;
  void setRasterizerMSAASamples(int samples);

  QString rasterizerMSAAShading() const;
  void setRasterizerMSAAShading(const QString& mode);

  int rasterizerShadowMapSize() const;
  void setRasterizerShadowMapSize(int size);

  int rasterizerShadowCascades() const;
  void setRasterizerShadowCascades(int cascades);

  double rasterizerShadowBias() const;
  void setRasterizerShadowBias(double bias);

  int rasterizerShadowFilterRadius() const;
  void setRasterizerShadowFilterRadius(int radius);

  QString rasterizerShadowFilter() const;
  void setRasterizerShadowFilter(const QString& filter);

  int wireframeLod() const;
  void setWireframeLod(int lod);

private:
  Scene* scene() const;
  engine::graph::RenderIntent intent() const;
  void setIntent(engine::graph::RenderIntent intent);

  engine::graph::RenderExecutorPreference executorFromText(const QString& text) const;
  engine::graph::RenderViewMode viewModeFromText(const QString& text) const;
  engine::graph::RenderPostProcessAA postProcessAAFromText(const QString& text) const;
  bool isRasterCounterView(engine::graph::RenderViewMode viewMode) const;
  bool isRaytracerProperty(const QString& propertyName) const;
  bool isRasterizerProperty(const QString& propertyName) const;
  bool isRasterizerShadowProperty(const QString& propertyName) const;
  bool isWireframeProperty(const QString& propertyName) const;
  QStringList raytracerSamplerChoices() const;
  QStringList raytracerViewPlaneChoices() const;
  QString toQString(const std::string& value) const;
  QString toQString(const char* value) const;
  QString normalizedText(const QString& text) const;
};
