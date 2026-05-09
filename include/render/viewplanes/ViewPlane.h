#pragma once

#include "core/math/Matrix.h"
#include "core/math/Vector.h"
#include "core/math/Rect.h"
#include "core/InequalityOperator.h"

#include <vector>
#include <algorithm>
#include <memory>

namespace render {
  class Sampler;

  /**
    * @brief The 2D pixel grid the camera projects rays through —
    *        plus the iteration order the renderer walks it in.
    *
    * A `ViewPlane` is two things bundled together:
    *
    *  1. A **plane in 3D space** at unit distance in front of the
    *     camera, oriented by the camera's `matrix()`. `pixelAt(x, y)`
    *     converts pixel coordinates `(0..width, 0..height)` into
    *     the 3D world-space point on that plane that the camera
    *     should fire a ray through. The renderer's primary-ray loop
    *     in `Camera::render` calls this on every iteration.
    *  2. An **iteration strategy** — `begin(rect)` returns an
    *     `Iterator` that walks the pixels in some order. The base
    *     `ViewPlane` walks row-by-row left-to-right; the various
    *     subclasses (`PointInterlacedViewPlane`,
    *     `RowInterlacedViewPlane`, `PointShuffledViewPlane`,
    *     `RowShuffledViewPlane`, `TiledViewPlane`) walk the same
    *     rect in different orders so a partial render shows useful
    *     coverage early instead of "filling top to bottom over 30
    *     seconds." That matters in interactive previews — by the
    *     time `Display::paintEvent` fires, the top-left of the
    *     image is rendered but the bottom is unstarted, vs.
    *     interlaced where you see a low-resolution version of the
    *     whole frame within a second or two.
    *
    * @htmlonly
    * <script type="text/javascript" src="figure.js"></script>
    * <script type="text/javascript" src="viewplane_iteration_order.js"></script>
    * @endhtmlonly
    *
    * The pixel `Iterator` is type-erased over the concrete
    * `IteratorBase` subclass each `ViewPlane` returns from
    * `begin()`, so the `Camera::render` loop is a single `for (it
    * = plane->begin(rect); it != plane->end(rect); ++it)` regardless
    * of which subclass is in use.
    *
    * The `pixelSize` field controls the magnification — cameras
    * with a `zoom` setting (Pinhole, Orthographic, ThinLens) push
    * `1.0 / zoom` in here so a higher zoom shrinks the per-pixel
    * delta on the view plane.
    *
    * The attached `Sampler` is what feeds the per-pixel sub-pixel
    * jitter (and, via `Sampler::stream`, any extra stochastic
    * dimensions a camera consumes — see `SampleStream`).
    *
    * @see Camera::render — the consumer of this iteration.
    * @see Sampler — supplies the sub-pixel offsets per iteration.
    */
  class ViewPlane {
  public:
    /**
      * @brief Type-erased base for the concrete iteration strategy
      *        a `ViewPlane` exposes.
      *
      * Subclasses implement `advance()` to step to the next pixel
      * in whatever order they're modelling (regular row-major,
      * interlaced, tiled, ...). The renderer doesn't see this
      * directly — it goes through the wrapping `Iterator` below.
      */
    class IteratorBase : public InequalityOperator<IteratorBase> {
    public:
      /// Begin iterator: starts at the first pixel of `rect`.
      explicit IteratorBase(const ViewPlane* plane, const Recti& rect);
      /// End iterator: positioned past the last pixel of `rect`.
      explicit IteratorBase(const ViewPlane* plane, const Recti& rect, bool end);
      virtual ~IteratorBase() {}

      /// @returns the 3D world-space point on the view plane for
      /// the current pixel, computed via `ViewPlane::pixelAt`.
      Vector3d current() const;

      /// Step to the next pixel in this iterator's traversal order.
      /// Subclasses implement the order they're modelling.
      virtual void advance() = 0;

      inline bool operator==(const IteratorBase& other) const {
        return m_row == other.m_row && m_column == other.m_column;
      }

      /// @returns the current absolute pixel column (rect.left() +
      /// internal offset).
      inline int column() const {
        return m_rect.left() + m_column;
      }

      /// @returns the current absolute pixel row (rect.top() +
      /// internal offset).
      inline int row() const {
        return m_rect.top() + m_row;
      }

      /// @returns the size of the current pixel block in screen
      /// pixels — interlaced strategies start with large blocks and
      /// shrink them on later passes for progressive refinement.
      /// Regular iteration always returns 1.
      inline int pixelSize() const {
        return m_pixelSize;
      }

    protected:
      const ViewPlane* m_plane;
      Recti m_rect;
      int m_column, m_row, m_pixelSize;
    };

    /// Plain row-major iterator. The default for `ViewPlane`
    /// itself; subclasses override `begin()` to swap in their own.
    class RegularIterator : public IteratorBase {
    public:
      explicit RegularIterator(const ViewPlane* plane, const Recti& rect);
      explicit RegularIterator(const ViewPlane* plane, const Recti& rect, bool);

      virtual void advance();
    };

    /**
      * @brief Type-erased iterator wrapper the renderer actually
      *        consumes. Owns the `IteratorBase*` it's constructed
      *        with — `delete`s on destruction.
      *
      * The wrapper allows `Camera::render` to write a single
      * `for (it = plane->begin(rect); it != plane->end(rect); ++it)`
      * loop regardless of which `ViewPlane` subclass produced the
      * begin iterator.
      */
    class Iterator : public InequalityOperator<Iterator> {
    public:
      inline explicit Iterator(IteratorBase* iteratorImpl) {
        m_iteratorImpl = iteratorImpl;
      }

      inline ~Iterator() {
        delete m_iteratorImpl;
      }

      /// @returns the 3D point for the current pixel.
      inline Vector3d operator*() const {
        return m_iteratorImpl->current();
      }

      /// @returns the current pixel coordinates as `(column, row)`.
      /// Used by `Camera::render` to combine with sub-pixel jitter.
      inline Vector2d pixel() const {
        return Vector2d(column(), row());
      }

      inline virtual Iterator& operator++() {
        m_iteratorImpl->advance();
        return *this;
      }

      inline bool operator==(const Iterator& other) const {
        return *m_iteratorImpl == *(other.m_iteratorImpl);
      }

      inline int column() const {
        return m_iteratorImpl->column();
      }

      inline int row() const {
        return m_iteratorImpl->row();
      }

      inline int pixelSize() const {
        return m_iteratorImpl->pixelSize();
      }

    protected:
      IteratorBase* m_iteratorImpl;
    };

    /// Default-constructs an empty plane. `setup()` must be called
    /// before iteration.
    ViewPlane();

    /// Construct with the camera matrix and pixel-rect already
    /// known. Equivalent to default-construct + `setup`.
    explicit ViewPlane(const Matrix4d& matrix, const Recti& window);

    virtual ~ViewPlane();

    /**
      * Re-orient the plane in front of a new camera position /
      * target and resize for a new window. Called by the
      * `Raytracer` before each render so the plane stays in sync
      * with camera and buffer changes. Recomputes the cached
      * `topLeft` / `right` / `down` basis vectors via
      * `setupVectors`.
      */
    inline void setup(const Matrix4d& matrix, const Recti& window) {
      m_matrix = matrix;
      m_window = window;
      setupVectors();
    }

    /// @returns the pixel-rect width.
    inline int width() const {
      return m_window.width();
    }

    /// @returns the pixel-rect height.
    inline int height() const {
      return m_window.height();
    }

    /**
      * Convert homogeneous clip coordinates into framebuffer pixel
      * coordinates without validating the clip vector. Intended for
      * engines that have already proven the point is projectable.
      */
    inline Vector3d screenFromClipUnchecked(const Vector4d& clip) const {
      const double invW = 1.0 / clip.w();
      const double ndcX = clip.x() * invW;
      const double ndcY = clip.y() * invW;
      return Vector3d((ndcX + 1.0) * width() / 2.0, (ndcY + 1.0) * height() / 2.0,
                      clip.z());
    }

    /**
      * Convert homogeneous clip coordinates into framebuffer pixel
      * coordinates, returning `Vector3d::undefined()` when the
      * perspective divide would be invalid.
      */
    inline Vector3d screenFromClip(const Vector4d& clip) const {
      if (clip.isUndefined() || clip.w() <= 0.0)
        return Vector3d::undefined();
      return screenFromClipUnchecked(clip);
    }

    /**
      * @returns an iterator over `rect` in this plane's traversal
      * order. The base implementation returns a `RegularIterator`;
      * interlaced / shuffled / tiled subclasses override.
      *
      * `rect` is in window-relative coordinates and may be a strict
      * subset of the full plane — the renderer subdivides the image
      * into tiles and asks each worker thread for its own iterator
      * over its tile.
      */
    virtual Iterator begin(const Recti& rect) const;

    /// @returns the past-the-end iterator for `rect`.
    inline Iterator end(const Recti& rect) const {
      return Iterator(new RegularIterator(this, rect, true));
    }

    /// World-space corner used by `pixelAt` as the origin of the
    /// pixel grid. Derived from the camera matrix in `setupVectors`.
    inline const Vector3d& topLeft() const {
      return m_topLeft;
    }

    /// World-space "+x" basis vector — one screen pixel rightward.
    inline const Vector3d& right() const {
      return m_right;
    }

    /// World-space "+y" basis vector — one screen pixel downward.
    inline const Vector3d& down() const {
      return m_down;
    }

    /// @returns the per-pixel scale factor, set by zoomed cameras.
    inline double pixelSize() const {
      return m_pixelSize;
    }

    /// Sets the per-pixel scale factor — typically `1.0 / zoom` for
    /// zoomed cameras. Larger values render a wider field of view.
    inline void setPixelSize(double pixelSize) {
      m_pixelSize = pixelSize;
    }

    /**
      * @returns the world-space 3D point on the plane for pixel
      * `(x, y)`. Pixel coordinates are window-relative; non-integer
      * inputs are valid (and used by the sub-pixel jitter path,
      * which adds `[0, 1)` offsets to the integer pixel coords).
      *
      * `pixelSize` scales the view plane around the camera position
      * (`m_matrix.translationVector()`), not around world origin.
      * Scaling around world origin — what an earlier version did —
      * made the camera's effective FOV depend on its absolute world
      * position, so two cameras with identical intrinsics in
      * different scenes saw the world differently. Scaling around
      * the camera makes pixelSize a pure FOV knob: smaller pixelSize
      * = narrower FOV (zoomed in) regardless of where the camera
      * sits.
      */
    inline Vector3d pixelAt(double x, double y) {
      const Vector3d cameraPos = m_matrix.translationVector();
      return cameraPos + (m_topLeft - cameraPos + m_right * x + m_down * y) * m_pixelSize;
    }

    /**
      * Replace the active sampler. Cameras like ThinLens that need
      * a multi-sample sampler call this from their `setViewPlane`
      * override; rendercli wires the user's `--sampler` choice
      * through here.
      */
    inline void setSampler(std::shared_ptr<render::Sampler> sampler) {
      m_sampler = sampler;
    }

    /// @returns the active sampler. The shading loop pulls
    /// per-pixel sub-pixel jitter (and via `Sampler::stream` any
    /// extra stochastic dimensions cameras consume) from this.
    inline std::shared_ptr<render::Sampler> sampler() const {
      return m_sampler;
    }

  protected:
    /// Recompute `m_topLeft` / `m_right` / `m_down` after a
    /// `setup` call. Called automatically; not for direct use.
    void setupVectors();

    friend class Iterator;

    Matrix4d m_matrix;
    Recti m_window;
    Vector3d m_topLeft, m_right, m_down;
    float m_pixelSize;

    std::shared_ptr<render::Sampler> m_sampler;
  };
}
