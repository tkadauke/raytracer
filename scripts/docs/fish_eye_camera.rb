class_doc do
  name "fish_eye_camera_cube"
  
  camera_scene
  fish_eye_camera :position => [0, -1, -3]
end

property_doc do |i|
  fov = 90 + (i - 1) * 60
  
  name "fish_eye_camera_cube_fov_#{fov}"
  
  camera_scene
  fish_eye_camera :position => [0, -1, -3], :fieldOfView => fov.degrees
end
