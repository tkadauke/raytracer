require 'json'
require 'fileutils'
require_relative 'core_ext'

class ElementCreator
  def initialize(element)
    @element = element
  end
  
  def method_missing(method, *args, &block)
    method.to_s.camelize.constantize.new(*args, &block).tap do |obj|
      @element.children << obj
    end
  end
end

class Element
  @@num_objects = 0
  
  def initialize(attributes = {}, &block)
    @@num_objects += 1
    @id = @@num_objects.to_s
    @name = "#{self.class.name} #{@id}"
    attributes.each do |key, value|
      self.send("#{key}=", value)
    end
    
    block.bind(ElementCreator.new(self)).call if block_given?
  end
  
  class << self
    attr_writer :properties
    
    def properties
      @properties ||= []
    end
  
    def property(props)
      self.properties += props.keys
      
      props.each do |name, default|
        define_method name do
          instance_variable_get("@#{name}") || default
        end
        
        attr_writer name
      end
    end

    def all_properties
      if self == Element
        properties
      else
        superclass.all_properties + properties
      end
    end
  end
  
  def attributes
    self.class.all_properties.inject({}) do |hash, prop|
      attr = send(prop)
      if attr.is_a?(Element)
        hash[prop] = attr.id
      else
        hash[prop] = attr
      end
      hash
    end
  end
  
  attr_writer :children
  
  def children
    @children ||= []
  end
  
  property :id => nil,
           :name => nil
  
  def to_json(*args)
    attributes.merge(
      :type => self.class.name,
      :children => children,
    ).to_json(*args)
  end
end

class Scene < Element
  def save_to_file(name)
    File.open(name, 'w') do |file|
      file.puts to_json
    end
  end
  
  def render(outfile, options = {})
    puts "Rendering #{outfile} ..."
    time = Time.now
    file_name = "/tmp/render_#{time.to_i}_#{time.usec}"
    
    save_to_file(file_name)
    
    FileUtils.mkdir_p(File.dirname(outfile))
    
    ENV['DYLD_FRAMEWORK_PATH'] = "/Users/tkadauke/Qt/5.5/clang_64/lib"
    
    args = options.map { |key, value| "--#{key}=#{value}" }.join(" ")
    system "tools/rendercli/rendercli #{file_name} #{outfile} #{args}"
    
    FileUtils.rm(file_name)
  end
end

class Surface < Element
  property :visible => true,
           :position => [0, 0, 0],
           :rotation => [0, 0, 0],
           :scale => [1, 1, 1],
           :material => nil
end

class Material < Element
end

class MatteMaterial < Material
  property :diffuseTexture => nil,
           :ambientCoefficient => 1,
           :diffuseCoefficient => 1
end

class PhongMaterial < MatteMaterial
  property :specularColor => [1, 1, 1],
           :exponent => 16,
           :specularCoefficient => 1
end

class ReflectiveMaterial < PhongMaterial
  property :reflectionColor => [0, 0, 0],
           :reflectionCoefficient => 1
end

class TransparentMaterial < PhongMaterial
  property :refractionIndex => 1
end

class Box < Surface
  property :size => [1, 1, 1]
end

class Sphere < Surface
  property :radius => 1
end

class Camera < Element
  property :position => [0, 0, -5],
           :target => [0, 0, 0]
end

class FishEyeCamera < Camera
  property :fieldOfView => 120.degrees
end

class OrthographicCamera < Camera
  property :zoom => 1
end

class PinholeCamera < Camera
  property :distance => 5,
           :zoom => 1
end

class SphericalCamera < Camera
  property :horizontalFieldOfView => 180.degrees,
           :verticalFieldOfView => 120.degrees
end

class Texture < Element
end

class CheckerBoardTexture < Texture
  property :brightTexture => nil,
           :darkTexture => nil
end

class ConstantColorTexture < Texture
  property :color => [0, 0, 0]
end
