# Memoised colour-texture helpers for the doc-render DSL. Each helper
# returns a `ConstantColorTexture` with the obvious RGB triple,
# memoised in a per-instance ivar so the same texture is reused
# everywhere it's referenced (keeps the emitted JSON small).
#
# Mixed into `ElementCreator` at the bottom of this file, so code
# inside a `scene { ... }` or a doc-render driver block can write
# `matte_material(:diffuseTexture => red)` directly.
#
# **Don't** wrap these in another `constant_color_texture(:color => red)`
# — that nests a texture inside another texture's `:color` slot, which
# is meaningless and silently produces a black material. `red` etc.
# ARE the textures, not RGB tuples.
#
# **Don't** copy-paste a helper without updating the memo ivar:
# `def blue; @green ||= constant_color_texture(...)` (the historical
# bug fixed in commit 78936da) silently aliases blue to green. The
# `scripts/test/test_colors.rb` "different colours are distinct
# instances" test catches this; keep it green.
module Colors
  def white
    @white ||= constant_color_texture(:color => [1, 1, 1])
  end
  
  def black
    @black ||= constant_color_texture(:color => [0, 0, 0])
  end
  
  def red
    @red ||= constant_color_texture(:color => [1, 0, 0])
  end

  def green
    @green ||= constant_color_texture(:color => [0, 1, 0])
  end
  
  def blue
    @blue ||= constant_color_texture(:color => [0, 0, 1])
  end

  def yellow
    @yellow ||= constant_color_texture(:color => [1, 1, 0])
  end

  def medium_grey
    @medium_grey ||= constant_color_texture(:color => [0.6, 0.6, 0.6])
  end
end

ElementCreator.send :include, Colors
