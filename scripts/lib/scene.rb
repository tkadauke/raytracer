require 'json'
require 'fileutils'
require 'securerandom'
require_relative 'core_ext'

class ElementCreator
  def initialize(element)
    @element = element
  end
  
  def method_missing(method, *args, &block)
    if @element.respond_to?(method)
      @element.send(method, *args, &block)
    else
      begin
        method.to_s.camelize.constantize.new(*args, &block).tap do |obj|
          @element.children << obj
        end
      rescue NameError
        super
      end
    end
  end
end

class Element
  @@num_objects = 0
  
  def initialize(attributes = {}, &block)
    self.class.all_properties.each do |name|
      self.instance_variable_set("@#{name}", :__property_uninitialized_sentinel__)
    end
    
    id = SecureRandom.hex
    @id = "{#{id[0..7]}-#{id[8..11]}-#{id[12..15]}-#{id[16..19]}-#{id[20..31]}}"

    @@num_objects += 1
    @name = "#{self.class.name} #{@@num_objects}"
    @dynamic_properties = {}
    attributes.each do |key, value|
      if respond_to?("#{key}=")
        self.send("#{key}=", value)
      else
        @dynamic_properties[key] = value
      end
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
        define_method name do |value = :__property_uninitialized_sentinel__|
          if value == :__property_uninitialized_sentinel__
            value = instance_variable_get("@#{name}")
            value == :__property_uninitialized_sentinel__ ? default : value
          else
            instance_variable_set("@#{name}", value)
          end
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
    
    def accessor(*fields)
      fields.each do |field|
        define_method field do |value = nil|
          if value
            instance_variable_set("@#{field}", value)
          else
            instance_variable_get("@#{field}")
          end
        end
      end
    end
  end
  
  def method_missing(method, *args)
    if @dynamic_properties.has_key?(method)
      @dynamic_properties[method]
    else
      super
    end
  end
  
  def attributes
    (self.class.all_properties + @dynamic_properties.keys).inject({}) do |hash, prop|
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
  property :ambient => [0.4, 0.4, 0.4]
  property :background => [0.4, 0.8, 1]
  
  accessor :outfile
  
  def options(opts = nil)
    if opts
      @options.update(opts)
    else
      @options
    end
  end
  
  def initialize(attributes = {}, &block)
    @options = {}
    super
  end
  
  def save_to_file(name)
    File.open(name, 'w') do |file|
      file.puts to_json
    end
  end
  
  def render(file = nil, opts = {})
    file ||= outfile
    file ||= "out.png"
    
    if !options.delete(:overwrite) && File.exist?(file)
      puts "Not rendering #{file} since it already exists"
      return
    end
    
    puts "Rendering #{file} ..."
    time = Time.now
    file_name = "/tmp/render_#{time.to_i}_#{time.usec}"
    
    save_to_file(file_name)
    
    FileUtils.mkdir_p(File.dirname(file))
    
    ENV['DYLD_FRAMEWORK_PATH'] = "/Users/tkadauke/Qt/5.5/clang_64/lib"
    
    args = options.merge(opts).map { |key, value| "--#{key}=#{value}" }.join(" ")
    system "tools/rendercli/rendercli #{file_name} #{file} #{args}"
    
    FileUtils.rm(file_name)
  end
end

class Transformable < Element
  property :position => [0, 0, 0],
           :rotation => [0, 0, 0],
           :scale => [1, 1, 1]
end

class Surface < Transformable
  property :visible => true,
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
           :specularCoefficient => 0.5
end

class ReflectiveMaterial < PhongMaterial
  property :reflectionColor => [1, 1, 1],
           :reflectionCoefficient => 1
end

class TransparentMaterial < PhongMaterial
  property :refractionIndex => 1
end

class Box < Surface
  property :size => [1, 1, 1],
           :bevelRadius => 0
end

class Sphere < Surface
  property :radius => 1
end

class Cylinder < Surface
  property :radius => 1,
           :height => 2,
           :bevelRadius => 0
end

class Ring < Surface
  property :outerRadius => 1,
           :innerRadius => 0.5,
           :height => 2,
           :bevelRadius => 0
end

class ScriptedSurface < Surface
  property :scriptName => ""
end

class CSGSurface < Surface
  property :active => true
end

class Intersection < CSGSurface
end

class Union < CSGSurface
end

class Difference < CSGSurface
end

class MinkowskiSum < CSGSurface
end

class ConvexHull < CSGSurface
end

class Light < Transformable
  property :visible => true,
           :color => [1, 1, 1],
           :intensity => 1
end

class PointLight < Light
end

class DirectionalLight < Light
  property :direction => [0, 0, 1]
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

def scene(opts = {}, &block)
  Scene.new do
    options opts
    block.bind(self).call
  end.render
end
