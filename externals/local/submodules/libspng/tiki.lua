-- https://github.com/randy408/libspng.git

local lodepng_module = module
if tiki.use_lib then
	local lodepng_project = Project:new( "libspng", ProjectTypes.StaticLibrary )
	lodepng_module = lodepng_project.module

	module.import_func = function( project, solution )
		project:add_project_dependency( lodepng_project )	
		solution:add_project( lodepng_project )
	end
end

lodepng_module.module_type = ModuleTypes.FilesModule

lodepng_module:add_files( 'spng/*.h' )
lodepng_module:add_files( 'spng/*.c' )

lodepng_module:set_define( "SPNG_USE_MINIZ" )
lodepng_module:set_define( "SPNG_STATIC" )

module:add_external( "https://github.com/richgel999/miniz" )

module:add_include_dir( "." )
