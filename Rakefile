require 'rake/clean'

QT_INCLUDE_DIRS = ['/Library/Frameworks/QtCore.framework/Headers/', '/Library/Frameworks/QtGui.framework/Headers/']
QT_FRAMEWORKS = ['QtCore', 'QtGui']

INCLUDE_DIR = File.expand_path(File.dirname(__FILE__) + '/include')
SRC_DIR = File.expand_path(File.dirname(__FILE__) + '/src')
GTEST_DIR = File.expand_path(File.dirname(__FILE__) + '/gtest')
TEST_DIR = File.expand_path(File.dirname(__FILE__) + '/test')
EXAMPLES_DIR = File.expand_path(File.dirname(__FILE__) + '/examples')

SRC = FileList["#{SRC_DIR}/**/*.cpp"]
TEST = FileList["#{TEST_DIR}/**/*.cpp"]
GTEST = FileList["#{GTEST_DIR}/**/*.cpp"]
EXAMPLES_SRC = FileList["#{EXAMPLES_DIR}/**/*.cpp"]

SRC_OBJ = SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }
TEST_OBJ = TEST.collect { |fn| fn.gsub(/\.cpp/, '.o') }
GTEST_OBJ = GTEST.collect { |fn| fn.gsub(/\.cpp/, '.o') }
EXAMPLES_OBJ = EXAMPLES_SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }

TEST_BIN = "#{TEST_DIR}/tests.run"
EXAMPLES = FileList["#{EXAMPLES_DIR}/*"]
EXAMPLES_BIN = EXAMPLES.collect { |ex| "#{ex}/#{File.basename(ex)}" }

INCLUDE_DIRS = [INCLUDE_DIR, GTEST_DIR, QT_INCLUDE_DIRS, '.'].flatten
FRAMEWORKS = [QT_FRAMEWORKS].flatten

C_FLAGS = "-O3 -funroll-loops -W -Wall"
LD_FLAGS = "#{FRAMEWORKS.collect { |l| "-framework #{l}" }.join(' ')}"

CLEAN.include(SRC_OBJ, TEST_OBJ, GTEST_OBJ, EXAMPLES_OBJ, TEST_BIN, EXAMPLES_BIN)

task :default => [:examples, :test]

def header_dependencies(file)
  file_path = nil
  if File.exist?(file)
    file_path = file
  else
    INCLUDE_DIRS.each do |dir|
      path = File.join(dir, file)
      file_path = path and break if File.exist?(path)
    end
  end
  
  if file_path
    headers = File.read(file_path).split("\n").grep(/#include \"/).collect { |inc| inc =~ /^#include \"(.*?)\"$/; $1 }
    [file_path, headers.collect { |header| header_dependencies(header) }].flatten
  else
    [file]
  end
end

def dependencies(objfile)
  source_file = objfile.gsub(/\.o/, '.cpp')
  header_dependencies(source_file).uniq
end

rule '.o' => lambda { |objfile| dependencies(objfile) } do |t|
  sh %{g++ #{INCLUDE_DIRS.collect { |dir| "-I#{dir}" }.join(' ')} #{C_FLAGS} -o #{t.name} -c #{t.source}}
end

file TEST_BIN => [SRC_OBJ, TEST_OBJ, GTEST_OBJ].flatten do
  sh %{g++ -Os -o #{TEST_BIN} #{[SRC_OBJ, TEST_OBJ, GTEST_OBJ].flatten.join(' ')} #{LD_FLAGS}}
end

EXAMPLES.each do |example|
  src = FileList["#{example}/**/*.cpp"]
  obj = src.collect { |fn| fn.gsub(/\.cpp/, '.o') }
  output = "#{example}/#{File.basename(example)}"
  
  file output => [obj, SRC_OBJ].flatten do
    sh %{g++ -Os -o #{output} #{[SRC_OBJ, obj].flatten.join(' ')} #{LD_FLAGS}}
  end
end

task :examples => EXAMPLES_BIN

namespace :test do
  task :build => TEST_BIN
  
  desc "Run all unit tests"
  task :run => :build do
    if ENV['ONLY']
      sh("#{TEST_BIN} --gtest_filter=#{ENV['ONLY']}")
    else
      sh(TEST_BIN)
    end
  end
end

desc "Run all tests"
task :test => ['test:build', 'test:run']
