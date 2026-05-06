# Motion-blur driver — produces the images referenced by the
# `world::Surface::setVelocity` docstring.
#
# Motion blur needs a stochastic sampler; with `Regular`, every
# primary ray draws the same time sample (0.5) and the result is a
# crisp render of the sphere at its half-time position, no blur. The
# `THIN_LENS_RENDER_OPTIONS` pattern (`Jittered` + bumped spp) was
# introduced for the same reason.
MOTION_BLUR_RENDER_OPTIONS = { :sampler => "Jittered", :samples_per_pixel => 64 }

# Hero shot — a single moving red sphere on the canonical checker
# floor. Velocity (1.5, 0, 0) gives a clearly-streaked sphere that
# stays mostly on-frame at the default camera distance.
class_doc(**MOTION_BLUR_RENDER_OPTIONS) do
  name "motion_blur_hero"
  sunlight
  checker_board
  sphere :material => matte_material(:diffuseTexture => red),
         :position => [-0.7, 0, 0],
         :scale => [0.8, 0.8, 0.8],
         :velocity => [1.5, 0, 0]
  default_camera
end

# Velocity-magnitude sweep. Speed 0 = static (sharp red sphere);
# higher speeds smear over a wider arc. Sweep tops out at 2.0 — past
# that the sphere stretches off-frame at the default camera framing
# and the blur becomes too faded to read at thumbnail size. Hard-coded
# value strings avoid IEEE 754 round-trip artefacts in the generated
# filenames.
property_doc(**MOTION_BLUR_RENDER_OPTIONS) do |i|
  speeds = ["0", "0.5", "1", "1.5", "2"]
  speed_str = speeds[i - 1]
  speed = speed_str.to_f
  name "motion_blur_velocity_#{speed_str}"
  sunlight
  checker_board
  sphere :material => matte_material(:diffuseTexture => red),
         :position => [-0.7, 0, 0],
         :scale => [0.8, 0.8, 0.8],
         :velocity => [speed, 0, 0]
  default_camera
end
