# 6. Cameras

> **Status:** Stub. Narrative not written yet. Source anchors and
> embed plan are committed below; expect prose to fill in.

## Arc

The pinhole as the canonical model, then the family — orthographic,
spherical, fisheye, equirectangular, tilt-shift, thin-lens. For
each: what it physically models, what it costs, and how the
codebase wires it in via the `Camera` interface +
`CameraFactory`. Depth-of-field via thin-lens + sampler interaction
is the centerpiece, with the focus invariant pinned by the
functional test under
[`test/functional/render/cameras/`](../../../test/functional/render/cameras/).

## Source anchors

<!-- source-anchors -->
- `include/render/cameras/Camera.h`
- `include/render/cameras/CameraFactory.h`
- `include/render/cameras/PinholeCamera.h`
- `include/render/cameras/OrthographicCamera.h`
- `include/render/cameras/SphericalCamera.h`
- `include/render/cameras/FishEyeCamera.h`
- `include/render/cameras/EquirectangularCamera.h`
- `include/render/cameras/TiltShiftCamera.h`
- `include/render/cameras/ThinLensCamera.h`
- `test/functional/render/cameras/ThinLensCameraTest.cpp`
<!-- /source-anchors -->

## Planned embeds

<!-- widget: camera_forward_projection -->
<!-- widget: wide_angle_camera_mappings -->
<!-- widget: thin_lens_camera_disc_sampling -->
<!-- widget: thin_lens_camera_convergence -->
<!-- widget: tilt_shift_camera_scheimpflug -->

Plus per-camera doc renders from `docs/images/` (sweeps from each
class's `class_doc(...)` driver under `scripts/docs/`).

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [5. The Whitted pipeline](05-the-whitted-pipeline.md)
- Next: [7. Primitives and intersection](07-primitives-and-intersection.md)
- Sampler interaction: [10. Sampling and anti-aliasing](10-sampling-and-anti-aliasing.md)
