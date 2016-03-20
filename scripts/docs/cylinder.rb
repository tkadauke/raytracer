class_doc do
  name "cylinder"
  
  object_scene
  cylinder :material => red_matte
end

property_doc do |i|
  name "cylinder_radius_#{i}"
  
  object_scene
  cylinder :radius => 0.5 + (i - 1) * 0.125, :material => red_matte
end

property_doc do |i|
  name "cylinder_height_#{i}"
  
  object_scene
  cylinder :height => 1 + (i - 1) * 0.25, :material => red_matte
end

property_doc do |i|
  name "cylinder_bevel_radius_#{i}"
  
  object_scene
  cylinder :bevelRadius => i / 10.0, :material => red_matte
end
