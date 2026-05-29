#pragma once

#include <vector>

namespace engine::raster::detail {
  class OpenGLShadowSamplingPlan;

  /**
    * CPU-side payload used to upload one eligible directional shadow map as an
    * OpenGL texture.
    *
    * The texture stores normalized linear shadow depth in every RGB channel and
    * keeps 1.0 as the "no occluder" sentinel. Shader code can then compare a
    * similarly normalized receiver depth without depending on CPU Buffer
    * layout or double precision.
    */
  class OpenGLShadowTextureData {
  public:
    static OpenGLShadowTextureData from(const OpenGLShadowSamplingPlan& plan);

    bool enabled() const;
    int width() const;
    int height() const;
    double depthScale() const;
    const std::vector<float>& rgbaPixels() const;

  private:
    static constexpr float kNoOccluderDepth = 1.0f;
    static constexpr float kMaximumOccluderDepth = 0.999999f;

    explicit OpenGLShadowTextureData(int width = 0, int height = 0, double depthScale = 1.0);

    float normalizedDepth(double depth) const;
    void appendTexel(double depth);

    int m_width;
    int m_height;
    double m_depthScale;
    std::vector<float> m_rgbaPixels;
  };
}
