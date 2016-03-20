module ::Common
  def reflective_doc(attrs = {})
    default_attrs = {
      :diffuseTexture => nil,
      :reflectionColor => [1, 1, 1],
      :reflectionCoefficient => 0.5
    }
    reflective_material(default_attrs.merge(attrs))
  end
end

class_doc do
  name "reflective_material_red"
  material_scene reflective_doc
end

rainbow_doc do |name, color|
  name "reflective_material_reflection_color_#{name}"
  material_scene reflective_doc(:reflectionColor => color)
end

property_doc do |i|
  coeff = (i - 1) * 0.25
  name "reflective_material_reflection_coeff_#{coeff}"

  material_scene reflective_doc(:reflectionCoefficient => coeff)
end
