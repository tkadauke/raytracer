# 6. Cameras

A camera turns a pixel coordinate into a ray. That sentence is
the entire camera contract in this codebase, and every camera
class does the same job by a different formula. The choice of
formula determines what kind of image the renderer produces:
straight perspective, parallel projection, fisheye dome, full
panorama, depth-of-field bokeh, miniature-faking tilt-shift.

This chapter is one of the longer ones because the camera family
has the most members. By the end you should know:

- the abstract `Camera` interface and the `rayForPixel` /
  `projectPoint` pair that every concrete camera implements,
- how the canonical pinhole projection works,
- the orthographic projection as the "no perspective" alternative
  and where it earns its keep,
- the three wide-angle projections (spherical, fisheye,
  equirectangular) as different mappings from image plane to
  sphere,
- depth-of-field through the thin-lens model, including the
  sampler-interaction subtlety that makes the model work at all,
- and the tilt-shift twist on thin-lens that produces the
  miniature-faking effect.

## 6.1 The `Camera` interface

The abstract base class is
[`render::Camera`](../../../include/render/cameras/Camera.h). It
holds:

- a **position** in world space (where the camera is),
- a **target** in world space (what the camera is pointing at),
- a **view plane** (covered in
  [chapter 13](../03-scene-structure/13-view-planes.md) — the 2D
  pixel grid the camera projects onto),
- and a small set of cached matrices derived from the first two.

Subclasses override two virtual methods that together form the
forward / inverse pair every camera owes the renderer:

```cpp
// include/render/cameras/Camera.h
virtual Rayd     rayForPixel(double x, double y, render::SampleStream& stream) const = 0;
virtual Vector2d projectPoint(const Vector3d& worldPoint) const;
```

`rayForPixel` is the **forward** direction: given a pixel
coordinate, produce a ray to trace through it. This is what the
renderer's primary-ray loop calls once per pixel (or per subpixel
sample, when anti-aliasing). The `SampleStream&` argument is what
makes a thin-lens camera able to draw a fresh lens sample per ray
without the caller having to know about it; pinhole and
orthographic ignore the stream entirely.

`projectPoint` is the **inverse**: given a 3D world point,
compute the pixel it would land in. This shows up in the
functional tests (e.g. the `PointLight` shadow-boundary test in
[chapter 9](09-lights-and-shading.md) uses it to translate
world-space shadow coordinates into pixel assertions), and in the
software rasterizer's vertex transform
([chapter 18](../04-rasterization/18-the-rasterization-pipeline.md))
where it's the basis of perspective vertex projection.

Two related projection methods live alongside `projectPoint`:

- `projectPointWithDepth(worldPoint)` returns the same pixel
  coordinates plus an eye-relative depth. The rasterizer's
  z-buffer reads this.
- `projectPointToClipSpace(worldPoint)` returns the un-divided
  homogeneous form $(x, y, z, w)$. The rasterizer's near-plane
  clipper reads this *before* the perspective divide so it can
  trim triangles that cross the near plane. Covered in
  [chapter 19](../04-rasterization/19-clipping-depth-stencil.md).

The `camera_forward_projection` widget shows the forward
projection at work — drag the world point, watch the projected
pixel and the homogeneous $w$ readout update:

<!-- widget: camera_forward_projection -->

The factory side is
[`CameraFactory`](../../../include/render/cameras/CameraFactory.h).
It registers each concrete camera by name so `rendercli` and
`GeneratedRayTracer` can swap camera types from a config file or
a dropdown, respectively. The [Whitted](../appendix/a-glossary.md#w) renderer doesn't care which
factory produced the camera — it just calls `rayForPixel` on
whatever it gets.

## 6.2 Pinhole: the canonical case

The pinhole is the simplest perspective model and the renderer's
default camera. The geometry: light from the scene passes through
an infinitesimally small pinhole and lands on a flat image plane
behind the pinhole. Every point in the scene maps to exactly one
pixel; the depth-of-field is infinite (everything is in focus
because the pinhole has zero area).

[`PinholeCamera`](../../../include/render/cameras/PinholeCamera.h)
parameterizes this with two scalars:

- **`distance`** (default 5) is the eye-to-view-plane distance.
  Smaller means wider field of view.
- **`zoom`** (default 1) scales the view-plane pixel size, so a
  larger zoom narrows the field of view without changing the
  position of the eye.

`rayForPixel(x, y)` constructs a ray with origin at the pinhole
and direction pointing through pixel $(x, y)$ on the view plane.
The math is one pair of basis-vector substitutions and a
normalization. The interactive widget above shows the
pinhole-projection inverse; the forward direction simply traces
the inverse arrow back the other way.

The doc-render gallery shows what `setDistance` and `setZoom` do
visually. `docs/images/pinhole_camera_cube_distance_*.png` sweep
the eye-to-plane distance from 1 to 5; the cube starts very
wide-angle and narrows toward a normal lens. The
`pinhole_camera_cube_zoom_*.png` sweep does the inverse — the
cube starts at default and zooms in toward 2×.

## 6.3 Orthographic: parallel projection

[`OrthographicCamera`](../../../include/render/cameras/OrthographicCamera.h)
drops the perspective divide. Every primary ray is parallel to the
camera's forward direction; the only thing that changes per pixel
is the ray's origin, which slides across the view plane.

Geometrically, orthographic is the limit of pinhole as the eye
recedes to infinity while the field of view tightens to compensate.
Practically, orthographic is what you want when the *measurable*
properties of the scene matter more than the *photographic* feel:
CAD drawings, isometric game graphics, technical illustrations,
and architectural plan views all use orthographic projection so
that two objects of the same world-space size occupy the same
pixel-space size regardless of distance from the camera.

The class has one parameter, `zoom`, that scales the view-plane
pixel size. There is no `distance` parameter because orthographic
projection doesn't have one — the eye is conceptually at infinity.

The chapter 13 view-plane vocabulary applies unchanged; the
sampler integration is unchanged; tonemap and depth and shading
all work the same. The only pixel-level difference is that
parallel lines stay parallel in the rendered image, where pinhole
projection makes them converge to a vanishing point.

## 6.4 Wide-angle: spherical, fisheye, equirectangular

Three cameras share one widget because they are all variations on
the same idea: *map a 2D image coordinate to a direction on the
unit sphere*. The differences are how the mapping is parameterized
and what part of the sphere the image covers.

<!-- widget: wide_angle_camera_mappings -->

[`SphericalCamera`](../../../include/render/cameras/SphericalCamera.h)
takes two angles, `horizontalFieldOfView` and
`verticalFieldOfView`, that control how much of the sphere the
image plane covers. With `(180°, 120°)` the camera produces a
half-dome panorama. With `(360°, 180°)` it covers the full sphere
and behaves as a panoramic camera. The doc renders sweep both
parameters and show what each value does to the rendered cube.

[`FishEyeCamera`](../../../include/render/cameras/FishEyeCamera.h)
takes a single angle, `fieldOfView`, and maps the image plane's
unit disc onto the sphere with that angular extent. Pixels outside
the unit disc — the corners of the image — are *not rays*; they
return undefined and don't contribute to the output. This is why
fisheye renders have the characteristic vignette of darkness at
the corners.

[`EquirectangularCamera`](../../../include/render/cameras/EquirectangularCamera.h)
hard-codes the 360° × 180° lat-long projection used in Google
Street View, VR environment maps, and [HDR](../appendix/a-glossary.md#h) sky textures. The image
is always 2:1, the longitude varies linearly with $x$, and the
latitude varies linearly with $y$. The poles get pixel-stretched
near the top and bottom of the image; the seam at $x = 0$ and $x =
\text{width}$ wraps to the same point on the sphere. There are no
parameters because the mapping is fixed by convention.

The widget makes the differences hands-on: switch between the
three modes and you can see the same draggable image point map to
three different sphere directions, with each camera's
characteristic distortions visible at the edges.

## 6.5 Thin-lens: depth of field

The pinhole model produces images where everything is in focus
because the pinhole has zero area. Real cameras have finite-area
apertures, and the cost of that finite area is *defocus*: points
not on the focal plane appear blurred, with the blur radius
proportional to how far off the focal plane the point sits.

[`ThinLensCamera`](../../../include/render/cameras/ThinLensCamera.h)
models this. It replaces the pinhole with a circular aperture of
configurable radius, and adds a `focalDistance` parameter that
specifies the distance at which the projection is sharp. The
parameter list:

- **`apertureRadius`** (default 0.5): the lens disc's radius. Set
  to zero and the camera reverts to a perfect pinhole.
- **`focalDistance`** (default 8): the distance from the camera
  position at which the projection is sharp.
- The position, target, distance, and zoom from the pinhole story
  carry over.

The clever part is how the rays get generated. For each pixel,
the camera:

1. Computes the **point on the focal plane** that the
   corresponding pinhole ray would converge to.
2. Samples a **point on the aperture disc** uniformly at random.
3. Constructs the ray from the aperture sample, *through* the
   focal-plane point.

The crucial property is that all rays for a given pixel converge
at the focal-plane point regardless of where on the aperture they
started — so a surface at the focal distance gets perfectly sharp
shading from any aperture sample. A surface *off* the focal plane
gets shaded from a different world-space point for each aperture
sample, and the average of those samples is the defocus blur.

The convergence-at-focal-plane property is what
`thin_lens_camera_convergence` shows:

<!-- widget: thin_lens_camera_convergence -->

And the disc-sampling pattern is what
`thin_lens_camera_disc_sampling` shows:

<!-- widget: thin_lens_camera_disc_sampling -->

There is one critical implementation detail: a thin-lens camera
*needs* a multi-sample sampler to produce visible defocus. With a
single-sample-per-pixel sampler, the camera draws exactly one
aperture sample per pixel, the ray happens to start somewhere on
the disc, and the result is a shifted-but-sharp pinhole render.
Without sample averaging, there is no blur to average out.
`ThinLensCamera::setViewPlane` enforces this by upgrading the
view plane's sampler to a 16-sample-per-pixel `JitteredSampler`
when the caller leaves the factory default in place. The
GUI-supplied sampler is respected if the caller has explicitly
configured one.

The depth-of-field invariant is pinned by
[`ThinLensCameraTest.FocalPlaneContractSharpVsBlurred`](../../../test/functional/render/cameras/ThinLensCameraTest.cpp):
a sphere placed exactly at the focal distance renders with a crisp
silhouette, while the same sphere with the focal plane well past
it blurs into a wide band of intermediate colors. The test
compares the partial-coverage edge-pixel counts of the two
renders and asserts the second is strictly larger than the first.

The doc-render sweeps in the header — apertures from 0.0 to 0.8,
focal distances from 4 to 12 — show what each parameter does to
the rendered image: aperture controls the blur radius for
out-of-focus surfaces, focal distance controls which surface is
sharp.

### A user-facing parameter caveat

`focalDistance` is measured from the camera *position*, not from
the internal eye/pinhole point of the underlying pinhole math.
This matches every photography app the user is likely to know
(every interchangeable-lens camera reports focal distance from
the front of the lens, not from any internal optical reference).
The implementation translates from the user's mental model to the
math model in one short step; the API never exposes the math
model. This is one of the codebase's "user-facing parameter names
follow user mental models" decisions described in `CLAUDE.md`.

## 6.6 Tilt-shift: a tilted focal plane

A normal thin-lens camera has a focal plane *parallel* to the
view plane: in-focus surfaces are at one specific distance from
the camera. A real-world tilt-shift lens has hinges that let you
tilt the focal plane *away* from parallel, so the in-focus region
becomes a wedge cutting through the scene at an angle.

[`TiltShiftCamera`](../../../include/render/cameras/TiltShiftCamera.h)
models this. It inherits from `ThinLensCamera` and adds a
single parameter:

- **`tilt`**: the angle of the focal plane, measured from the
  parallel default.

The geometry is governed by the **[Scheimpflug](../appendix/a-glossary.md#s) condition**: the
view plane, the lens plane, and the focal plane must all meet
along a single line, called the Scheimpflug line. As you tilt
the lens plane, the focal plane rotates around the Scheimpflug
line in lockstep. The widget shows the relationship interactively:

<!-- widget: tilt_shift_camera_scheimpflug -->

The visible-image consequence: only a narrow stripe of the scene
is in focus, and the rest blurs heavily — including parts of the
scene that are at the same physical distance from the camera as
the in-focus stripe but happen to lie outside the tilted focal
plane. The effect mimics the look of an extreme close-up, which
the human visual system reads as "small subject, photographed up
close." That is why a tilt-shift photo of a city looks like a
toy-train diorama: the eye reads "huge depth-of-field falloff" as
"tiny scene."

The doc-render sweep with `tilt` from 0° to 45° shows the
miniature effect intensifying. At `tilt = 0°` the camera is
literally a `ThinLensCamera`; at extreme tilt angles, only a
ribbon of the image stays sharp.

## 6.7 Picking a camera

For most scenes the pinhole is the right answer. It is what
every other rendering tutorial assumes, every standard
photographic intuition transfers, and the math is trivial.

The orthographic camera is for scenes where you want measurable,
parallel-line-preserving projection — CAD, technical drawings,
editor previews of objects that should be readable as
schematics.

The wide-angle cameras (spherical, fisheye, equirectangular) are
for cases where the field of view exceeds what pinhole projection
can express without absurd distortion. [Equirectangular](../appendix/a-glossary.md#e) is the
right choice for VR environment maps and HDR sky probes;
fisheye is for the "looking up at the dome" effect and for
scenes where the corners-cut-off look is desired; spherical is
the parameter-tunable middle ground for everything in between.

The thin-lens camera is for any render that wants a believable
photograph rather than a CGI feel. Depth-of-field is the single
strongest signal a viewer reads as "this is real" — pinhole
renders without it look slightly synthetic to anyone who has
held a camera. The cost is the multi-sample sampler the
thin-lens path requires; expect roughly 16× the per-pixel
shading work compared to a pinhole render.

The tilt-shift camera is a special-effect lens. Use it when you
specifically want the miniature look or, more rarely, when you
need a tilted focal plane to keep an angled subject (a
shot-down-an-aisle kind of view) in focus from front to back.

## 6.8 Exercises

1. Predict what a `PinholeCamera` renders if you set
   `distance = 0`. Verify with a real run. What goes wrong, and
   why?
2. Build an `OrthographicCamera` and a `PinholeCamera` looking at
   the same scene, both with `zoom = 1`. Two parallel lines on
   the floor of the scene render parallel in one and convergent
   in the other. Which is which, and why?
3. Set a `ThinLensCamera`'s `apertureRadius` to 0 and render. Set
   it to 0.5 with a single-sample sampler. What does each render
   look like? What does it look like with a 16-sample sampler at
   the same aperture? Why?
4. Read `ThinLensCamera::setViewPlane` and find the branch that
   distinguishes "factory default sampler" from "caller-supplied
   sampler." What goes wrong if that branch silently overrides
   the caller's sampler choice in the GUI?
5. The equirectangular camera has no parameters. Why? What would
   it mean to add a `verticalFieldOfView` to it, and how would
   that interact with the convention that latitude varies
   linearly with $y$?

## See also

- Volume index: [Volume II — Ray rendering](README.md)
- Previous: [5. The Whitted pipeline](05-the-whitted-pipeline.md)
- Next: [7. Primitives and intersection](07-primitives-and-intersection.md)
- Sampler interaction:
  [10. Sampling and anti-aliasing](10-sampling-and-anti-aliasing.md)
  — the multi-sample requirement for thin-lens
- View plane:
  [13. View planes](../03-scene-structure/13-view-planes.md) — the
  pixel grid the projection lands on
- Forward-projection consumer:
  [18. The rasterization pipeline](../04-rasterization/18-the-rasterization-pipeline.md)
  — `projectPointWithDepth` is the rasterizer's vertex transform
- Functional test:
  [`test/functional/render/cameras/ThinLensCameraTest.cpp`](../../../test/functional/render/cameras/ThinLensCameraTest.cpp)

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
