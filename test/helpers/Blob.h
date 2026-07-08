#pragma once

#include "core/Buffer.h"
#include "core/Color.h"
#include "core/math/Rect.h"

#include <optional>
#include <vector>

namespace testing {
  /**
    * @brief A pixel coordinate within a buffer.
    *
    * Conventions match `Buffer<T>` indexing: `(0, 0)` is the top-left,
    * `x` runs left-to-right, `y` runs top-to-bottom.
    */
  struct Pixel {
    int x;
    int y;
  };

  /**
    * @brief A connected region of target-coloured pixels in a raster
    *        buffer — the foundation for classical-CV blob analysis.
    *
    * Constructed by `findAllBlobs` / `findLargestBlob` from a
    * BFS flood-fill over a `Buffer<unsigned int>`. Carries the full
    * pixel set, the boundary subset (pixels with at least one
    * non-blob 4-neighbour or buffer-edge neighbour), and the
    * pre-computed centroid + axis-aligned bounding box.
    *
    * Geometric descriptors live as `const` query methods. Each
    * descriptor is a few lines and rotation/translation/scale
    * properties are noted in its docstring — the `Blob` API doubles
    * as a teaching reference for classical blob descriptors.
    *
    * Filled-vs-outline blobs: most descriptors are computed on the
    * boundary subset rather than the full pixel set. `radialVariance`
    * and `aspectRatio` give the same answer whether the blob is
    * solid (Raytracer-rendered shape) or just an outline
    * (Wireframe-rendered shape) — that's the property that lets the
    * `ShapeClassifier` work uniformly across engines.
    *
    * @see ShapeClassifier — composes these descriptors into named
    *      shape predicates (`isCircle`, `isRectangle`).
    * @see findAllBlobs / findLargestBlob — the constructors.
    */
  class Blob {
  public:
    explicit Blob(std::vector<Pixel> pixels, int bufferWidth, int bufferHeight);

    /// Total pixel count in the blob (interior + boundary).
    int area() const {
      return static_cast<int>(m_pixels.size());
    }

    /// Boundary pixel count — perimeter in pixel units.
    int perimeter() const {
      return static_cast<int>(m_boundary.size());
    }

    /// Pixel-mass-weighted centroid (rounded to nearest pixel).
    Pixel centroid() const {
      return m_centroid;
    }

    /// Axis-aligned bounding box, tight to the blob extents.
    Recti boundingBox() const {
      return m_bbox;
    }

    const std::vector<Pixel>& pixels() const {
      return m_pixels;
    }
    const std::vector<Pixel>& boundary() const {
      return m_boundary;
    }

    /**
      * @brief Polsby-Popper compactness measure.
      *
      * `4π · area / perimeter²`. Maximum value 1.0 for a perfect
      * circle (the shape with maximum area for given perimeter);
      * π/4 ≈ 0.785 for a square; π√3/9 ≈ 0.605 for an equilateral
      * triangle; approaches zero for elongated or fragmented shapes.
      *
      * Sensible only for *filled* blobs — for outline-only blobs
      * (Wireframe-rendered silhouettes) area ≈ perimeter and the
      * value collapses to ≈ 4π/perimeter, which doesn't carry
      * shape information. For the engine-agnostic universal metric
      * use `radialVariance` instead.
      */
    double circularity() const;

    /// Bounding-box height divided by width. 1.0 for square-ish
    /// shapes (circles, squares); ≠ 1.0 for elongated ones.
    double aspectRatio() const;

    /**
      * @brief Standard deviation of boundary-point distance from the
      *        centroid, normalised by mean — the universal
      *        roundness metric.
      *
      * Approaches 0 for a perfect circle (every boundary point at
      * the same distance). Approaches ≈ 0.12 for a square (corners
      * stick out by `√2 - 1`). Approaches ≈ 0.25 for an
      * equilateral triangle.
      *
      * Computed on the boundary, so the value is the same for a
      * filled blob (all interior + boundary pixels) and for an
      * outline blob (boundary only). That's what makes the metric
      * engine-agnostic: a Raytracer-rendered red filled circle and
      * a Wireframe-rendered white outline circle both score ≈ 0.
      */
    double radialVariance() const;

    /// Filled-area fraction of the bounding box: `area / bbox_area`.
    /// 1.0 for a blob that fills its bbox (axis-aligned solid
    /// rectangle); π/4 ≈ 0.785 for a filled circle; very small for
    /// outline-only blobs.
    double extent() const;

  private:
    std::vector<Pixel> m_pixels;
    std::vector<Pixel> m_boundary;
    Pixel m_centroid{0, 0};
    Recti m_bbox;
  };

  /// Find every connected blob of `color`-coloured pixels in
  /// `buffer`. Uses 4-connectivity BFS flood-fill.
  std::vector<Blob> findAllBlobs(const Buffer<unsigned int>& buffer, const Colord& color);

  /// Find the largest blob (by area) of `color`-coloured pixels.
  /// Returns `std::nullopt` if no such blob exists.
  std::optional<Blob> findLargestBlob(const Buffer<unsigned int>& buffer, const Colord& color);

  // Shared implementation details used by Blob and Silhouette.

  struct CentroidAndBbox {
    Pixel centroid;
    Recti bbox;
  };

  /// Single-pass centroid and tight bounding box over a non-empty pixel set.
  /// Callers must guarantee `points` is non-empty.
  CentroidAndBbox computeCentroidAndBbox(const std::vector<Pixel>& points);

  /// Coefficient of variation of boundary-point distances from `centroid`.
  /// Returns 0 if `points` is empty or all points coincide with the centroid.
  double computeRadialVariance(const std::vector<Pixel>& points, Pixel centroid);
}
