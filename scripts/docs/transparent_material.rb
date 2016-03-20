module Common
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

property_doc(7) do |i|
  ior = 1.01 + 0.02 * (i - 1)
  name "transparent_material_ior_#{ior}"
  
  material_scene transparent_doc(:refractionIndex => ior)
end
