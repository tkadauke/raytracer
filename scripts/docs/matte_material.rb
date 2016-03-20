class_doc do
  name "matte_material_red"
  material_scene red_matte
end

rainbow_doc do |name, color|
  name "matte_material_rainbow_#{name}"
  material_scene matte_material(:diffuseTexture => constant_color_texture(:color => color))
end

property_doc do |i|
  coeff = (i - 1) * 0.25
  name "matte_material_ambient_#{coeff}"
  
  material_scene matte_material(:diffuseTexture => green, :ambientCoefficient => coeff)
end

property_doc do |i|
  coeff = (i - 1) * 0.5
  name "matte_material_diffuse_#{coeff}"
  
  material_scene matte_material(:diffuseTexture => green, :diffuseCoefficient => coeff)
end
