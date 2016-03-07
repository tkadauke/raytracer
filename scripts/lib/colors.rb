module Colors
  def white
    @white ||= constant_color_texture(:color => [1, 1, 1])
  end
  
  def black
    @black ||= constant_color_texture(:color => [0, 0, 0])
  end
  
  def red
    @red ||= constant_color_texture(:color => [1, 0, 0])
  end

  def green
    @green ||= constant_color_texture(:color => [0, 1, 0])
  end
  
  def blue
    @green ||= constant_color_texture(:color => [0, 0, 1])
  end
  
  def medium_grey
    @medium_grey ||= constant_color_texture(:color => [0.6, 0.6, 0.6])
  end
end

ElementCreator.send :include, Colors
