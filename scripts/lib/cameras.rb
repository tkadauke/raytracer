module Cameras
  def default_camera(attrs = {}, &block)
    default_attrs = {
      :position => [0.5, -1, -3], :zoom => 2
    }
    pinhole_camera default_attrs.merge(attrs), &block
  end
end

ElementCreator.send :include, Cameras
