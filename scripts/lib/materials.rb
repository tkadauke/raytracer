module Materials
  def red_matte
    @red_matte ||= matte_material(:diffuseTexture => red)
  end

  def blue_matte
    @blue_matte ||= matte_material(:diffuseTexture => blue)
  end
  
  def glass
    @glass ||= transparent_material(:refractionIndex => 1.57)
  end
end

ElementCreator.send :include, Materials
