class_doc do
  name "orthographic_camera_cube"
  camera_scene
  orthographic_camera :position => [0, -1, -3]
end

property_doc do |i|
  zoom = 1 + (i - 1) * 0.25
  name "orthographic_camera_cube_zoom_#{zoom}"
  
  camera_scene
  orthographic_camera :position => [0, -1, -3], :zoom => zoom
end
