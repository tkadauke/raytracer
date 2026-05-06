require 'optparse'
require_relative 'lib/scene'
require_relative 'lib/colors'
require_relative 'lib/lights'
require_relative 'lib/materials'
require_relative 'lib/objects'
require_relative 'lib/cameras'

# Top-level CLI for the doc-render framework. See `scripts/README.md`
# for the full architecture; this file is the entry point that wires
# everything together. When invoked as a script (the
# `if __FILE__ == $PROGRAM_NAME` block at the bottom), it parses
# CLI flags and walks every `.rb` file under `scripts/docs/`.

# `Common` mixes the `name` helper plus the canonical scene-setup
# helpers (`camera_scene`, `dof_scene`, `panorama_scene`,
# `material_scene`, ...) into `ElementCreator`. Adding a new helper
# here makes it available inside every doc-render driver block —
# useful when the same scene-setup pattern shows up across multiple
# drivers (e.g. cameras all want `camera_scene`; DOF cameras want
# `dof_scene`; panoramic cameras want `panorama_scene`).
#
# The helpers are designed to compose: `panorama_scene` calls
# `sunlight` and `checker_board`; you can mix and match in your
# driver. Each helper is a sequence of element constructions that
# get attached to the implicit current Scene via
# `ElementCreator#method_missing`.
module Common
  # Set the output path for this image. Every doc-render driver
  # should call `name` first inside each `class_doc` /
  # `property_doc` block — otherwise the renderer falls back to
  # `out.png` (which would overwrite itself across drivers).
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

  # Scene for demonstrating panorama / 360° cameras (equirectangular,
  # full-FOV spherical, etc.). Cardinal-direction spheres so the unwrap
  # has identifiable content at every longitude — front (red), right
  # (green), back (blue), left (yellow), plus a checkered floor for
  # vertical reference.
  #
  # Camera lives at (0, -1, 0): the floor is at y=1.1 so the camera sits
  # 2 m above it, putting the horizon line near the equator of the
  # equirect output.
  def panorama_scene
    sunlight
    checker_board
    sphere :material => matte_material(:diffuseTexture => red),
           :position => [0, -1, 4]
    sphere :material => matte_material(:diffuseTexture => green),
           :position => [4, -1, 0]
    sphere :material => matte_material(:diffuseTexture => blue),
           :position => [0, -1, -4]
    sphere :material => matte_material(:diffuseTexture => yellow),
           :position => [-4, -1, 0]
  end

  # Scene for demonstrating depth-of-field cameras. Three coloured
  # spheres arranged front-to-back along the camera axis on a checker
  # board, so a DOF render shows clear sharp/blur transitions between
  # them.
  #
  # The spheres are sized and spaced to make the DOF effect read clearly
  # at Doxygen thumbnail size (~150-200 px wide):
  #   - Larger spheres (full unit radius) so the bokeh-disc edge is
  #     visible within a small image, not just a few aliased pixels.
  #   - Tighter Z-spacing (-2.5 / 0 / +2.5) so all three fit comfortably
  #     in a moderate framing without a wide-angle camera.
  #   - Distinct primary colours, used in the canonical DSL form
  #     (`matte_material(:diffuseTexture => red)`); `red`/`green`/`blue`
  #     in scripts/lib/colors.rb are already constant-colour textures,
  #     not RGB tuples — nesting them inside another texture (which an
  #     earlier version did) silently produces black spheres.
  def dof_scene
    sunlight
    checker_board
    sphere :material => matte_material(:diffuseTexture => red),
           :position => [-2.0, -1, -2.5]
    sphere :material => matte_material(:diffuseTexture => green),
           :position => [0, -1, 0]
    sphere :material => matte_material(:diffuseTexture => blue),
           :position => [2.0, -1, 2.5]
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

# Owns the lifecycle of a doc-render run:
#
#   1. Constructed with the parsed CLI `options` hash (`:samples_per_pixel`,
#      `:filter`, `:missing`).
#   2. `run` walks every `scripts/docs/*.rb` file matching `:filter`
#      and `eval`s it in this renderer's binding — so the driver
#      script's top-level `class_doc { ... }` call resolves to the
#      method below.
#   3. Each `class_doc` / `property_doc` / `rainbow_doc` wraps
#      `doc_scene`, which builds a `Scene`, sets render options,
#      runs the driver block to populate children, and dispatches to
#      `Scene#render` for the JSON-write + rendercli-shell-out +
#      sidecar-hash.
class DocsRenderer
  def initialize(options)
    @options = options
  end

  # Build and render a documentation scene.
  #
  # `options` contains rendercli flags as a hash —
  # `{:width => 640, :height => 480, :sampler => "Jittered",
  # :samples_per_pixel => 64}`. The block defines the scene contents
  # (children, camera, lights, materials) via the DSL.
  #
  # The default sampler is `Regular` because doc renders prioritise
  # determinism and quick re-runs over Monte-Carlo polish; specific
  # drivers can override (e.g. ThinLens needs a stochastic sampler
  # for usable bokeh).
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

  # Produce one image at the canonical 640-px-wide size. Use for the
  # "this is what the class looks like at default settings" hero
  # image referenced by the class-level `@image html` in the C++
  # docstring. Pass `aspect: :panoramic` (or `:square`) for cameras
  # that need a non-4:3 framing. Extra keyword arguments (e.g.
  # `sampler: "Jittered"`, `samples_per_pixel: 64`) override the
  # corresponding `doc_scene` defaults — useful for stochastic
  # cameras like ThinLens that need a real Monte-Carlo sampler to
  # produce smooth bokeh.
  def class_doc(aspect: :default, **options, &block)
    doc_scene render_size(1, aspect: aspect).merge(options) do
      block.bind(self).call
    end
  end

  # Produce one image per entry in `rainbow_colors`, named by colour.
  # Used by the matte-/phong-/etc.-material drivers to show what the
  # material looks like across the visible spectrum.
  def rainbow_doc(aspect: :default, **options, &block)
    rainbow_colors.each do |name, color|
      doc_scene render_size(7, aspect: aspect).merge(options) do
        block.bind(self).call(name, color)
      end
    end
  end

  # Produce N (default 5) images, calling the block once per `i` in
  # 1..N. Used for parameter sweeps — the block typically computes
  # the parameter value from `i` and constructs a scene with it.
  # Keep `num` ≤ a small handful (5 is the de-facto standard) so the
  # resulting per-setter `<table>` of images doesn't wrap awkwardly
  # in the rendered Doxygen page.
  def property_doc(num = 5, aspect: :default, **options, &block)
    1.upto(num) do |i|
      doc_scene render_size(num, aspect: aspect).merge(options) do
        block.bind(self).call(i)
      end
    end
  end

  # Walk every `scripts/docs/*.rb` driver matching the `:filter`
  # regexp and eval it in this renderer's binding. Each driver's
  # top-level `class_doc { ... }` / `property_doc { ... }` calls
  # then resolve to the methods above.
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

  # Render dimensions for documentation images. The first argument
  # picks the width preset (1 image = 640 px wide for class_doc; 5
  # images = 240 px wide for property_doc; 7 = 160 px wide for
  # rainbow_doc). The keyword `aspect:` then determines the height
  # relative to that width:
  #
  #   :default   — 4:3 (the historical default; suits perspective
  #                cameras and most materials).
  #   :panoramic — 2:1 (equirectangular and other full-sphere
  #                projections; without this the spheres render as
  #                ovals because of pixel-aspect mismatch).
  #   :square    — 1:1 (cubemap faces, sphere primitives, anything
  #                rotation-symmetric).
  #
  # Add new aspects here when a future camera/material needs one;
  # don't reach into doc_scene with raw width/height in the per-doc
  # script unless the aspect is genuinely one-off — uniform aspects
  # are easier to skim across the rendered docs page.
  def render_size(num, aspect: :default)
    base_width = case num
      when 1 then 640
      when 5 then 240
      when 7 then 160
      else
        raise "Unknown render size for #{num} images"
      end

    case aspect
    when :default
      { :width => base_width, :height => (base_width * 0.75).to_i }
    when :panoramic
      { :width => base_width, :height => base_width / 2 }
    when :square
      { :width => base_width, :height => base_width }
    else
      raise "Unknown aspect ratio: #{aspect.inspect} (try :default, :panoramic, or :square)"
    end
  end
end

# Only run the CLI when invoked as a script. Tests `require` this file
# to reach the `render_size` helper without triggering the top-level
# CLI parsing + `.run` (which would try to render every doc image
# during test setup).
if __FILE__ == $PROGRAM_NAME
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
end
