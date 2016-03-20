require_relative 'colors'

module Objects
  def checker_board(attrs = {}, &block)
    default_attrs = {
      :material => reflective_material(
        :diffuseTexture => checker_board_texture(
          :brightTexture => white,
          :darkTexture => black,
        ),
        :reflectionCoefficient => 0.2
      ),
      :size => [12, 0.1, 12],
      :position => [0, 1.1, 0]
    }
    box default_attrs.merge(attrs), &block
  end
end

ElementCreator.send :include, Objects
