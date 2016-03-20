class_doc do
  name "spherical_camera_cube"
  camera_scene
  spherical_camera :position => [0, -1, -3]
end

property_doc do |i|
  fov = 90 + (i - 1) * 60
  name "spherical_camera_cube_horz_fov_#{fov}"
  
  camera_scene
  spherical_camera :position => [0, -1, -3], :horizontalFieldOfView => fov.degrees
end

property_doc do |i|
  fov = 30 + (i - 1) * 30
  name "spherical_camera_cube_vert_fov_#{fov}"
  
  camera_scene
  spherical_camera :position => [0, -1, -3], :verticalFieldOfView => fov.degrees
end
