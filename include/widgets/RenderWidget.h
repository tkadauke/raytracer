#pragma once
#include "core/math/Rect.h"

#include <cstdint>
#include <memory>

#include <QWidget>

class QImage;

namespace render {
  class RenderEngine;
}

/**
  * @brief Qt widget that hosts an in-progress render.
  *
  * `RenderWidget` is what the GUI applications (`SceneBrowser`,
  * `GeneratedRayTracer`'s `RenderWindow`) display in their main
  * pane. It owns a render-thread back buffer plus a UI-thread
  * front image. Worker threads write the back buffer; the selected
  * display mode decides when pixels are copied into the front image,
  * and `paintEvent` only draws that immutable snapshot.
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
  enum class DisplayMode {
    /// Periodically copy the whole back buffer while rendering.
    /// This is the historic live-preview mode and can show partial
    /// tile writes for engines that progressively fill the LDR buffer.
    PeriodicUpdate,

    /// Periodically copy only engine-reported completed tiles.
    /// Avoids reading tiles still being written by workers.
    CompletedTilePublishing,

    /// Do not publish in-flight pixels; copy the completed frame
    /// when the render thread finishes.
    DoubleBuffer
  };

  /**
    * Construct as a child of `parent`, rendering through `engine`.
    * Caller retains ownership of the engine; it may be reconfigured
    * (camera, scene) between renders without recreating this widget.
    * Use `setEngine` to swap engines (e.g. raytracer → wireframe).
    */
  explicit RenderWidget(QWidget* parent, std::shared_ptr<render::RenderEngine> engine);
  ~RenderWidget();

  /// Paints the current front-buffer snapshot, optionally with the
  /// red in-progress overlay over still-rendering tiles.
  virtual void paintEvent(QPaintEvent*);

  /// Triggered on the in-render-progress timer; publishes according
  /// to `displayMode()`, then calls `update()`.
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

  /// Controls how the render-thread back buffer is published into
  /// the UI-thread front image while the render is in flight.
  void setDisplayMode(DisplayMode mode);
  DisplayMode displayMode() const;

  /// Controls whether a fresh render starts from the previous back
  /// buffer contents. Interactive raytracer previews use this with
  /// point-interlaced view planes so the first coarse pass overwrites
  /// the previous frame instead of copying an empty buffer.
  void setClearBackBufferOnRenderStart(bool clear);

  /// Override the in-flight publication timer. Use `0` for the
  /// automatic interval based on buffer width.
  void setProgressUpdateIntervalMs(int intervalMs);

  /// @returns true while the render thread is still producing the
  /// current frame.
  bool isRendering() const;

  /// Request cancellation without waiting for the render thread to
  /// finish. `finished()` is emitted when the worker thread exits.
  void cancelRender();

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

private:
  void renderThreadDone(std::uint64_t generation);
  void publishProgressUpdate();
  void publishCompletedTiles();
  void publishFullBackBuffer();
  void publishTile(const Recti& tile);
  void markTilesInProgress(QImage& image) const;
  QRgb progressTint(QRgb color) const;

  struct Private;
  std::unique_ptr<Private> p;
};
