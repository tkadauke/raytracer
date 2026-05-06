# ThinLens needs a stochastic sampler — the framework's default
# `Regular` produces a deterministic n×n grid which, when routed
# through the lens-disc concentric mapping, gives visible stripey
# artefacts in heavy-bokeh frames. `Jittered` at 64 spp is enough
# to smooth even the widest aperture in the sweep below; the cost
# is a few seconds per frame (doc-render is offline so this is
# fine).
THIN_LENS_RENDER_OPTIONS = { :sampler => "Jittered", :samples_per_pixel => 64 }

class_doc(**THIN_LENS_RENDER_OPTIONS) do
  name "thin_lens_camera_dof"
  dof_scene
  thin_lens_camera :position => [0, -1, -8], :target => [0, -1, 0],
                   :apertureRadius => 0.4, :focalDistance => 8,
                   :zoom => 2
end

# Aperture sweep — at zero radius the camera degenerates to a pinhole
# (everything sharp); larger radii blur the front and back spheres while
# keeping the middle sphere (at the focal distance) sharp.
#
# The sweep [0.0, 0.2, 0.4, 0.6, 0.8] is wider than a typical real-world
# aperture range so the difference reads at Doxygen thumbnail size
# (~150-200 px). For reference, a real photographic lens at f/1.4 has
# an effective aperture of ~36 mm — about 0.36 in our scene units when
# the camera-to-subject distance is 8 — so values up to 0.8 push past
# what's physically plausible but make the educational point clearly.
#
# Hard-coded value strings (rather than computed) avoid IEEE 754
# precision artefacts in the generated filenames; (i-1)*0.2 for i=2
# gives 0.20000000000000001 in double precision, which Ruby's "#{...}"
# prints with all the digits — and the Doxygen header expects exactly
# "..._aperture_0.2.png".
property_doc(**THIN_LENS_RENDER_OPTIONS) do |i|
  radii = ["0.0", "0.2", "0.4", "0.6", "0.8"]
  radius_str = radii[i - 1]
  radius = radius_str.to_f
  name "thin_lens_camera_dof_aperture_#{radius_str}"
  dof_scene
  thin_lens_camera :position => [0, -1, -8], :target => [0, -1, 0],
                   :apertureRadius => radius, :focalDistance => 8,
                   :zoom => 2
end

# Focal-distance sweep — the in-focus plane shifts through the three
# spheres as the focal distance increases. Aperture is held at a
# moderately large 0.5 so the out-of-focus blur is dramatic enough to
# read at thumbnail size.
#
# Camera at z=-8, spheres at z=-2.5 / 0 / +2.5 → distances from camera
# are 5.5 / 8 / 10.5. focalDistance is measured from the camera position
# along forward, so values 5.5 / 8 / 10.5 land precisely on the front /
# middle / back sphere respectively. Interleaved 6.75 and 9.25 land
# cleanly between adjacent spheres, giving a five-frame sweep that walks
# focus from front to back.
#
# Same value-string pattern as the aperture sweep above to avoid
# float→string precision artefacts in the generated filenames.
property_doc(**THIN_LENS_RENDER_OPTIONS) do |i|
  distances = ["5.5", "6.75", "8", "9.25", "10.5"]
  distance_str = distances[i - 1]
  distance = distance_str.to_f
  name "thin_lens_camera_dof_focal_#{distance_str}"
  dof_scene
  thin_lens_camera :position => [0, -1, -8], :target => [0, -1, 0],
                   :apertureRadius => 0.5, :focalDistance => distance,
                   :zoom => 2
end
