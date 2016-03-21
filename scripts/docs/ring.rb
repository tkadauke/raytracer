class_doc do
  name "ring"
  
  object_scene
  ring :material => red_matte
end

property_doc do |i|
  name "ring_outer_radius_#{i}"
  
  object_scene
  ring :outerRadius => 0.75 + (i - 1) * 0.0625, :material => red_matte
end

property_doc do |i|
  name "ring_inner_radius_#{i}"
  
  object_scene
  ring :innerRadius => 0.5 + (i - 1) * 0.0625, :material => red_matte
end

property_doc do |i|
  name "ring_height_#{i}"
  
  object_scene
  ring :height => 1 + (i - 1) * 0.25, :material => red_matte
end

property_doc do |i|
  name "ring_bevel_radius_#{i}"
  
  object_scene
  ring :bevelRadius => i / 20.0, :material => red_matte
end
