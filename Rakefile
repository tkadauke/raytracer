require 'rake/clean'
require 'cgi'
require 'fileutils'

# The Rakefile is now a thin layer of project utilities. Compilation lives
# entirely under CMake (see CMakePresets.json); the tasks below either
# wrap CMake presets or run project-specific tooling that has no obvious
# CMake home (cppcheck, the inline-method convention check, line-count
# stats, and the Doxygen example-image renderer).

# Stale `.moc` and `ui_*.h` siblings left over from old Rakefile-driven
# builds shadow AUTOMOC's autogen output and produce duplicate-symbol link
# errors. CMake never writes into the source tree, so anything matching
# these patterns under src/, test/, or examples/ is dead.
CLEAN.include(Rake::FileList["src/**/*.moc", "test/**/*.moc", "examples/**/*.moc"])
CLEAN.include(Rake::FileList["src/**/ui_*.h", "examples/**/ui_*.h"])

RENDERCLI_DEFAULT = 'build/release/tools/rendercli/rendercli'
DOC_WIDGET_DEPENDENCIES = ["angle_from_x.js"].freeze
DOC_WIDGET_FRAME_DIR = "docs/html/widget-pages".freeze
DOC_IMAGE_GALLERY_PATH = "docs/html/rendered-images.html".freeze
DOC_IMAGE_GALLERY_ASSET_DIR = "docs/html/rendered-images".freeze
FIGURE_DEPENDENCY_PATTERN =
  /\b(new\s+(Canvas|Vector|Group|Line|Ray|Circle|Rectangle|Text|Axes|Path|Slider|DragHandler|OrderedHash|FigureWidget|FigureSvg|FigureSegmentedControl|FigureSliderControl|FigureDraggablePoint)|Path\.polyline|extends\s+AngleFromX)\b/

def docs_widget_scripts
  Dir.glob("scripts/docs/*.js")
    .map { |path| File.basename(path) }
    .reject { |file| file == "figure.js" || DOC_WIDGET_DEPENDENCIES.include?(file) }
    .sort
end

def docs_widget_title(file)
  File.basename(file, ".js").split("_").map(&:capitalize).join(" ")
end

def docs_widget_slug(file)
  File.basename(file, ".js").gsub(/[^a-z0-9]+/i, "-").downcase
end

def docs_widget_dependencies(file)
  source = File.read(File.join("scripts/docs", file))
  dependencies = []
  dependencies << "figure.js" if source.match?(FIGURE_DEPENDENCY_PATTERN)
  dependencies << "angle_from_x.js" if File.basename(file, ".js").start_with?("angle_from_")
  dependencies.uniq
end

def write_docs_widget_frame(path, widget)
  slug = docs_widget_slug(widget)
  scripts = (docs_widget_dependencies(widget) + [widget]).map do |file|
    version = File.mtime(File.join("scripts/docs", file)).to_i
    %(<script src="../#{CGI.escapeHTML(file)}?v=#{version}"></script>)
  end.join("\n    ")

  html = <<~HTML
    <!doctype html>
    <html lang="en">
    <head>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <style>
        html,
        body {
          margin: 0;
          background: #ffffff;
          color: #202020;
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
        }

        .widget-stage {
          box-sizing: border-box;
          overflow: auto;
          padding: 16px;
        }

        .widget-stage > script {
          display: none;
        }

        .widget-stage svg {
          max-width: 100%;
          height: auto;
        }
      </style>
    </head>
    <body>
      <div class="widget-stage">
    #{scripts}
      </div>
      <script>
        (() => {
          const id = "#{CGI.escapeHTML(slug)}";
          const reportHeight = () => {
            const height = Math.ceil(document.documentElement.scrollHeight);
            parent.postMessage({ type: "widget-gallery-resize", id, height }, "*");
          };
          window.addEventListener("load", reportHeight);
          if (typeof ResizeObserver !== "undefined") {
            new ResizeObserver(reportHeight).observe(document.body);
          } else {
            setTimeout(reportHeight, 100);
          }
        })();
      </script>
    </body>
    </html>
  HTML

  File.write(path, html)
end

def write_docs_widget_gallery(path, widgets)
  generated_at = Time.now.utc.strftime("%Y-%m-%d %H:%M:%S UTC")
  rows = widgets.map do |widget|
    title = CGI.escapeHTML(docs_widget_title(widget))
    filename = CGI.escapeHTML(widget)
    slug = docs_widget_slug(widget)
    <<~HTML
      <section class="widget-card" id="widget-#{slug}">
        <h2>#{title}</h2>
        <div class="widget-meta">#{filename}</div>
        <iframe
          title="#{title}"
          data-widget-id="#{slug}"
          src="widget-pages/#{slug}.html"
          loading="lazy"></iframe>
      </section>
    HTML
  end.join("\n")

  html = <<~HTML
    <!doctype html>
    <html lang="en">
    <head>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <title>Raytracer Widget Gallery</title>
      <style>
        :root {
          color-scheme: light;
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
          background: #f5f5f5;
          color: #202020;
        }

        body {
          margin: 0;
        }

        header {
          background: #ffffff;
          border-bottom: 1px solid #d8d8d8;
          padding: 24px clamp(16px, 4vw, 48px);
        }

        h1 {
          font-size: clamp(28px, 4vw, 42px);
          line-height: 1.1;
          margin: 0 0 8px 0;
        }

        header p {
          margin: 0;
          max-width: 820px;
          color: #555;
        }

        main {
          display: grid;
          grid-template-columns: repeat(auto-fit, minmax(360px, 1fr));
          gap: 18px;
          padding: 24px clamp(16px, 4vw, 48px) 48px;
        }

        .widget-card {
          background: #ffffff;
          border: 1px solid #d8d8d8;
          border-radius: 8px;
          overflow: hidden;
        }

        .widget-card h2 {
          font-size: 18px;
          line-height: 1.25;
          margin: 0;
          padding: 14px 16px 4px;
        }

        .widget-meta {
          color: #666;
          font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
          font-size: 12px;
          padding: 0 16px 12px;
        }

        iframe {
          border-top: 1px solid #e5e5e5;
          border-right: 0;
          border-bottom: 0;
          border-left: 0;
          display: block;
          height: 360px;
          width: 100%;
        }

        @media (max-width: 420px) {
          main {
            grid-template-columns: 1fr;
          }
        }
      </style>
    </head>
    <body>
      <header>
        <h1>Raytracer Widget Gallery</h1>
        <p>Generated by <code>rake docs:widgets</code> at #{generated_at}. This page loads #{widgets.length} interactive docs widgets from <code>scripts/docs</code> for quick visual regression checks.</p>
      </header>
      <main>
    #{rows}
      </main>
      <script>
        (() => {
          const frames = new Map(
            Array.from(document.querySelectorAll("iframe[data-widget-id]"))
              .map(frame => [frame.dataset.widgetId, frame])
          );
          window.addEventListener("message", event => {
            const data = event.data || {};
            if (data.type !== "widget-gallery-resize") return;
            const frame = frames.get(data.id);
            if (!frame) return;
            frame.style.height = `${Math.max(260, Number(data.height) || 0)}px`;
          });
        })();
      </script>
    </body>
    </html>
  HTML

  File.write(path, html)
end

def docs_rendered_images
  extensions = [".png", ".jpg", ".jpeg", ".gif", ".webp"]
  Dir.glob("docs/images/*")
    .select { |path| File.file?(path) && extensions.include?(File.extname(path).downcase) }
    .sort
end

def docs_image_title(path)
  File.basename(path, File.extname(path)).split("_").reject(&:empty?).map(&:capitalize).join(" ")
end

def docs_file_size_label(bytes)
  return "#{bytes} B" if bytes < 1024

  kib = bytes / 1024.0
  return format("%.1f KB", kib) if kib < 1024

  format("%.1f MB", kib / 1024.0)
end

def write_docs_image_gallery(path, images)
  generated_at = Time.now.utc.strftime("%Y-%m-%d %H:%M:%S UTC")
  rows = images.map do |image_path|
    filename = File.basename(image_path)
    title = CGI.escapeHTML(docs_image_title(image_path))
    escaped_filename = CGI.escapeHTML(filename)
    relative_path = "rendered-images/#{escaped_filename}"
    size = CGI.escapeHTML(docs_file_size_label(File.size(image_path)))
    filter_name = CGI.escapeHTML(filename.downcase)

    <<~HTML
      <figure class="image-card" data-name="#{filter_name}">
        <a href="#{relative_path}">
          <img
            src="#{relative_path}"
            alt="Rendered documentation image #{escaped_filename}"
            loading="lazy">
        </a>
        <figcaption>
          <span class="image-title">#{title}</span>
          <span class="image-meta">#{escaped_filename} &middot; #{size}</span>
        </figcaption>
      </figure>
    HTML
  end.join("\n")

  html = <<~HTML
    <!doctype html>
    <html lang="en">
    <head>
      <meta charset="utf-8">
      <meta name="viewport" content="width=device-width, initial-scale=1">
      <title>Raytracer Rendered Image Gallery</title>
      <style>
        :root {
          color-scheme: light;
          font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
          background: #f5f5f5;
          color: #202020;
        }

        body {
          margin: 0;
        }

        header {
          background: #ffffff;
          border-bottom: 1px solid #d8d8d8;
          padding: 24px clamp(16px, 4vw, 48px);
        }

        h1 {
          font-size: clamp(28px, 4vw, 42px);
          line-height: 1.1;
          margin: 0 0 8px 0;
        }

        header p {
          color: #555;
          margin: 0 0 16px 0;
          max-width: 900px;
        }

        .toolbar {
          align-items: center;
          display: flex;
          flex-wrap: wrap;
          gap: 12px;
        }

        input[type="search"] {
          border: 1px solid #bdbdbd;
          border-radius: 6px;
          box-sizing: border-box;
          font: inherit;
          min-width: min(100%, 320px);
          padding: 8px 10px;
        }

        #image-count {
          color: #555;
          font-size: 14px;
        }

        main {
          display: grid;
          gap: 18px;
          grid-template-columns: repeat(auto-fit, minmax(280px, 1fr));
          padding: 24px clamp(16px, 4vw, 48px) 48px;
        }

        .image-card {
          background: #ffffff;
          border: 1px solid #d8d8d8;
          border-radius: 8px;
          margin: 0;
          overflow: hidden;
        }

        .image-card[hidden] {
          display: none;
        }

        .image-card a {
          align-items: center;
          background:
            linear-gradient(45deg, #ececec 25%, transparent 25%),
            linear-gradient(-45deg, #ececec 25%, transparent 25%),
            linear-gradient(45deg, transparent 75%, #ececec 75%),
            linear-gradient(-45deg, transparent 75%, #ececec 75%),
            #ffffff;
          background-position: 0 0, 0 8px, 8px -8px, -8px 0;
          background-size: 16px 16px;
          border-bottom: 1px solid #e5e5e5;
          display: flex;
          min-height: 220px;
          padding: 12px;
        }

        img {
          display: block;
          height: auto;
          margin: auto;
          max-height: 260px;
          max-width: 100%;
          object-fit: contain;
        }

        figcaption {
          padding: 12px 14px 14px;
        }

        .image-title {
          display: block;
          font-weight: 700;
          line-height: 1.25;
          margin-bottom: 4px;
        }

        .image-meta {
          color: #666;
          display: block;
          font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
          font-size: 12px;
          overflow-wrap: anywhere;
        }

        @media (max-width: 420px) {
          main {
            grid-template-columns: 1fr;
          }
        }
      </style>
    </head>
    <body>
      <header>
        <h1>Raytracer Rendered Image Gallery</h1>
        <p>Generated by <code>rake docs:images</code> at #{generated_at}. This page loads #{images.length} rendered documentation images from <code>docs/images</code> for quick visual regression checks.</p>
        <div class="toolbar">
          <input id="image-filter" type="search" placeholder="Filter filenames" autocomplete="off">
          <span id="image-count"></span>
        </div>
      </header>
      <main>
    #{rows}
      </main>
      <script>
        (() => {
          const filter = document.getElementById("image-filter");
          const count = document.getElementById("image-count");
          const cards = Array.from(document.querySelectorAll(".image-card"));

          const update = () => {
            const query = filter.value.trim().toLowerCase();
            let visible = 0;
            cards.forEach(card => {
              const matches = card.dataset.name.includes(query);
              card.hidden = !matches;
              if (matches) visible += 1;
            });
            count.textContent = `${visible} / ${cards.length} images`;
          };

          filter.addEventListener("input", update);
          update();
        })();
      </script>
    </body>
    </html>
  HTML

  File.write(path, html)
end

desc "Build (debug) via CMake"
task :build do
  sh "cmake --preset debug"
  sh "cmake --build --preset debug --parallel"
end

desc "Build (release) via CMake"
task :release do
  sh "cmake --preset release"
  sh "cmake --build --preset release --parallel"
end

desc "Build and run the test suite"
task :test => :build do
  sh "ctest --preset debug --output-on-failure"
end

task :default => :test

namespace :docs do
  desc "Render docs images (auto-builds rendercli via CMake if missing)"
  task :render, [:only] do |t, args|
    rendercli = ENV.fetch('RENDERCLI', RENDERCLI_DEFAULT)
    unless File.executable?(rendercli)
      if rendercli == RENDERCLI_DEFAULT
        # Self-bootstrap only at the default path. If the user pointed
        # RENDERCLI at a custom binary, building something else underneath
        # them would be presumptuous.
        sh "cmake --preset release"
        sh "cmake --build --preset release --target rendercli --parallel"
      else
        fail "RENDERCLI=#{rendercli} not found or not executable."
      end
    end
    cmd = if args[:only]
      "ruby scripts/render_docs.rb --only #{args[:only]}"
    else
      "ruby scripts/render_docs.rb --missing"
    end
    sh({ "RENDERCLI" => File.expand_path(rendercli) }, cmd)
  end

  task :generate => [:render, :html]

  task :html do
    sh "doxygen"
    sh "cp scripts/docs/*.js docs/html"
  end

  desc "Create a standalone HTML gallery containing every interactive widget"
  task :widgets do
    widgets = docs_widget_scripts
    fail "No widgets found under scripts/docs" if widgets.empty?

    FileUtils.mkdir_p("docs/html")
    FileUtils.mkdir_p(DOC_WIDGET_FRAME_DIR)
    FileUtils.cp(Dir.glob("scripts/docs/*.js"), "docs/html")
    output = "docs/html/widgets.html"
    widgets.each do |widget|
      write_docs_widget_frame(File.join(DOC_WIDGET_FRAME_DIR, "#{docs_widget_slug(widget)}.html"), widget)
    end
    write_docs_widget_gallery(output, widgets)
    puts "Wrote #{output} with #{widgets.length} widgets"
  end

  desc "Create a standalone HTML gallery containing every rendered docs image"
  task :images do
    images = docs_rendered_images
    fail "No rendered images found under docs/images; run `rake docs:render` first" if images.empty?

    FileUtils.mkdir_p("docs/html")
    FileUtils.mkdir_p(DOC_IMAGE_GALLERY_ASSET_DIR)
    FileUtils.cp(images, DOC_IMAGE_GALLERY_ASSET_DIR)
    write_docs_image_gallery(DOC_IMAGE_GALLERY_PATH, images)
    puts "Wrote #{DOC_IMAGE_GALLERY_PATH} with #{images.length} images"
  end

  task :clean
  namespace :clean do
    task :html do
      rm_rf "docs/html"
    end
    
    task :images do
      rm_rf "docs/images"
    end
    
    task :latex do
      rm_rf "docs/latex"
    end
  end
  task :clean => ["docs:clean:html", "docs:clean:images", "docs:clean:latex"]
end

desc "Generate documentation"
task :docs => "docs:generate"

namespace :test do
  desc "Run the Ruby tests for the doc-render framework"
  task :"scripts:rb" do
    # Use minitest's autorun via direct file invocation. Each test file
    # `require's 'minitest/autorun'`, so just running them executes the
    # cases and exits non-zero on failure. Globbing them all in one
    # process keeps the report tight.
    test_files = Dir.glob("scripts/test/test_*.rb")
    if test_files.empty?
      puts "No Ruby script tests found under scripts/test/"
      next
    end
    sh "ruby -Iscripts -e \"#{test_files.map { |f| %{require '#{File.expand_path(f)}'} }.join('; ')}\""
  end

  desc "Run the JavaScript tests for the interactive widgets framework"
  task :"scripts:js" do
    # Each test file is a self-contained Node script using the
    # built-in `node:test` runner — no external dependencies. Run
    # in series so failures from one file don't get hidden by
    # another's output.
    test_files = Dir.glob("scripts/test/test_*.js")
    if test_files.empty?
      puts "No JS script tests found under scripts/test/"
      next
    end
    test_files.each { |f| sh "node #{f}" }
  end

  desc "Run all script-framework tests (Ruby + JS)"
  task :scripts => [:"scripts:rb", :"scripts:js"]
end

namespace :check do
  desc "Run cppcheck on the repository"
  task :cpp => :release do
    # --library=qt teaches cppcheck about Q_OBJECT / signals / slots so it
    # doesn't bail with unknownMacro on every widget header. --library=
    # googletest does the same for TEST / TEST_F / EXPECT_* in the test
    # tree. -i build/ skips the FetchContent GoogleTest tree and AUTOMOC
    # autogen sources that compile_commands.json otherwise pulls in.
    sh "cppcheck --project=build/release/compile_commands.json " \
       "--suppressions-list=.cppchecksuppress " \
       "--library=qt --library=googletest " \
       "-i build/ " \
       "--quiet --enable=all -j 24 --force"
  end

  desc "Lint Doxygen @image html refs against docs/images/ (broken refs fail; orphan PNGs warn)"
  task :"doc-images" do
    # Pass --strict to also fail on orphan PNGs (renders no header
    # references). Default is "broken refs are errors, orphans are
    # warnings" — keeps the lint useful pre-CI without flagging every
    # legacy unused render as a blocker.
    sh "ruby scripts/lint_doc_images.rb"
  end

  desc "Check that all inline methods are marked as such"
  task :inline do
    sh %q<
      find include -name '*.h' |
      xargs grep -in -vE '^\s+[\{\*]' |
      grep -E '\d+:\s+.*\(.*\).*\{$' |
      grep -vE 'class|for|if|typedef|union|else|static|struct|inline' ||
      /usr/bin/true
    >
  end
end

desc "Outputs test and code lines"
task :stats do
  test_lines = `find test | grep '\\.cpp\\|\\.h$' | xargs cat 2>/dev/null | wc -l`
  code_lines = `find src include | grep '\\.cpp\\|\\.h$' | xargs cat 2>/dev/null | wc -l`

  puts "Test lines: #{test_lines}"
  puts "Code lines: #{code_lines}"
  puts "Ratio: #{"%2.2f" % (test_lines.to_f / code_lines.to_f)}"
end
