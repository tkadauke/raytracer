# Tiny stdlib monkey-patches the doc-render DSL relies on. Loaded by
# `scene.rb` at startup. Kept minimal — anything more elaborate
# would warrant pulling in ActiveSupport, which we don't want for
# such a small consumer.

# `"thin_lens_camera".camelize -> "ThinLensCamera"`. Used by
# `ElementCreator#method_missing` to turn snake_case DSL method names
# into the matching class name. Known rendering acronyms are preserved
# so `uv_color_texture` maps to `UVColorTexture`.
# `constantize` then resolves it via `eval`. Both are needed for the
# `pinhole_camera :position => ...`
# style of DSL.
class String
  CAMELIZE_ACRONYMS = {
    "uv" => "UV",
  }.freeze

  def camelize
    split('_').map { |w| CAMELIZE_ACRONYMS.fetch(w, w.capitalize) }.join
  end

  def constantize
    eval(self)
  end
end

# `Proc#bind(object)` lets the DSL run a user-supplied block in the
# binding of an `ElementCreator`. Without this, code inside
# `scene { sphere :radius => 1 }` couldn't resolve `sphere` to
# `ElementCreator#method_missing`.
class Proc
  def bind(object)
    block, time = self, Time.now
    object.class.instance_eval do
      method_name = "__bind_#{time.to_i}_#{time.usec}"
      define_method(method_name, &block)
      method = instance_method(method_name)
      remove_method(method_name)
      method
    end.bind(object)
  end
end

# Angle units in the DSL. `90.degrees` returns radians (the canonical
# storage unit) — matches the C++ `Angled` API. Use the suffix that
# matches the user's mental model:
#
#     spherical_camera :horizontalFieldOfView => 180.degrees
#     thin_lens_camera :focalDistance => 8                  # plain number
class Numeric
  def degrees
    self * 0.01745329251996
  end

  def turns
    self * 2 * Math.PI
  end

  def radians
    self
  end
end

# `[255, 127, 0].to_color → [1.0, 0.498..., 0.0]`. Used in
# `rainbow_doc` to turn 8-bit-per-channel constants into the
# 0..1 floats the renderer expects. Most other code uses
# pre-built textures (`red`, `green`, `blue`) — this is for the
# `rainbow_doc` sweep specifically.
class Array
  def to_color
    raise "to_color only works on arrays with 3 elements" if size != 3
    map { |e| e / 255.0 }
  end
end
