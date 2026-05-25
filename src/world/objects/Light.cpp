#include "world/objects/Light.h"

#include "engine/graph/RenderSceneAnalysis.h"

Light::Light(Element* parent)
    : Transformable(parent),
      m_visible(true),
      m_color(Colord::white()),
      m_intensity(0.5) {
}

void Light::contributeToRenderGraphAnalysis(engine::graph::RenderSceneAnalysis& analysis) const {
  if (visible()) {
    analysis.recordVisibleLight();
  }
}
