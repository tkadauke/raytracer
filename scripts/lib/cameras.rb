module Cameras
  def default_camera(attrs = {})
    default_attrs = {
      :position => [0.5, -1, -3], :zoom => 2
    }
    pinhole_camera default_attrs.merge(attrs)
  end
end

ElementCreator.send :include, Cameras
