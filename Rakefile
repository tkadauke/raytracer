require 'rake/clean'

QT_BASE = '/usr/local/Qt-5.5.1'
QT_BIN = "#{QT_BASE}/bin"
QT_LIB = "#{QT_BASE}/lib"

QT_FRAMEWORKS = ['QtCore', 'QtGui', 'QtWidgets', 'QtScript']
QT_INCLUDE_DIRS = QT_FRAMEWORKS.map { |f| "#{QT_LIB}/#{f}.framework/Headers" }

QT_MOC = "#{QT_BIN}/moc"
QT_UIC = "#{QT_BIN}/uic"

INCLUDE_DIR = File.expand_path(File.dirname(__FILE__) + '/include')
SRC_DIR = File.expand_path(File.dirname(__FILE__) + '/src')
WIDGETS_DIR = File.expand_path(File.dirname(__FILE__) + '/src/widgets')
GTEST_DIR = File.expand_path(File.dirname(__FILE__) + '/gtest')
GMOCK_DIR = File.expand_path(File.dirname(__FILE__) + '/gmock')
TEST_DIR = File.expand_path(File.dirname(__FILE__) + '/test')
UNIT_TEST_DIR = File.expand_path(File.dirname(__FILE__) + '/test/unit')
FUNCTIONAL_TEST_DIR = File.expand_path(File.dirname(__FILE__) + '/test/functional')
TEST_HELPER_DIR = File.expand_path(File.dirname(__FILE__) + '/test/helpers')
EXAMPLES_DIR = File.expand_path(File.dirname(__FILE__) + '/examples')
TOOLS_DIR = File.expand_path(File.dirname(__FILE__) + '/tools')

SRC = FileList["#{SRC_DIR}/**/*.cpp"]
UNIT_TEST = FileList["#{UNIT_TEST_DIR}/**/*.cpp"]
FUNCTIONAL_TEST = FileList["#{FUNCTIONAL_TEST_DIR}/**/*.cpp"]
TEST_HELPER = FileList["#{TEST_HELPER_DIR}/**/*.cpp"]
GTEST = FileList["#{GTEST_DIR}/**/*.cpp", "#{GMOCK_DIR}/**/*.cpp"]
EXAMPLES_SRC = FileList["#{EXAMPLES_DIR}/**/*.cpp"]
TOOLS_SRC = FileList["#{TOOLS_DIR}/**/*.cpp"]

SRC_OBJ = SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }
UNIT_TEST_OBJ = UNIT_TEST.collect { |fn| fn.gsub(/\.cpp/, '.o') }
FUNCTIONAL_TEST_OBJ = FUNCTIONAL_TEST.collect { |fn| fn.gsub(/\.cpp/, '.o') }
TEST_HELPER_OBJ = TEST_HELPER.collect { |fn| fn.gsub(/\.cpp/, '.o') }
GTEST_OBJ = GTEST.collect { |fn| fn.gsub(/\.cpp/, '.o') }
EXAMPLES_OBJ = EXAMPLES_SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }
TOOLS_OBJ = TOOLS_SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }

UNIT_TEST_BIN = "#{UNIT_TEST_DIR}/tests.run"
FUNCTIONAL_TEST_BIN = "#{FUNCTIONAL_TEST_DIR}/tests.run"
EXAMPLES = FileList["#{EXAMPLES_DIR}/*"]
EXAMPLES_BIN = EXAMPLES.collect { |ex| "#{ex}/#{File.basename(ex)}" }
TOOLS = FileList["#{TOOLS_DIR}/*"]
TOOLS_BIN = TOOLS.collect { |ex| "#{ex}/#{File.basename(ex)}" }

INCLUDE_DIRS = ['.', INCLUDE_DIR, WIDGETS_DIR, GTEST_DIR, GMOCK_DIR, QT_INCLUDE_DIRS].flatten
FRAMEWORKS = [QT_FRAMEWORKS].flatten

INCLUDES = INCLUDE_DIRS.collect { |dir| "-I#{dir}" }.join(' ')
FRAMEWORK_LINKS = "-F #{QT_LIB}"


if ENV['DEBUG']
  DEBUG_FLAGS = "-g -fno-inline"
else
  DEBUG_FLAGS = ""
end

WARNING_FLAGS = "-W -Wall"
if ENV['COVERAGE']
  OPTIMIZE_FLAGS = "-O1"
  COVERAGE_FLAGS = "-fprofile-arcs -ftest-coverage"
else
  OPTIMIZE_FLAGS = "-O3 -funroll-loops -mtune=native"
  COVERAGE_FLAGS = ""
end

COMPILER_FLAGS = "-std=c++14"

C_FLAGS = "#{COMPILER_FLAGS} #{INCLUDES} #{FRAMEWORK_LINKS} #{DEBUG_FLAGS} #{OPTIMIZE_FLAGS} #{WARNING_FLAGS} #{COVERAGE_FLAGS}"
T_FLAGS = "#{COMPILER_FLAGS} #{INCLUDES} #{FRAMEWORK_LINKS} #{OPTIMIZE_FLAGS} #{WARNING_FLAGS} #{COVERAGE_FLAGS}"
CC = "g++"
#  --param max-inline-insns-single  --param inline-unit-growth --param large-function-growth
LD_FLAGS = "-F #{QT_LIB} #{FRAMEWORKS.collect { |l| "-framework #{l}" }.join(' ')}"

CLEAN.include(SRC_OBJ, UNIT_TEST_OBJ, FUNCTIONAL_TEST_OBJ, TEST_HELPER_OBJ, GTEST_OBJ, EXAMPLES_OBJ, TOOLS_OBJ, UNIT_TEST_BIN, FUNCTIONAL_TEST_BIN, EXAMPLES_BIN, TOOLS_BIN)
CLEAN.include(Rake::FileList["**/*.moc", "**/*.uic"])

task :default => [:examples, :tools, :test]

@header_dependency_cache = {}

def header_dependencies(file, pwd = '')
  file_path = nil
  if File.exist?(file)
    file_path = file
  elsif file =~ /\.(moc|uic)$/
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

rule '.uic' => '.ui' do |t|
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

file UNIT_TEST_BIN => [SRC_OBJ, UNIT_TEST_OBJ, TEST_HELPER_OBJ, GTEST_OBJ].flatten do
  sh %{#{CC} -Os -o #{UNIT_TEST_BIN} #{[SRC_OBJ, UNIT_TEST_OBJ, TEST_HELPER_OBJ, GTEST_OBJ].flatten.join(' ')} #{LD_FLAGS}}
end

file FUNCTIONAL_TEST_BIN => [SRC_OBJ, FUNCTIONAL_TEST_OBJ, TEST_HELPER_OBJ, GTEST_OBJ].flatten do
  sh %{#{CC} -Os -o #{FUNCTIONAL_TEST_BIN} #{[SRC_OBJ, FUNCTIONAL_TEST_OBJ, TEST_HELPER_OBJ, GTEST_OBJ].flatten.join(' ')} #{LD_FLAGS}}
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
      sh "ruby scripts/render_docs.rb"
    end
  end
  
  task :generate => :render do
    sh "doxygen"
  end
  
  task :clean do
    rm_rf "docs"
  end
end

desc "Generate documentation"
task :docs => "docs:generate"

QT_LD = "DYLD_FRAMEWORK_PATH=#{QT_LIB}"

namespace :test do
  task :build => [UNIT_TEST_BIN, FUNCTIONAL_TEST_BIN]
  task :run => [:units, :functionals]
  
  desc "Run all unit tests"
  task :units => :build do
    if ENV['ONLY']
      sh("#{QT_LD} #{UNIT_TEST_BIN} --gtest_filter=#{ENV['ONLY']}")
    else
      sh("#{QT_LD} #{UNIT_TEST_BIN}")
    end
  end

  desc "Run all functional tests"
  task :functionals => :build do
    if ENV['ONLY']
      sh("#{QT_LD} #{FUNCTIONAL_TEST_BIN} --gtest_filter=#{ENV['ONLY']}")
    else
      sh("#{QT_LD} #{FUNCTIONAL_TEST_BIN}")
    end
  end
  
  desc "Gather test coverage"
  task :coverage do
    ENV['COVERAGE'] = 'true'
    sh "drake -j 24 test:build"
    sh "rake test:run"
    sh "lcov --directory . -b . --capture --output-file test/coverage/info"
    sh "lcov -r test/coverage/info /usr/include/\\* -o test/coverage/info"
    sh "lcov -r test/coverage/info test/\\* -o test/coverage/info"
    sh "lcov -r test/coverage/info gtest/\\* -o test/coverage/info"
    sh "lcov -r test/coverage/info gmock/\\* -o test/coverage/info"
    sh "lcov -r test/coverage/info \\*.moc -o test/coverage/info"
    sh "lcov -r test/coverage/info \\*.uic -o test/coverage/info"
    sh "genhtml test/coverage/info -o test/coverage"
  end
end

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

desc "Run all tests"
task :test => ['test:build', 'test:run']

desc "Outputs test and code lines"
task :stats do
  test_lines = `find test | grep '\\.cpp\\|\\.h$' | xargs cat 2>/dev/null | wc -l`
  code_lines = `find src include | grep '\\.cpp\\|\\.h$' | xargs cat 2>/dev/null | wc -l`
  
  puts "Test lines: #{test_lines}"
  puts "Code lines: #{code_lines}"
  puts "Ratio: #{"%2.2f" % (test_lines.to_f / code_lines.to_f)}"
end