class_doc do
  name "pinhole_camera_cube"
  camera_scene
  pinhole_camera :position => [0, -1, -3]
end

property_doc do |i|
  name "pinhole_camera_cube_distance_#{i}"
  camera_scene
  pinhole_camera :position => [0, -1, -3], :distance => i
end

property_doc do |i|
  zoom = 1 + (i - 1) * 0.25
  name "pinhole_camera_cube_zoom_#{zoom}"
  
  camera_scene
  pinhole_camera :position => [0, -1, -3], :zoom => zoom
end
