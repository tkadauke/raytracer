#include "world/objects/Light.h"

#include "engine/graph/RenderSceneAnalysis.h"

#include <string>
#include <vector>

namespace {
  std::vector<std::string> toStdStrings(const std::vector<QString>& values) {
    std::vector<std::string> result;
    result.reserve(values.size());
    for (const auto& value : values) {
      result.push_back(value.toStdString());
    }
    return result;
  }
}

Light::Light(Element* parent)
    : Transformable(parent),
      m_visible(true),
      m_color(Colord::white()),
      m_intensity(0.5) {
}

void Light::contributeToRenderGraphAnalysis(engine::graph::RenderSceneAnalysis& analysis) const {
  if (visible()) {
    analysis.recordVisibleLight();
    analysis.recordSelectableObject(id().toStdString(), name().toStdString(),
                                    toStdStrings(renderGraphTags()),
                                    toStdStrings(renderGraphLayers()),
                                    displayName().toStdString());
  }
}
