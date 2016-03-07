module Cameras
  def default_camera
    pinhole_camera :position => [0.5, -1, -3], :zoom => 2
  end
end

ElementCreator.send :include, Cameras
