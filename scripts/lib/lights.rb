# Light helpers — convenience constructors for the "I want a generic
# scene-illuminating light" case in doc-render drivers. Currently
# just `sunlight` (a directional light from the upper-left, the
# canonical lighting setup most doc-render scenes use). Mixed into
# `ElementCreator` so a doc-render driver can write `sunlight`
# directly in a `scene { ... }` block.
module Lights
  def sunlight(attrs = {}, &block)
    default_attrs = {
      :direction => [-0.5, -1, -0.5]
    }
    directional_light default_attrs.merge(attrs), &block
  end
end

ElementCreator.send :include, Lights
