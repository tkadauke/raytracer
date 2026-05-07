#pragma once
#include <memory>

#include "world/objects/Camera.h"

/**
  * Equirectangular cameras produce a full 360°×180° panoramic
  * projection of the scene. Useful for VR environment maps, HDR sky
  * captures, reflection probes, and Street-View-style turnarounds.
  *
  * @image html equirectangular_camera.png "Equirectangular render — coloured spheres at every cardinal direction wrap around the full panorama"
  *
  * Editable wrapper for `render::EquirectangularCamera`. Inherits
  * `position` and `target` from `Camera`; has no additional tunable
  * parameters — the projection is always the canonical full-sphere
  * mapping.
  *
  * See `render::EquirectangularCamera` for the underlying math,
  * the pole-stretching / seam artefacts inherent to the projection,
  * and how it differs from `SphericalCamera`.
  *
  * ### Empirical exploration
  *
  * Open `examples/GeneratedRayTracer/scenes/panorama_demo.json` in
  * GeneratedRayTracer (File → Open) for a pre-built scene with
  * coloured spheres in every cardinal direction; or pick **Edit → Add
  * Camera → Equirectangular Camera (360°)** in a fresh scene. **Render
  * the scene at a 2:1 aspect ratio** (e.g. 1024×512) — the output is
  * meaningless at any other aspect because the projection is built
  * around full-sphere coverage with square equator-pixels.
  */
class EquirectangularCamera : public Camera {
  Q_OBJECT;

public:
  /**
    * Constructs an equirectangular camera at the world origin looking
    * along +z. Override via the inherited `setPosition` / `setTarget`.
    */
  explicit EquirectangularCamera(Element* parent = nullptr);

  /**
    * Convert this editable camera into the runtime
    * `render::EquirectangularCamera` used by the renderer. Just
    * forwards `position` / `target`.
    */
  virtual std::shared_ptr<render::Camera> toRaytracer() const;
};
