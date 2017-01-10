property_doc do |i|
  name "camera_position_#{i}"

  offset = (i - 1) - 2.5
  
  camera_scene
  default_camera :position => [offset, -1, -3] 
end

property_doc do |i|
  name "camera_target_#{i}"

  offset = (i - 1) - 1.5
  
  camera_scene
  default_camera :target => [offset, 0, 0] 
end
