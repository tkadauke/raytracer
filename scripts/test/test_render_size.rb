# Tests for the render_size DSL hook in scripts/render_docs.rb.
# Pins the aspect-ratio overrides added to support panoramic /
# square / future cameras.

$LOAD_PATH.unshift(File.expand_path('..', __dir__))

require 'minitest/autorun'
require 'render_docs'

class RenderSizeTest < Minitest::Test
  # render_size is private on DocsRenderer; reach in via send so the
  # test doesn't have to invent a public exposure that production
  # code doesn't need.
  def render_size(*args, **kwargs)
    DocsRenderer.new({}).send(:render_size, *args, **kwargs)
  end

  # ---- Width presets -----------------------------------------------------

  def test_class_doc_width_is_640
    assert_equal 640, render_size(1)[:width]
  end

  def test_property_doc_5_width_is_240
    assert_equal 240, render_size(5)[:width]
  end

  def test_rainbow_doc_7_width_is_160
    assert_equal 160, render_size(7)[:width]
  end

  def test_unknown_count_raises
    err = assert_raises(RuntimeError) { render_size(42) }
    assert_match(/render size for 42/, err.message)
  end

  # ---- Aspect ratios -----------------------------------------------------

  def test_default_aspect_is_4_to_3
    s = render_size(1, aspect: :default)
    assert_equal 640, s[:width]
    assert_equal 480, s[:height]
    assert_in_delta 4.0 / 3.0, s[:width].to_f / s[:height], 0.001
  end

  def test_panoramic_aspect_is_2_to_1
    # Equirectangular cameras need this — without it, spheres render
    # as ovals because of the 4:3 default's vertical stretch.
    s = render_size(1, aspect: :panoramic)
    assert_equal 640, s[:width]
    assert_equal 320, s[:height]
    assert_in_delta 2.0, s[:width].to_f / s[:height], 0.001
  end

  def test_square_aspect_is_1_to_1
    s = render_size(1, aspect: :square)
    assert_equal s[:width], s[:height]
  end

  def test_aspect_applies_at_property_doc_width_too
    s = render_size(5, aspect: :panoramic)
    assert_equal 240, s[:width]
    assert_equal 120, s[:height]
  end

  def test_unknown_aspect_raises_with_helpful_message
    err = assert_raises(RuntimeError) { render_size(1, aspect: :potato) }
    assert_match(/Unknown aspect ratio/, err.message)
    # The error names valid choices to help the next person picking
    # an aspect figure out what's available.
    assert_match(/:default/, err.message)
    assert_match(/:panoramic/, err.message)
  end

  def test_default_aspect_kept_when_keyword_omitted
    # Backward-compat smoke: the `aspect:` arg defaults to `:default`,
    # so any pre-existing call to render_size(num) without the keyword
    # gets the historical 4:3 size.
    assert_equal render_size(1, aspect: :default), render_size(1)
    assert_equal render_size(5, aspect: :default), render_size(5)
    assert_equal render_size(7, aspect: :default), render_size(7)
  end
end
