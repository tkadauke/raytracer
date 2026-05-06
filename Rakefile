require 'rake/clean'

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

  task :clean do
    rm_rf "docs"
  end
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
