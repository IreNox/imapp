-- https/github.com/lvandeve/lodepng.git

local lodepng_module = module
if tiki.use_lib then
	local lodepng_project = Project:new( "LodePNG", ProjectTypes.StaticLibrary )
	lodepng_module = lodepng_project.module

	module.import_func = function( project, solution )
		project:add_project_dependency( lodepng_project )	
		solution:add_project( lodepng_project )
	end
end

-- kind of a hack to compile it as C and not C++
lodepng_module.module_type = ModuleTypes.UnityCModule

lodepng_module:add_files( 'lodepng.h' )
lodepng_module:add_files( 'lodepng.cpp' )

lodepng_module:set_define( "LODEPNG_NO_COMPILE_DISK" )
lodepng_module:set_define( "LODEPNG_NO_COMPILE_ENCODER" )
--lodepng_module:set_define( "LODEPNG_NO_COMPILE_ALLOCATORS" )
lodepng_module:set_define( "LODEPNG_NO_COMPILE_CPP" )

if tiki.use_lib then
	module:set_define( "LODEPNG_NO_COMPILE_DISK" )
	module:set_define( "LODEPNG_NO_COMPILE_ENCODER" )
	--module:set_define( "LODEPNG_NO_COMPILE_ALLOCATORS" )
	module:set_define( "LODEPNG_NO_COMPILE_CPP" )
end

module:add_include_dir( "." )
