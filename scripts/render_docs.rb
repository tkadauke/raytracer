require_relative 'lib/scene'
require_relative 'lib/colors'
require_relative 'lib/lights'
require_relative 'lib/materials'
require_relative 'lib/objects'
require_relative 'lib/cameras'

module Common
  def name(file)
    outfile "docs/images/#{file}.png"
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

def render_size(num)
  case num
  when 1 then { :width => 640, :height => 480 }
  when 5 then { :width => 240, :height => 180 }
  when 7 then { :width => 160, :height => 120 }
  else
    raise "Unknown render size for #{num} images"
  end
end

def class_doc(&block)
  scene render_size(1) do
    block.bind(self).call
  end
end

def rainbow_doc(&block)
  rainbow_colors.each do |name, color|
    scene render_size(7) do
      block.bind(self).call(name, color)
    end
  end
end

def property_doc(num = 5, &block)
  1.upto(num) do |i|
    scene render_size(num) do
      block.bind(self).call(i)
    end
  end
end

Dir.glob(File.dirname(__FILE__) + "/docs/*.rb").each do |file|
  if ENV["ONLY"]
    load file if file =~ /#{ENV["ONLY"]}/
  else
    load file
  end
end
