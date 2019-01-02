module ::Common
  def transparent_doc(attrs = {})
    default_attrs = {
      :diffuseTexture => medium_grey,
      :refractionIndex => 1.01,
      :ambientCoefficient => 0.1,
      :diffuseCoefficient => 0
    }
    transparent_material(default_attrs.merge(attrs))
  end
end

class_doc do
  name "transparent_material"
  material_scene transparent_doc
end

rainbow_doc do |name, color|
  name "transparent_material_refl_color_#{name}"
  material_scene transparent_doc(:reflectionColor => color, :reflectionCoefficient => 1)
end

property_doc(7) do |i|
  ior = 1.01 + 0.02 * (i - 1)
  name "transparent_material_ior_#{ior}"

  material_scene transparent_doc(:refractionIndex => ior)
end

property_doc(5) do |i|
  coeff = (i - 1) * 0.25
  name "transparent_material_transcoeff_#{coeff}"

  material_scene transparent_doc(:transmissionCoefficient => coeff)
end

property_doc(5) do |i|
  coeff = (i - 1) * 0.25
  name "transparent_material_reflcoeff_#{coeff}"

  material_scene transparent_doc(:reflectionColor => [1, 0, 0], :reflectionCoefficient => coeff)
end
