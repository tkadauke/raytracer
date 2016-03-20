property_doc do |i|
  name "transformable_position_#{i}"

  offset = (i - 1) * 0.25
  
  object_scene
  box :position => [offset, -offset, 0], :material => red_matte
end

property_doc do |i|
  name "transformable_rotation_#{i}"

  offset = (i - 1) * 0.25
  
  object_scene
  box :position => [0, -0.4, 0], :rotation => [offset, -offset, 0], :material => red_matte
end

property_doc do |i|
  name "transformable_scale_#{i}"

  offset = (i - 1) * 0.125
  
  object_scene
  sphere :scale => [1.0 - offset, 0.5 + offset, 0.5 + offset], :material => red_matte
end
