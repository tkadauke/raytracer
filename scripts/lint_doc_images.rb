#!/usr/bin/env ruby
# Linter for Doxygen `@image html` references.
#
# Walks the public headers under `include/`, extracts every
# `@image html <name>.png` reference, and compares against what
# actually exists in `docs/images/`. Reports:
#
#   - **Broken refs** — `@image html foo.png` in a header but no
#     `docs/images/foo.png` on disk. This is what catches typos
#     (referenced "thin_lens_camera_dof_focal_4.png" but the
#     doc-render driver produces "thin_lens_camera_dof_focal_4.5.png").
#     These cause silent "image not found" placeholders in the
#     rendered Doxygen output.
#
#   - **Orphan PNGs** — `docs/images/foo.png` on disk but no header
#     references it. These are renders that nothing links to —
#     usually leftover from a renamed sweep (e.g. integer focal
#     distances from before we switched to 4.5/8/11.5). They take
#     up space and confuse anyone browsing the directory but
#     don't break anything; reported as warnings, not errors.
#
# Exits 0 if no broken refs, 1 otherwise. Orphans don't fail the
# lint by default — pass --strict to fail on them too.
#
# Run via `rake check:doc-images` or directly:
#
#     ruby scripts/lint_doc_images.rb [--strict]

require 'optparse'
require 'set'

ROOT = File.expand_path('..', __dir__)

# CLI options ---------------------------------------------------------------

options = { :strict => false }
OptionParser.new do |opts|
  opts.banner = "Usage: lint_doc_images [--strict]"
  opts.on("--strict", "Treat orphan PNGs as errors too") do
    options[:strict] = true
  end
end.parse!

# Scan headers --------------------------------------------------------------

# Pattern: `@image html <filename>` — Doxygen requires the keyword
# "html" so we don't match other @image variants (latex, rtf, …) that
# might pull from a separate filesystem path. Strict whitespace only:
# Doxygen accepts a single space/tab between tokens.
IMAGE_HTML_RE = /@image\s+html\s+(\S+\.png)/

referenced = Hash.new { |h, k| h[k] = [] }   # filename → [header_paths]

Dir.glob(File.join(ROOT, "include", "**", "*.h")).sort.each do |header|
  IO.foreach(header).with_index do |line, lineno|
    line.scan(IMAGE_HTML_RE) do |(filename)|
      referenced[filename] << "#{header}:#{lineno + 1}"
    end
  end
end

# Compare against on-disk PNGs ---------------------------------------------

images_dir = File.join(ROOT, "docs", "images")
on_disk = if Dir.exist?(images_dir)
  Dir.entries(images_dir).select { |f| f.end_with?(".png") }.to_set
else
  warn "lint_doc_images: docs/images/ does not exist; run `rake docs:render` first"
  Set.new
end

referenced_set = referenced.keys.to_set

broken  = referenced_set - on_disk
orphans = on_disk         - referenced_set

# Report --------------------------------------------------------------------

exit_code = 0

unless broken.empty?
  puts "Broken @image html references (#{broken.size}):"
  broken.sort.each do |filename|
    puts "  #{filename}"
    referenced[filename].each { |loc| puts "    referenced from #{loc.sub(ROOT + '/', '')}" }
  end
  puts
  exit_code = 1
end

unless orphans.empty?
  severity = options[:strict] ? "Orphan PNGs" : "Orphan PNGs (warning)"
  puts "#{severity} (#{orphans.size}):"
  orphans.sort.each { |f| puts "  docs/images/#{f}" }
  puts
  exit_code = 1 if options[:strict]
end

if broken.empty? && (orphans.empty? || !options[:strict])
  puts "OK: #{referenced.size} @image html references, all backed by docs/images/."
  puts "    (#{orphans.size} orphan PNGs ignored; pass --strict to flag)" unless orphans.empty?
end

exit(exit_code)
