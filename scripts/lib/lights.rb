module Lights
  def sunlight(attrs = {}, &block)
    default_attrs = {
      :direction => [-0.5, -1, -0.5]
    }
    directional_light default_attrs.merge(attrs), &block
  end
end

ElementCreator.send :include, Lights
