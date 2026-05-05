# Memoised material helpers — pre-cooked `MatteMaterial` /
# `TransparentMaterial` instances with a colour and physical
# parameters dialled in. Same per-instance memoisation pattern as
# `Colors`. Mixed into `ElementCreator` at the bottom of this file.
#
# These cover the "I just want a generic red surface" case in
# doc-render scenes. For per-driver tweaks, build the material
# directly: `matte_material(:diffuseTexture => red,
# :ambientCoefficient => 0.4, :diffuseCoefficient => 0.6)`.
module Materials
  def red_matte
    @red_matte ||= matte_material(:diffuseTexture => red)
  end

  def blue_matte
    @blue_matte ||= matte_material(:diffuseTexture => blue)
  end
  
  def white_matte
    @white_matte ||= matte_material(:diffuseTexture => white)
  end
  
  def glass
    @glass ||= transparent_material(:refractionIndex => 1.57)
  end
end

ElementCreator.send :include, Materials
