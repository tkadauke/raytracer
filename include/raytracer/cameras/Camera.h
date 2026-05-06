#pragma once
#include <memory>

#include "core/math/Vector.h"
#include "core/math/Matrix.h"
#include "core/math/Ray.h"
#include "core/Color.h"
#include "core/MemoizedValue.h"
#include "raytracer/viewplanes/ViewPlane.h"
#include "raytracer/samplers/SampleStream.h"
#include "raytracer/Object.h"

template<class T>
class Buffer;
template<class T>
class Rect;

namespace raytracer {
  class Raytracer;

  class Camera : public Object {
  public:
    Camera();
    explicit Camera(const Vector3d& position, const Vector3d& target);

    virtual ~Camera();

    inline void setPosition(const Vector3d& position) {
      m_matrix.reset();
      m_position = position;
    }

    inline void setTarget(const Vector3d& target) {
      m_matrix.reset();
      m_target = target;
    }

    inline std::shared_ptr<ViewPlane> viewPlane() const {
      return m_viewPlane;
    }

    virtual void setViewPlane(std::shared_ptr<ViewPlane> plane);

    inline void setShowProgressIndicators(bool show) {
      m_showProgressIndicators = show;
    }

    inline bool showProgressIndicators() const {
      return m_showProgressIndicators;
    }

    const Matrix4d& matrix() const;

    void render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer) const;
    virtual void render(std::shared_ptr<Raytracer> raytracer, Buffer<unsigned int>& buffer, const Rect<int>& rect) const;

    /**
      * Generate a primary ray for pixel `(x, y)`.
      *
      * The `stream` argument supplies stratified Monte-Carlo samples
      * for any extra stochastic dimensions the camera needs (lens
      * disc for thin-lens; shutter time for motion-blur cameras;
      * analyser angle for polarised cameras; ...). Cameras that don't
      * need extra dimensions ignore the parameter.
      *
      * The renderer threads a fresh stream per `(pixel, sample)` pair
      * — see `Camera::render`. Two consecutive `stream.next2D()` calls
      * within a single `rayForPixel` invocation return *independent*
      * stratified dimensions, so a future Kolb lens model that needs
      * both a lens-element sample and a wavelength sample can pull
      * both without correlation.
      */
    virtual Rayd rayForPixel(double x, double y, SampleStream& stream) const = 0;

    /**
      * Convenience overload that uses a `NullSampleStream` returning
      * the centre of every dimension. Useful for tests and ad-hoc
      * callers (e.g. `SceneBrowser`'s pixel-pick) that don't have a
      * sampler at hand. Don't use in production rendering — the
      * camera loses access to stratification.
      */
    inline Rayd rayForPixel(double x, double y) const {
      NullSampleStream stream;
      return rayForPixel(x, y, stream);
    }

    inline Rayd rayForPixel(const Vector2d& pixel) const {
      return rayForPixel(pixel.x(), pixel.y());
    }

    inline void cancel() {
      m_cancelled = true;
    }

    inline bool isCancelled() const {
      return m_cancelled;
    }

    inline void uncancel() {
      m_cancelled = false;
    }

  protected:
    void plot(Buffer<unsigned int>& buffer, const Recti& rect, const ViewPlane::Iterator& pixel, const Colord& color) const;
    void plotRGB(Buffer<unsigned int>& buffer, const Recti& rect, const ViewPlane::Iterator& pixel, unsigned int rgbColor) const;

  private:
    bool m_cancelled;
    bool m_showProgressIndicators;
    Vector3d m_position, m_target;
    mutable MemoizedValue<Matrix4d> m_matrix;
    std::shared_ptr<ViewPlane> m_viewPlane;
  };
}
