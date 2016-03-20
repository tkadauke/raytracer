rainbow_doc do |name, color|
  name "scene_ambient_#{name}"
  ambient color
  
  object_scene
  sphere :material => reflective_material(:diffuseTexture => nil, :reflectionColor => [1, 1, 1], :reflectionCoefficient => 0.5)
end

rainbow_doc do |name, color|
  name "scene_background_#{name}"
  background color
  
  object_scene
  sphere :material => reflective_material(:diffuseTexture => nil, :reflectionColor => [1, 1, 1], :reflectionCoefficient => 0.5)
end
