#pragma once
#include <memory>

#include <QWidget>

namespace render {
  class RenderEngine;
}

/**
  * @brief Qt widget that hosts an in-progress render.
  *
  * `RenderWidget` is what the GUI applications (`SceneBrowser`,
  * `GeneratedRayTracer`'s `RenderWindow`) display in their main
  * pane. It owns a buffer the worker threads write into, paints
  * the buffer on every `paintEvent`, and runs a `QTimer` that
  * triggers repaints while the render is in flight so the user
  * sees pixels appearing live.
  *
  * The widget does not own the `RenderEngine` — that's a
  * `shared_ptr` passed in from the application. The application
  * stays in control of when to swap scenes / cameras / engines and
  * is responsible for reissuing `render()` on the widget afterwards.
  *
  * Engine-agnostic: any `RenderEngine` subclass (Raytracer,
  * Wireframe, future SoftwareRasterEngine, ...) drops in.
  * Subclasses that want raytracer-specific operations (e.g. the
  * mouse-pick `rayState` probe in `Display`) `dynamic_cast` to
  * `Raytracer*` and skip the operation when the active engine
  * isn't one.
  *
  * Subclasses (`QtDisplay` adds mouse-drag camera control) override
  * the paint and mouse events, but the render lifecycle stays here.
  *
  * @see QtDisplay — interactive variant with click-to-pick.
  */
class RenderWidget : public QWidget {
  Q_OBJECT;
public:
  /**
    * Construct as a child of `parent`, rendering through `engine`.
    * Caller retains ownership of the engine; it may be reconfigured
    * (camera, scene) between renders without recreating this widget.
    * Use `setEngine` to swap engines (e.g. raytracer → wireframe).
    */
  explicit RenderWidget(QWidget* parent, std::shared_ptr<render::RenderEngine> engine);
  ~RenderWidget();

  /// Paints the current state of the buffer, optionally with the
  /// red in-progress overlay over still-rendering tiles.
  virtual void paintEvent(QPaintEvent*);

  /// Triggered on the in-render-progress timer; calls `update()`
  /// so paint sees the latest pixel writes from worker threads.
  virtual void timerEvent(QTimerEvent *event);

  /**
    * Kick off a render. Resets the buffer, starts the worker
    * threads via the engine's `render()`, and starts the repaint
    * timer. Emits `finished()` when the render completes (or is
    * stopped).
    */
  virtual void render();

  /**
    * Swap the active render engine. The new engine should share
    * scene + camera state with the previous one (callers
    * typically use `RenderEngine::scene()` / `camera()` to copy
    * over). Calling this during a render is undefined; use
    * `stop()` first.
    */
  void setEngine(std::shared_ptr<render::RenderEngine> engine);

  /// @returns the active render engine.
  std::shared_ptr<render::RenderEngine> renderEngine() const;

  /**
    * Resize the internal buffer. Call before `render()` to match
    * the desired output resolution; resizing during a render is
    * undefined.
    */
  void setBufferSize(const QSize& size);

  /// Toggle the red overlay drawn over still-rendering tiles.
  /// Useful for interactive previews; turn off for end-of-render
  /// screenshots where the overlay would pollute the result.
  void setShowProgressIndicators(bool show);

signals:
  /// Emitted when the render thread reports completion (success
  /// or via `stop()`). Connect this to update the UI's busy state.
  void finished();

public slots:
  /// Cancel the in-flight render. The widget calls
  /// `RenderEngine::cancel()` internally; the render exits when its
  /// in-progress tiles finish, then `finished()` fires.
  void stop();

protected:
  std::shared_ptr<render::RenderEngine> m_engine;

private slots:
  void renderThreadDone();

private:
  void markRectsInProgress(QImage& image) const;
  QRgb darken(QRgb color, double factor) const;

  struct Private;
  std::unique_ptr<Private> p;
};
