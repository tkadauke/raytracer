#pragma once
#include "core/math/Rect.h"
#include "render/viewplanes/ViewPlane.h"

#include <cstdint>
#include <memory>

#include <QWidget>

class QImage;
template<class T>
class Buffer;

namespace render {
  class RenderEngine;
}

/**
  * @brief Qt widget that hosts an in-progress render.
  *
  * `RenderWidget` is what the modeler preview and render dialog display in their main
  * pane. It owns a UI-thread front image plus one back buffer per
  * active render job. Worker threads write their job-local back
  * buffer; the selected display mode decides when pixels are copied
  * into the front image, and `paintEvent` only draws that immutable
  * snapshot.
  *
  * The widget does not own the `RenderEngine` — that's a
  * `shared_ptr` passed in from the application. The application
  * stays in control of when to swap scenes / cameras / engines and
  * is responsible for reissuing `render()` on the widget afterwards.
  *
  * Engine-agnostic: any `RenderEngine` subclass (Raytracer,
  * Wireframe, future SoftwareRasterEngine, ...) drops in. Engines
  * that implement `RenderEngine::cloneForRender()` get a snapshot
  * per render job, which lets interactive previews cancel an old job
  * and start the replacement immediately while the old worker drains.
  * Engines that return `nullptr` keep the serialized lifecycle: the
  * widget waits for the current render before starting the next one.
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
  Q_OBJECT
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
  virtual void timerEvent(QTimerEvent* event);

  /**
    * Kick off a render. Starts the worker thread through either an
    * isolated engine snapshot (`cloneForRender`) or the control
    * engine itself when snapshots are unsupported. Snapshot-capable
    * in-flight renders are cancelled and retired without blocking so
    * the replacement frame can start immediately; unsupported engines
    * are still serialized. Emits `finished()` when the active render
    * completes and publishes its final frame.
    */
  virtual void render();

  /**
    * Swap the control render engine. The new engine should share
    * scene + camera state with the previous one (callers typically
    * use `RenderEngine::scene()` / `camera()` to copy over). Call
    * `stop()` first when changing engine kind; old snapshot jobs may
    * be draining in the background, but the control engine pointer is
    * what future renders clone from.
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

  /// @returns the framebuffer size used for the next render job.
  QSize bufferSize() const;

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

  /**
    * Set the aspect-ratio fit mode applied to the engine's camera
    * before each render. Defaults to `FitWidth` — the fix for the
    * "squishes on resize" bug present with the historical `Stretch`
    * behavior.
    *
    * This is applied to `m_engine->camera()` at the start of every
    * `render()` call, so it survives engine and camera swaps.
    */
  void setAspectMode(render::AspectMode mode);

  /// @returns the stored aspect mode.
  render::AspectMode aspectMode() const;

  /**
    * Switch to `FitExact` mode with the given intrinsic aspect ratio
    * (width / height), or return to `FitWidth` when `ratio` is ≤ 0.
    *
    * A positive value sets the mode to `FitExact` and stores `ratio`
    * as the intrinsic aspect for the bar-fill calculation. Passing
    * `0.0` or a negative value reverts to `FitWidth` (no bars,
    * always square pixels).
    */
  void setAspectRatio(double ratio);

  /// @returns true while the render thread is still producing the
  /// current frame.
  bool isRendering() const;

  /// Request cancellation. Snapshot-capable renders are detached and
  /// allowed to drain in the background, so no `finished()` signal is
  /// emitted for that stale frame. Non-snapshot renders remain the
  /// active job until their worker exits.
  void cancelRender();

signals:
  /// Emitted when the active render reports completion and its final
  /// frame was considered for publication. Retired stale jobs do not
  /// emit this signal when they eventually finish.
  void finished();

public slots:
  /// Cancel all known render jobs and wait for their worker threads
  /// to exit. Unlike `cancelRender()`, this is a blocking teardown
  /// path used by destruction, resize, and explicit engine swaps.
  void stop();

protected:
  std::shared_ptr<render::RenderEngine> m_engine;

private:
  void renderThreadDone(std::uint64_t generation);
  void cancelActiveRender();
  void clearInactiveActiveRender();
  void reapRetiredRenderJobs();
  void retireActiveRender();
  Buffer<unsigned int>* activeBackBuffer() const;
  render::RenderEngine* activeRenderEngine() const;
  void copyFrontImageTo(Buffer<unsigned int>& buffer) const;
  void publishProgressUpdate();
  void publishCompletedTiles();
  void publishFullBackBuffer();
  void publishTile(const Recti& tile);
  void markTilesInProgress(QImage& image) const;
  QRgb progressTint(QRgb color) const;

  struct Private;
  std::unique_ptr<Private> p;
};
