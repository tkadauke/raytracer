require 'json'
require 'fileutils'
require 'securerandom'
require 'digest'
require_relative 'core_ext'

# The Ruby DSL backing the doc-render framework + scene scripts.
#
# This file mirrors the C++ Q_PROPERTY hierarchy in `include/world/objects/`
# as plain Ruby classes, plus a Scene class that knows how to serialize
# itself to JSON and shell out to `rendercli`. The DSL on top of those
# classes is what makes doc-render drivers like
# `scripts/docs/thin_lens_camera.rb` readable:
#
#     class_doc do
#       camera_scene
#       thin_lens_camera :position => [0, -1, -8], :focalDistance => 8
#     end
#
# `class_doc` is in `render_docs.rb`; `camera_scene` is a helper in
# `Common` (also `render_docs.rb`); `thin_lens_camera` is dispatched by
# `ElementCreator#method_missing` to instantiate `ThinLensCamera`. See
# `scripts/README.md` for the full architecture.
#
# The Ruby ↔ C++ class mirror is currently maintained by hand. Adding
# a new C++ Q_PROPERTY class means adding a matching `class Foo < Bar`
# block at the bottom of this file. See
# `docs/plans/framework-critique.md` §1 for the (deferred) plan to
# autogenerate this from the C++ metadata.

# Wraps an `Element` so DSL code inside a `scene { ... }` block can
# refer to child elements by camel-cased method name. Example:
#
#     scene do
#       pinhole_camera :position => [0, 0, -5]    # → PinholeCamera.new(...)
#       sphere :radius => 1                        # → Sphere.new(...)
#     end
#
# `method_missing` first tries to delegate to the wrapped element (for
# property setters like `position=`), then falls back to camelising the
# method name and instantiating that class as a child. Helpers like
# `red`, `sunlight`, `camera_scene` are mixed into ElementCreator via
# `ElementCreator.send :include, ...` (see `Colors`, `Lights`,
# `Materials`, `Objects`, `Cameras`, and `Common`); each is a module
# of pre-cooked element constructors with sensible defaults, memoised
# per-creator-instance so the JSON stays small.
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

# Base class for every entity in the scene tree — cameras, lights,
# materials, textures, primitives, and the scene itself. Subclasses
# declare their JSON-serialized fields via `property :foo => default`
# at the class level (see `Camera`, `Sphere`, etc. below); the values
# round-trip through `to_json` / `read` (the C++ side) by name.
#
# Each instance gets a per-run random UUID as its `id` (used as a
# reference target by other elements like a Surface's `material`
# slot). Per-run randomness is why staleness detection in
# `Scene#render` strips ids before hashing — a logically-identical
# scene re-rendered tomorrow produces different ids and would
# otherwise hash differently.
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

# Top-level scene container — root of every JSON file emitted by the
# Ruby DSL. Owns the canonical render pipeline:
#
#   1. The DSL block populates `@children` with cameras, lights,
#      surfaces, materials, textures.
#   2. `to_json` serializes the whole tree (inherited from `Element`).
#   3. `render` writes the JSON to a temp file, shells out to
#      `rendercli`, and writes a sidecar `.png.hash` for the
#      staleness check on the next run.
#
# Render options (sampler, samples_per_pixel, width, height,
# overwrite, ...) are passed via `options(opts)` from the DSL —
# they map 1-to-1 to rendercli's `--key=value` flags.
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

    json_str = to_json
    overwrite = options.delete(:overwrite)
    hash_file = "#{file}.hash"
    new_hash = Scene.scene_hash(json_str, options.merge(opts))

    # Three states for an existing on-disk image:
    #   - file present + sidecar hash matches  → up to date, skip
    #   - file present + sidecar hash differs  → driver changed, re-render
    #   - file present + no sidecar hash       → produced by an older
    #                                            version of this script;
    #                                            re-render so the sidecar
    #                                            hash gets written
    # If `overwrite` is true (the per-scene opt-out), force render
    # regardless. The user-facing `--missing` flag flips overwrite OFF
    # for the whole run; without `--missing`, every render is forced
    # (matches the historical pre-staleness behaviour).
    if !overwrite && File.exist?(file) && File.exist?(hash_file) &&
       File.read(hash_file).strip == new_hash
      puts "Skipping #{file} (up to date)"
      return
    end

    puts "Rendering #{file} ..."
    time = Time.now
    file_name = "/tmp/render_#{time.to_i}_#{time.usec}"

    save_to_file(file_name)

    FileUtils.mkdir_p(File.dirname(file))

    render_options = options.merge(opts)
    args = Scene.render_args(render_options)
    rendercli = ENV.fetch('RENDERCLI', 'build/release/tools/rendercli/rendercli')
    if system Scene.render_env(render_options), "#{rendercli} #{file_name} #{file} #{args}"
      File.write(hash_file, new_hash)
    end

    FileUtils.rm(file_name)
  end

  # Compute a content hash of (scene JSON + render options) suitable
  # for staleness detection. The JSON's element IDs are random UUIDs
  # generated per-run via SecureRandom (see Element#initialize), so a
  # raw hash of `to_json` would change on every run. Auto-generated
  # names also include a process-global object counter, so adding an
  # object to one docs scene would otherwise invalidate the hashes for
  # later scenes in the same `rake docs:render` run. Strip those
  # non-rendering labels out so the hash captures the SCENE structure,
  # not per-run metadata.
  #
  # The render options (sampler, samples_per_pixel, width, height) are
  # included too — changing the sample count should re-render even if
  # the scene is otherwise identical.
  def self.scene_hash(json_str, opts)
    normalized = normalize_hash_payload(JSON.parse(json_str))
    payload = "#{JSON.generate(normalized)}\n#{opts.sort.to_h.inspect}"
    Digest::SHA1.hexdigest(payload)
  end

  def self.render_args(opts)
    opts.map do |key, value|
      if value == true
        "--#{key}"
      elsif value == false || value.nil?
        nil
      else
        "--#{key}=#{value}"
      end
    end.compact.join(" ")
  end

  def self.render_env(opts)
    backend = opts[:raster_backend] || opts["raster_backend"]
    if ["gl", "gpu", "opengl"].include?(backend.to_s.downcase)
      {"RAYTRACER_ALLOW_RENDERCLI_COCOA_OPENGL" => "1"}
    else
      {}
    end
  end

  def self.normalize_hash_payload(value)
    case value
    when Hash
      value.keys.sort.each_with_object({}) do |key, result|
        result[key] =
          if key == "id"
            "<id>"
          elsif key == "name"
            "<name>"
          else
            normalize_hash_payload(value[key])
          end
      end
    when Array
      value.map { |item| normalize_hash_payload(item) }
    when String
      value.match?(/\A\{[a-f0-9-]+\}\z/i) ? "<id>" : value
    else
      value
    end
  end
end

class Transformable < Element
  property :position => [0, 0, 0],
           :rotation => [0, 0, 0],
           :scale => [1, 1, 1]
end

class Surface < Transformable
  property :visible => true,
           :material => nil,
           :velocity => [0, 0, 0]
end

class Material < Element
end

class MatteMaterial < Material
  property :diffuseTexture => nil,
           :normalTexture => nil,
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
  property :refractionIndex => 1,
           :transmissionCoefficient => 1,
           :reflectionColor => [0, 0, 0],
           :reflectionCoefficient => 0
end

class PortalMaterial < Material
  property :position => [0, 0, 0],
           :rotation => [0, 0, 0],
           :scale => [1, 1, 1],
           :filterColor => [1, 1, 1]
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

class OpenCylinder < Surface
  property :radius => 1,
           :height => 2
end

class Disk < Surface
  property :radius => 1
end

class Triangle < Surface
  property :vertexA => [ 1, 0, 0],
           :vertexB => [-1, 0, 0],
           :vertexC => [ 0, -1, 0]
end

class Rectangle < Surface
  property :leg1 => [1, 0, 0],
           :leg2 => [0, 0, 1]
end

class Torus < Surface
  property :sweptRadius => 2,
           :tubeRadius => 1
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

class RectangularAreaLight < Light
  property :width => 2,
           :height => 2
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

class ThinLensCamera < Camera
  property :distance => 5,
           :zoom => 1,
           :apertureRadius => 0.1,
           :focalDistance => 5
end

class TiltShiftCamera < ThinLensCamera
  property :tilt => 0.degrees,
           :shiftX => 0,
           :shiftY => 0
end

class EquirectangularCamera < Camera
end

class SphericalCamera < Camera
  property :horizontalFieldOfView => 180.degrees,
           :verticalFieldOfView => 120.degrees
end

class Texture < Element
end

class CheckerBoardTexture < Texture
  property :brightTexture => nil,
           :darkTexture => nil,
           :mapping => "planar",
           :uScale => 1,
           :vScale => 1
end

class ConstantColorTexture < Texture
  property :color => [0, 0, 0]
end

class ImageTexture < Texture
  property :path => "",
           :filter => "nearest",
           :wrap => "repeat",
           :mapping => "uv",
           :uScale => 1,
           :vScale => 1
end

class UVColorTexture < Texture
end

def scene(opts = {}, &block)
  Scene.new do
    options opts
    block.bind(self).call
  end.render
end
