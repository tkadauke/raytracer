# Camera helpers — convenience constructors for the "I just want a
# scene-viewing camera" case in doc-render drivers. Currently just
# `default_camera` (a pinhole at z=-3, slightly above and right of
# the origin, with zoom=2.5 picked so a unit-radius sphere fills
# ~40% of the thumbnail width). Used by the `light_scene` and
# `material_scene` helpers in `render_docs.rb`'s `Common`.
#
# For doc-render drivers that are *about* a specific camera
# (`scripts/docs/pinhole_camera.rb`, `thin_lens_camera.rb`, etc.),
# don't use `default_camera` — instantiate the specific camera
# class directly so the parameter sweep exercises that camera type.
module Cameras
  def default_camera(attrs = {}, &block)
    default_attrs = {
      :position => [0.5, -1, -3], :zoom => 2.5
    }
    pinhole_camera default_attrs.merge(attrs), &block
  end
end

ElementCreator.send :include, Cameras
