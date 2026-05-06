# Tests for the doc-render colour helpers in scripts/lib/colors.rb.
#
# These exist specifically because of the historical `def blue` bug
# where the memo target was `@green` (copy-paste typo) — every
# panorama render that used both `green()` and `blue()` produced two
# green spheres for who knows how long, until visual inspection
# caught it. The test below for "distinct colours are distinct
# instances" would have failed loudly the first time someone called
# both helpers in sequence.

$LOAD_PATH.unshift(File.expand_path('..', __dir__))

require 'minitest/autorun'
require 'lib/scene'
require 'lib/colors'

class ColorsTest < Minitest::Test
  # The Colors module is mixed into `ElementCreator`, not an arbitrary
  # Object — its helpers call `constant_color_texture(...)` which
  # `ElementCreator#method_missing` resolves dynamically by camelising
  # the method name to a class. A bare `Object.new.extend(Colors)`
  # tester gets a NoMethodError on the dispatch.
  #
  # A fresh creator per test case keeps the per-instance @red/@green/
  # @blue memos isolated so cross-contamination bugs surface.
  def setup
    @scene = Scene.new
    @ctx = ElementCreator.new(@scene)
  end

  def test_red_is_pure_red
    assert_equal [1, 0, 0], @ctx.red.color
  end

  def test_green_is_pure_green
    assert_equal [0, 1, 0], @ctx.green.color
  end

  def test_blue_is_pure_blue
    # Regression: this assertion FAILED with the historical
    # `@green ||= constant_color_texture(:color => [0, 0, 1])` bug.
    # When `green` was called first (memoising into @green), `blue`'s
    # `@green ||= ...` short-circuited and returned the green texture.
    @ctx.green                         # establish the memo collision
    assert_equal [0, 0, 1], @ctx.blue.color, \
      "blue() must return a blue texture, even after green() was called"
  end

  def test_yellow_is_pure_yellow
    assert_equal [1, 1, 0], @ctx.yellow.color
  end

  def test_white_black_grey
    assert_equal [1, 1, 1], @ctx.white.color
    assert_equal [0, 0, 0], @ctx.black.color
    assert_equal [0.6, 0.6, 0.6], @ctx.medium_grey.color
  end

  def test_colours_are_memoised
    # Same colour helper called twice should return the SAME object —
    # the memoisation is what keeps doc renders cheap (one JSON
    # texture per colour, not one per sphere).
    assert_same @ctx.red, @ctx.red
    assert_same @ctx.blue, @ctx.blue
  end

  def test_different_colours_are_distinct_instances
    # Direct regression test for the copy-paste-memo-target bug.
    # Each colour helper has its OWN memo slot; cross-contamination
    # between them is the failure mode this catches.
    refute_same @ctx.red, @ctx.green
    refute_same @ctx.green, @ctx.blue
    refute_same @ctx.blue, @ctx.yellow
    refute_same @ctx.red, @ctx.blue
  end
end
