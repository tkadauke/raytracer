# Tilt-shift / Scheimpflug camera doc-render driver. Same logic as
# `thin_lens_camera.rb`: tilt-shift is fundamentally a stochastic
# sampling camera (DOF + tilted focal plane), so the default `Regular`
# sampler can't represent the blur — opt into `Jittered` at 64 spp.
TILT_SHIFT_RENDER_OPTIONS = { :sampler => "Jittered", :samples_per_pixel => 64 }

# Hero shot — the canonical miniature effect over the same
# three-sphere DOF scene used by ThinLens. Tilting the focal plane
# 20° produces visible non-uniform DOF: the focus band sweeps across
# the scene at an angle instead of staying at a fixed distance.
class_doc(**TILT_SHIFT_RENDER_OPTIONS) do
  name "tilt_shift_camera_miniature"
  dof_scene
  tilt_shift_camera :position => [0, -1, -8], :target => [0, -1, 0],
                    :apertureRadius => 0.4, :focalDistance => 8,
                    :tilt => 20.degrees,
                    :zoom => 2
end

# Tilt-angle sweep. 0 degenerates to ThinLens (uniform DOF — the focus
# band is centred at focalDistance and parallel to the image plane);
# higher angles produce progressively more dramatic tilted focus.
property_doc(**TILT_SHIFT_RENDER_OPTIONS) do |i|
  tilts = ["0", "10", "20", "30", "45"]
  tilt_str = tilts[i - 1]
  tilt = tilt_str.to_f
  name "tilt_shift_camera_tilt_#{tilt_str}"
  dof_scene
  tilt_shift_camera :position => [0, -1, -8], :target => [0, -1, 0],
                    :apertureRadius => 0.4, :focalDistance => 8,
                    :tilt => tilt.degrees,
                    :zoom => 2
end
