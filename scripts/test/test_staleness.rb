# Tests for the staleness-detection logic in Scene#render via
# Scene.scene_hash. The hash determines whether `rake docs:render`
# (with --missing) re-renders an existing PNG or skips it.
#
# The load-bearing property: identical scenes produce the same hash
# across runs (so `--missing` skips when nothing changed), while
# scenes that differ in any meaningful way produce different hashes
# (so `--missing` re-renders when the driver script was edited).

$LOAD_PATH.unshift(File.expand_path('..', __dir__))

require 'minitest/autorun'
require 'lib/scene'

class SceneHashTest < Minitest::Test
  # Each scene gets fresh element IDs (random UUIDs from
  # SecureRandom) on every initialize. The hash function strips
  # those out so it captures the SCENE STRUCTURE, not the per-run
  # random labels.

  def hash_of(json_str, opts = {})
    Scene.scene_hash(json_str, opts)
  end

  def test_same_structure_different_uuids_same_hash
    a = '{"id":"{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}","type":"Sphere"}'
    b = '{"id":"{11111111-2222-3333-4444-555555555555}","type":"Sphere"}'
    assert_equal hash_of(a), hash_of(b),
      "Hash must ignore element IDs (per-run random UUIDs); otherwise " \
      "every render of the same scene re-renders. Regression here " \
      "defeats the whole point of staleness detection."
  end

  def test_different_structure_different_hash
    a = '{"id":"{a-a-a-a-a}","type":"Sphere","radius":1}'
    b = '{"id":"{a-a-a-a-a}","type":"Sphere","radius":2}'
    refute_equal hash_of(a), hash_of(b),
      "Hash must change when scene structure changes (here, the " \
      "sphere radius). Otherwise editing a doc-render driver doesn't " \
      "trigger a re-render."
  end

  def test_options_change_changes_hash
    # If samples_per_pixel changes, we want a re-render even though the
    # scene JSON is byte-identical — more samples = better image.
    json = '{"id":"{a-a-a-a-a}","type":"Sphere"}'
    h1 = hash_of(json, :samples_per_pixel => 16)
    h2 = hash_of(json, :samples_per_pixel => 64)
    refute_equal h1, h2,
      "Hash must factor in the render options. Bumping " \
      "samples_per_pixel without changing the scene should still " \
      "trigger re-render."
  end

  def test_uuid_property_references_normalized
    # Material/texture references in JSON are stored as the target
    # element's UUID. Same property, two different UUIDs → same scene
    # logically; the hash should agree.
    a = '{"material":"{aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee}"}'
    b = '{"material":"{11111111-2222-3333-4444-555555555555}"}'
    assert_equal hash_of(a), hash_of(b)
  end

  def test_generated_names_do_not_affect_hash
    # Element#initialize assigns auto names from a process-global object
    # counter, e.g. "Sphere 17". Adding a new object in an earlier docs
    # scene changes those numbers for every later scene in the same run,
    # but names do not affect rendered pixels.
    a = '{"children":[{"type":"Sphere","name":"Sphere 17","radius":1}]}'
    b = '{"children":[{"type":"Sphere","name":"Sphere 42","radius":1}]}'
    assert_equal hash_of(a), hash_of(b),
      "Hash must ignore element names; otherwise adding objects to one " \
      "docs driver makes unrelated later images look stale."
  end

  def test_scene_hash_is_order_independent_for_json_objects
    a = '{"type":"Sphere","radius":1,"name":"Sphere 17"}'
    b = '{"name":"Sphere 42","radius":1,"type":"Sphere"}'
    assert_equal hash_of(a), hash_of(b)
  end

  def test_options_order_independent
    # render_options is a Ruby Hash; keys aren't ordered. Make sure
    # `{a:1, b:2}` and `{b:2, a:1}` produce the same hash — otherwise
    # the test is non-deterministic.
    json = '{"type":"Scene"}'
    h1 = hash_of(json, :samples_per_pixel => 16, :sampler => "Regular")
    h2 = hash_of(json, :sampler => "Regular", :samples_per_pixel => 16)
    assert_equal h1, h2
  end

  def test_hash_is_stable_string
    # Sanity: hash returns a non-empty hex string. Used as a sidecar
    # `.png.hash` file content; if it's ever empty or non-deterministic
    # the staleness check breaks subtly.
    h = hash_of('{"type":"Scene"}')
    assert_match(/\A[a-f0-9]{40}\z/, h, "Expected SHA1-hex sidecar form")
  end

  def test_boolean_render_options_emit_flag_style_arguments
    assert_equal "--shadow_maps --width=32",
                 Scene.render_args(:shadow_maps => true, :width => 32)
    assert_equal "--width=32",
                 Scene.render_args(:shadow_maps => false, :width => 32)
  end

  def test_opengl_raster_docs_opt_into_macos_cocoa_context
    assert_equal({"RAYTRACER_ALLOW_RENDERCLI_COCOA_OPENGL" => "1"},
                 Scene.render_env(:raster_backend => "opengl"))
    assert_equal({"RAYTRACER_ALLOW_RENDERCLI_COCOA_OPENGL" => "1"},
                 Scene.render_env("raster_backend" => "gpu"))
    assert_equal({}, Scene.render_env(:raster_backend => "cpu"))
  end
end
