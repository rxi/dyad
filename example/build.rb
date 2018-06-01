#!/usr/local/bin/ruby

require 'yaml'
include Gem

def command_format(cmd, var)
  var.each_pair do |key, value|
    cmd = cmd.gsub! "{#{key.to_s}}", value
  end
  return cmd
end

Dir.chdir __dir__

data = File.read 'config.yml'
config = YAML.load data, symbolize_names: true

if ARGF.argv.count < 1
  puts "Usage: build.rb [file]"
  exit
end

program_ext = Platform.local.os == "mswin32" ? '.exe' : ''
program_name = ARGF.argv[0].gsub '.c', program_ext

unless Dir.exist? config[:bin]
  Dir.mkdir config[:bin]
end

source_files = Dir.entries(config[:source]).select { |e| e.end_with?('.c', '.C') }
source_files.map! {|e| "#{config[:source]}/#{e}"}
source_files << ARGF.argv[0]

libraries = config[:dlibs].select do |e|
  e.keys.include?(Platform.local.os.to_sym)
end

unless libraries.empty?
  common = config[:dlibs].select { |e| e.keys.include?(:common)}.first
  libraries << common
  libraries.map! { |e| e.values }.flatten!
end

command = command_format(
  "{compiler} {flags} {include} -o {out} {source} {libs} {argv}",
  {
    compiler: config[:compiler],
    flags:    config[:cflags].map \
              { |e| !e.start_with?('-') ? e.prepend('-') : e }.join(' '),
    include:  "-I#{config[:include]}",
    out:      "#{config[:bin]}/#{program_name}",
    source:   source_files.join(' '),
    libs:     libraries.map {|e| "-l#{e}"}.join(' '),
    argv:     ARGF.argv.count > 1 ? ARGF.argv[1..ARGF.argv.count].join(' ') : ""
  }
)

puts "Compiling #{program_name}..."
`#{command}`
puts "Done"
