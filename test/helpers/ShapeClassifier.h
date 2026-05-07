#pragma once

#include "core/Buffer.h"
#include "core/Color.h"

namespace testing {
  /**
    * @brief Classical-CV shape classifier built on `Silhouette`
    *        descriptors.
    *
    * Each predicate (`isCircle`, `isRectangle`) extracts the outer
    * silhouette of the target colour from the buffer (via
    * `Silhouette` — leftmost+rightmost per row plus topmost+
    * bottommost per column), computes geometric descriptors on the
    * sample set (radial variance, bounding-box aspect ratio), and
    * compares against documented tolerance bands.
    *
    * Silhouette-based descriptors are the engine-agnostic choice: a
    * solid filled disk and a hollow circle outline produce the same
    * silhouette, so the same predicate fires for both. Callers that
    * need interior-fill information (counting distinct objects,
    * measuring area) use `Blob` directly instead.
    *
    * @see Silhouette — the descriptor source.
    * @see Blob — for interior-aware analysis.
    */
  class ShapeClassifier {
  public:
    /// Construct a classifier that operates on `targetColor` pixels.
    /// Default red.
    inline explicit ShapeClassifier(const Colord& targetColor = Colord(1, 0, 0))
      : m_targetColor(targetColor)
    {
    }

    /**
      * @returns true if the silhouette of `targetColor` pixels is a
      * recognisable circle.
      *
      * Decision: `radialVariance < 0.10` AND bounding-box aspect
      * ratio within `[0.83, 1.20]`. The variance threshold is below
      * the ≈0.12 expected for a square; the aspect-ratio gate
      * catches very elongated shapes that happen to have low radial
      * variance (e.g. a vertical bar centred on its midpoint).
      * Tolerance allows for moderate pixel-discretisation jitter at
      * test buffer sizes (≈200×150).
      */
    bool isCircle(const Buffer<unsigned int>& buffer) const;

    /**
      * @returns true if the silhouette of `targetColor` pixels is a
      * recognisable rectangle (or square).
      *
      * Decision: `radialVariance ∈ [0.10, 0.30]`. Square corners
      * stick out from edge midpoints by `(√2 - 1) ≈ 0.41` of the
      * edge radius — std/mean works out to ≈ 0.12. Elongated
      * rectangles push the variance higher; the upper bound rejects
      * triangles (≈ 0.25 for equilateral) and very irregular shapes.
      */
    bool isRectangle(const Buffer<unsigned int>& buffer) const;

  private:
    Colord m_targetColor;
  };
}
