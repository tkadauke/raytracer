require_relative 'colors'

module Objects
  def checker_board
    box :material => matte_material(
          :diffuseTexture => checker_board_texture(
            :brightTexture => white,
            :darkTexture => black,
          )
        ),
        :size => [12, 0.1, 12],
        :position => [0, 1.1, 0]
  end
end

ElementCreator.send :include, Objects
