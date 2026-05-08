module ::Common
  def portal_material_scene
    ambient [0.08, 0.08, 0.08]
    background [0.16, 0.19, 0.25]

    sunlight :direction => [-0.5, -1, -0.35]
    default_camera :position => [0.4, -0.18, -4.8],
                   :target => [0, 0, 0],
                   :zoom => 2.8

    remote_wall = matte_material(
      :diffuseTexture => constant_color_texture(:color => [0.24, 0.68, 0.82]),
      :ambientCoefficient => 4.0,
      :diffuseCoefficient => 0.0
    )
    rectangle :position => [-4.0, 8.0, 11.2],
              :leg1 => [8.0, 0, 0],
              :leg2 => [0, 6.0, 0],
              :material => remote_wall

    # This small scene is far below the camera's direct view. It appears only
    # through the portal because the material redirects rays to that region.
    sphere :position => [0, 8.5, 7.6],
           :radius => 1.05,
           :material => matte_material(:diffuseTexture => red,
                                       :ambientCoefficient => 0.35)
    sphere :position => [-1.35, 9.25, 8.15],
           :radius => 0.55,
           :material => matte_material(:diffuseTexture => blue,
                                       :ambientCoefficient => 0.35)
    sphere :position => [1.05, 9.25, 8.15],
           :radius => 0.55,
           :material => matte_material(:diffuseTexture => yellow,
                                       :ambientCoefficient => 0.35)

    frame_material = matte_material(
      :diffuseTexture => constant_color_texture(:color => [0.02, 0.02, 0.02]),
      :ambientCoefficient => 0.45
    )
    portal = portal_material(:position => [0, 0, -4],
                             :rotation => [0.79, 0, 0],
                             :filterColor => [1, 0.9, 0.82])
    portal_surface_rotation = [0, -0.12, 0]

    rectangle :position => [-1.12, -0.68, 0],
              :leg1 => [2.24, 0, 0],
              :leg2 => [0, 1.36, 0],
              :rotation => portal_surface_rotation,
              :material => portal
    rectangle :position => [-1.32, -0.82, -0.02],
              :leg1 => [2.64, 0, 0],
              :leg2 => [0, 0.14, 0],
              :rotation => portal_surface_rotation,
              :material => frame_material
    rectangle :position => [-1.32, 0.68, -0.02],
              :leg1 => [2.64, 0, 0],
              :leg2 => [0, 0.14, 0],
              :rotation => portal_surface_rotation,
              :material => frame_material
    rectangle :position => [-1.32, -0.68, -0.02],
              :leg1 => [0.14, 0, 0],
              :leg2 => [0, 1.36, 0],
              :rotation => portal_surface_rotation,
              :material => frame_material
    rectangle :position => [1.18, -0.68, -0.02],
              :leg1 => [0.14, 0, 0],
              :leg2 => [0, 1.36, 0],
              :rotation => portal_surface_rotation,
              :material => frame_material
  end
end

class_doc(engines: [:raytracer]) do
  name "portal_material"
  portal_material_scene
end
