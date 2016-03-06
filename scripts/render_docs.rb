require_relative 'lib/scene'

module Common
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
  
  def default_camera
    pinhole_camera :position => [0.5, -1, -3], :zoom => 2
  end
  
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
  
  def red_matte
    @red_matte ||= matte_material(:diffuseTexture => red)
  end
  
  def sunlight(options = {})
    directional_light({ :direction => [-0.5, -1, -0.5] }.merge(options))
  end

  def box_on_checker_board
    checker_board
    box :material => red_matte
  end

  def sphere_on_checker_board
    checker_board
    sphere :material => red_matte
  end

  def light_scene
    sphere_on_checker_board
    default_camera
  end
  
  def camera_scene
    sunlight
    box_on_checker_board
  end
  
  def object_scene
    sunlight
    checker_board
    default_camera
  end

  def material_scene(mat)
    object_scene
    sphere :material => mat
  end
end

ElementCreator.send :include, Common

def rainbow_colors
  {
    "red"    => [255,   0,   0].to_color,
    "orange" => [255, 127,   0].to_color,
    "yellow" => [255, 255,   0].to_color,
    "green"  => [  0, 255,   0].to_color,
    "blue"   => [  0,   0, 255].to_color,
    "indigo" => [ 75,   0, 130].to_color,
    "violet" => [139,   0, 255].to_color,
  }
end

Dir.glob(File.dirname(__FILE__) + "/docs/*.rb").each do |file|
  load file
end
