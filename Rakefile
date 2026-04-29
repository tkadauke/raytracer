require 'rake/clean'

QT_BASE = '/opt/homebrew/Cellar/qt@5/5.15.17'
QT_BIN = "#{QT_BASE}/bin"
QT_LIB = "#{QT_BASE}/lib"
QT_LIBEXEC = "#{QT_BASE}/share/qt/libexec"

QT_FRAMEWORKS = ['QtCore', 'QtGui', 'QtWidgets', 'QtScript']
QT_INCLUDE_DIRS = QT_FRAMEWORKS.map { |f| "#{QT_LIB}/#{f}.framework/Headers" }

QT_MOC = "#{QT_BIN}/moc"
QT_UIC = "#{QT_BIN}/uic"

INCLUDE_DIR = File.expand_path(File.dirname(__FILE__) + '/include')
SRC_DIR = File.expand_path(File.dirname(__FILE__) + '/src')
WIDGETS_DIR = File.expand_path(File.dirname(__FILE__) + '/src/widgets')
EXAMPLES_DIR = File.expand_path(File.dirname(__FILE__) + '/examples')
TOOLS_DIR = File.expand_path(File.dirname(__FILE__) + '/tools')

SRC = FileList["#{SRC_DIR}/**/*.cpp"]
EXAMPLES_SRC = FileList["#{EXAMPLES_DIR}/**/*.cpp"]
TOOLS_SRC = FileList["#{TOOLS_DIR}/**/*.cpp"]

SRC_OBJ = SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }
EXAMPLES_OBJ = EXAMPLES_SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }
TOOLS_OBJ = TOOLS_SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }

EXAMPLES = FileList["#{EXAMPLES_DIR}/*"].select { |f| File.directory?(f) }
EXAMPLES_BIN = EXAMPLES.collect { |ex| "#{ex}/#{File.basename(ex)}" }
TOOLS = FileList["#{TOOLS_DIR}/*"].select { |f| File.directory?(f) }
TOOLS_BIN = TOOLS.collect { |ex| "#{ex}/#{File.basename(ex)}" }

INCLUDE_DIRS = ['.', INCLUDE_DIR, WIDGETS_DIR, QT_INCLUDE_DIRS].flatten
FRAMEWORKS = [QT_FRAMEWORKS].flatten

INCLUDES = INCLUDE_DIRS.collect { |dir| "-I#{dir}" }.join(' ')
FRAMEWORK_LINKS = "-F #{QT_LIB}"


if ENV['DEBUG']
  DEBUG_FLAGS = "-g -fno-inline -fsanitize=address"
else
  DEBUG_FLAGS = ""
end

WARNING_FLAGS = "-W -Wall -pedantic -Wno-extra-semi"  # " -Werror"
if ENV['COVERAGE']
  OPTIMIZE_FLAGS = "-O1"
  COVERAGE_FLAGS = "-fprofile-arcs -ftest-coverage"
else
  OPTIMIZE_FLAGS = "-O3 -funroll-loops -mtune=native"
  COVERAGE_FLAGS = ""
end

COMPILER_FLAGS = "-std=c++17"

C_FLAGS = "#{COMPILER_FLAGS} #{INCLUDES} #{FRAMEWORK_LINKS} #{DEBUG_FLAGS} #{OPTIMIZE_FLAGS} #{WARNING_FLAGS} #{COVERAGE_FLAGS}"
T_FLAGS = "#{COMPILER_FLAGS} #{INCLUDES} #{FRAMEWORK_LINKS} #{DEBUG_FLAGS} #{OPTIMIZE_FLAGS} #{WARNING_FLAGS} #{COVERAGE_FLAGS}"
CC = "g++"
#  --param max-inline-insns-single  --param inline-unit-growth --param large-function-growth
LD_FLAGS = "-F #{QT_LIB} #{FRAMEWORKS.collect { |l| "-framework #{l}" }.join(' ')} #{DEBUG_FLAGS}"

CLEAN.include(SRC_OBJ, EXAMPLES_OBJ, TOOLS_OBJ, EXAMPLES_BIN, TOOLS_BIN)
CLEAN.include(Rake::FileList["**/*.moc", "**/ui_*.h"])

task :default => [:examples, :tools]

@header_dependency_cache = {}

def header_dependencies(file, pwd = '')
  file_path = nil
  if File.exist?(file)
    file_path = file
  elsif file =~ /\.moc$/ || file =~ /^ui_[A-Za-z0-9_]+\.h$/
    return ["#{pwd}/#{file}"]
  else
    INCLUDE_DIRS.each do |dir|
      path = File.join(dir, file)
      file_path = path and break if File.exist?(path)
    end
  end

  if @header_dependency_cache[file_path]
    return @header_dependency_cache[file_path]
  end

  @header_dependency_cache[file_path] = if file_path
    headers = File.read(file_path).split("\n").grep(/#include \"/).collect { |inc| inc =~ /^#include \"(.*?)\"/; $1 }
    [file_path, headers.collect { |header| header_dependencies(header, File.dirname(file_path)) }].flatten
  else
    [File.join(pwd, file)]
  end

  @header_dependency_cache[file_path]
end

def dependencies(objfile)
  source_file = objfile.gsub(/\.o/, '.cpp')
  header_dependencies(source_file).uniq
end

# AUTOUIC convention: ui_<X>.h is generated from <X>.ui in the same directory.
rule(%r{(?:^|/)ui_[A-Za-z0-9_]+\.h$} => proc { |out|
  dir = File.dirname(out)
  base = File.basename(out).sub(/^ui_/, '').sub(/\.h$/, '')
  File.join(dir, "#{base}.ui")
}) do |t|
  sh %{#{QT_UIC} -o #{t.name} #{t.source}}
end

rule '.moc' => lambda { |mocfile| mocfile.sub(/src\//, 'include/').sub('.moc', '.h') } do |t|
  sh %{#{QT_MOC} -o #{t.name} #{t.source}}
end

rule '.o' => lambda { |objfile| dependencies(objfile) } do |t|
  if t.source =~ /Test\.cpp/
    sh %{#{CC} #{T_FLAGS} -o #{t.name} -c #{t.source}}
  else
    sh %{#{CC} #{C_FLAGS} -o #{t.name} -c #{t.source}}
  end
end

EXAMPLES.each do |example|
  src = FileList["#{example}/**/*.cpp"]
  obj = src.collect { |fn| fn.gsub(/\.cpp/, '.o') }
  output = "#{example}/#{File.basename(example)}"

  file output => [obj, SRC_OBJ].flatten do
    sh %{#{CC} -Os -o #{output} #{[SRC_OBJ, obj].flatten.join(' ')} #{LD_FLAGS}}
  end
end

TOOLS.each do |tool|
  src = FileList["#{tool}/**/*.cpp"]
  obj = src.collect { |fn| fn.gsub(/\.cpp/, '.o') }
  output = "#{tool}/#{File.basename(tool)}"

  file output => [obj, SRC_OBJ].flatten do
    sh %{#{CC} -Os -o #{output} #{[SRC_OBJ, obj].flatten.join(' ')} #{LD_FLAGS}}
  end
end

desc "Build examples"
task :examples => EXAMPLES_BIN

desc "Build tools"
task :tools => TOOLS_BIN

namespace :docs do
  desc "Render docs images"
  task :render, [:only] => :tools do |t, args|
    if args[:only]
      sh "ruby scripts/render_docs.rb --only #{args[:only]}"
    else
      sh "ruby scripts/render_docs.rb --missing"
    end
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

QT_LD = "DYLD_FRAMEWORK_PATH=#{QT_LIB}"

# The vendored gtest/gmock copies were removed when CMake switched to
# FetchContent for GoogleTest 1.14 (modernize.md §3.2). The Rakefile-driven
# test build relied on the in-tree copies and the deprecated MOCK_METHODn
# macros that come with them, so it can no longer build the test suite.
# Tests now live under CMake; the Rakefile is being retired (modernize.md §5).
namespace :test do
  task :build do
    fail <<~MSG
      The Rakefile no longer builds the test suite — the vendored gtest/gmock
      copies were removed in favor of GoogleTest 1.14 via CMake FetchContent.
      Use the CMake build instead:

          cmake --preset release
          cmake --build --preset release
          ctest --preset release
    MSG
  end

  task :units => :build
  task :functionals => :build
  task :run => :build
  task :coverage => :build
end

task :test => 'test:build'

namespace :check do
  desc "Run cppcheck on the repository"
  task :cpp do
    sh "cppcheck --suppressions-list=.cppchecksuppress --quiet --enable=all -j 24 --force #{INCLUDES} src examples tools"
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