require 'optparse'
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

class DocsRenderer
  def initialize(options)
    @options = options
  end
  
  def doc_scene(options = {}, &block)
    default_options = {
      :sampler => "Regular",
      :samples_per_pixel => @options[:samples_per_pixel],
      :overwrite => !@options[:missing]
    }
    
    scene default_options.merge(options) do
      block.bind(self).call
    end
  end

  def class_doc(&block)
    doc_scene render_size(1) do
      block.bind(self).call
    end
  end

  def rainbow_doc(&block)
    rainbow_colors.each do |name, color|
      doc_scene render_size(7) do
        block.bind(self).call(name, color)
      end
    end
  end

  def property_doc(num = 5, &block)
    1.upto(num) do |i|
      doc_scene render_size(num) do
        block.bind(self).call(i)
      end
    end
  end

  def run
    Dir.glob(File.dirname(__FILE__) + "/docs/*.rb").each do |file|
      load file if File.basename(file) =~ @options[:filter]
    end
  end

private
  def load(file)
    eval(File.read(file))
  end
  
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
end

options = { :samples_per_pixel => 16, :filter => // }
OptionParser.new do |opts|
  opts.on("--samples N", Numeric, "Samples per pixel") do |n|
    options[:samples_per_pixel] = n
  end
  opts.on("--only REGEXP", String, "Regexp to filter files under docs to load") do |filter|
    options[:filter] = Regexp.new(filter)
  end
  opts.on("--missing", "Only render missing images") do |missing|
    options[:missing] = true
  end
end.parse!

DocsRenderer.new(options).run
