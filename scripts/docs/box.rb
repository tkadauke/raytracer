class_doc do
  name "box"
  
  object_scene
  box :material => red_matte
end

property_doc do |i|
  name "box_size_#{i}"

  offset = (i - 1) * 0.125
  
  object_scene
  box :size => [1.0 - offset, 0.5 + offset, 0.5 + offset], :material => red_matte
end

property_doc do |i|
  name "box_bevel_radius_#{i}"
  
  object_scene
  box :bevelRadius => i / 10.0, :material => red_matte
end
