module Lights
  def sunlight(options = {})
    directional_light({ :direction => [-0.5, -1, -0.5] }.merge(options))
  end
end

ElementCreator.send :include, Lights
