#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Rect.h"

#include "test/helpers/Blob.h"

#include <vector>

namespace testing {
  /**
    * @brief Outer-silhouette extractor — the engine-agnostic shape
    *        primitive in this codebase's classical-CV arsenal.
    *
    * Where `Blob` (connected-components flood fill) returns the *full*
    * region of target-coloured pixels — useful when you care about
    * the interior fill, like a Raytracer-rendered solid disk — a
    * `Silhouette` returns just the outer-edge sample points,
    * extracted as the leftmost + rightmost target pixel per row plus
    * the topmost + bottommost target pixel per column.
    *
    * That's the property that makes shape classification work
    * *uniformly* across rendering engines: a Raytracer-rendered
    * filled circle (every interior pixel is target-coloured) and a
    * Wireframe-rendered circle outline (only the silhouette edge is
    * target-coloured) produce the same `Silhouette`. The interior
    * UV-grid wires of a Wireframe sphere don't pollute the silhouette
    * either — only the outermost extremes per row/column count.
    *
    * Implementation is two linear scans over the buffer (one
    * row-major, one column-major); cost is `O(width × height)` plus
    * a small auxiliary point set. No flood fill, no connectivity
    * analysis. The simpler-and-cheaper sibling of `Blob`.
    *
    * @see Blob — connected-components-based primitive for when the
    *      interior fill matters.
    * @see ShapeClassifier — the consumer of these descriptors.
    */
  class Silhouette {
  public:
    explicit Silhouette(std::vector<Pixel> points);

    int sampleCount() const {
      return static_cast<int>(m_points.size());
    }
    Pixel centroid() const {
      return m_centroid;
    }
    Recti boundingBox() const {
      return m_bbox;
    }
    const std::vector<Pixel>& points() const {
      return m_points;
    }

    /// Bounding-box height divided by width.
    double aspectRatio() const;

    /**
      * @brief Standard deviation of point distance from the centroid,
      *        normalised by mean.
      *
      * Engine-agnostic roundness metric. Approaches 0 for a perfect
      * circle (every silhouette point at the same radius). ≈ 0.12
      * for an axis-aligned square (corners stick out by `√2 - 1` of
      * the edge radius). ≈ 0.25 for an equilateral triangle.
      */
    double radialVariance() const;

  private:
    std::vector<Pixel> m_points;
    Pixel m_centroid{0, 0};
    Recti m_bbox;
  };

  /// Extract the outer silhouette of `color`-coloured pixels in
  /// `buffer`. Returns an empty silhouette (`sampleCount() == 0`) if
  /// no target pixels exist.
  Silhouette extractSilhouette(const Buffer<unsigned int>& buffer, const Colord& color);
}
