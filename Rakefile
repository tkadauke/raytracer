require 'rake/clean'

INCLUDE_DIR = File.expand_path(File.dirname(__FILE__) + '/include')
SRC_DIR = File.expand_path(File.dirname(__FILE__) + '/src')
GTEST_DIR = File.expand_path(File.dirname(__FILE__) + '/gtest')
TEST_DIR = File.expand_path(File.dirname(__FILE__) + '/test')

SRC = FileList["#{SRC_DIR}/**/*.cpp"]
TEST = FileList["#{TEST_DIR}/**/*.cpp"]
GTEST = FileList["#{GTEST_DIR}/**/*.cpp"]

SRC_OBJ = SRC.collect { |fn| fn.gsub(/\.cpp/, '.o') }
TEST_OBJ = TEST.collect { |fn| fn.gsub(/\.cpp/, '.o') }
GTEST_OBJ = GTEST.collect { |fn| fn.gsub(/\.cpp/, '.o') }

TEST_BIN = "#{TEST_DIR}/tests.run"

INCLUDE_DIRS = [INCLUDE_DIR, GTEST_DIR, '.']

CLEAN.include(SRC_OBJ, TEST_OBJ, GTEST_OBJ, TEST_BIN)

task :default => [:build, :test]

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
  sh %{g++ #{INCLUDE_DIRS.collect { |dir| "-I#{dir}" }.join(' ')} -O3 -o #{t.name} -c #{t.source}}
end

file TEST_BIN => [SRC_OBJ, TEST_OBJ, GTEST_OBJ].flatten do
  sh %{g++ -Os -o #{TEST_BIN} #{[SRC_OBJ, TEST_OBJ, GTEST_OBJ].flatten.join(' ')}}
end

namespace :test do
  desc "Run all unit tests"
  task :run => TEST_BIN do
    sh(TEST_BIN)
  end
end

desc "Run all tests"
task :test => ['test:run']
