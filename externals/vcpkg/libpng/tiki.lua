-- vcpkg://libpng

module:add_library_file( "libpng16d.lib", "Debug" )
module:add_library_file( "zlibd.lib", "Debug" )

module:add_library_file( "libpng16.lib", "Release" )
module:add_library_file( "zlib.lib", "Release" )
