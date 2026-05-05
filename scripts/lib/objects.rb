require_relative 'colors'

# Compound-object helpers — single-name shortcuts for "an X surface
# with materials and dimensions already set up." Currently just the
# canonical checker-board floor, used as the visual reference plane
# in nearly every doc-render scene (see `camera_scene`, `dof_scene`,
# `panorama_scene`, `material_scene` in `render_docs.rb`'s `Common`
# module).
#
# Mixed into `ElementCreator` so doc-render drivers can drop the
# floor in with a single `checker_board` call.
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
