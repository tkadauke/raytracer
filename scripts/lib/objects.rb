require_relative 'colors'

module Objects
  def checker_board(attrs = {})
    default_attrs = {
      :material => matte_material(
        :diffuseTexture => checker_board_texture(
          :brightTexture => white,
          :darkTexture => black,
        )
      ),
      :size => [12, 0.1, 12],
      :position => [0, 1.1, 0]
    }
    box default_attrs.merge(attrs)
  end
end

ElementCreator.send :include, Objects
