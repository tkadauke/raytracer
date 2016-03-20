class_doc do
  name "phong_material_red"
  material_scene phong_material(:diffuseTexture => red)
end

rainbow_doc do |name, color|
  name "phong_material_specular_color_#{name}"
  material_scene phong_material(:diffuseTexture => white, :specularColor => color)
end

property_doc do |i|
  coeff = (i - 1) * 0.25
  name "phong_material_specular_coeff_#{coeff}"

  material_scene phong_material(:diffuseTexture => red, :specularCoefficient => coeff)
end

property_doc do |i|
  exp = i ** 3
  name "phong_material_exponent_#{exp}"

  material_scene phong_material(:diffuseTexture => red, :exponent => exp)
end
