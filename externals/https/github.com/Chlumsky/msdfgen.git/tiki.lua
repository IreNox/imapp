-- https://github.com/Chlumsky/msdfgen.git

local msdf_module = module
local msdf_project = nil
if tiki.use_lib then
	msdf_project = Project:new( "msdfgen", ProjectTypes.StaticLibrary )
	msdf_module = msdf_project.module
end

msdf_module.module_type = ModuleTypes.FilesModule

msdf_module:add_include_dir( "." )

msdf_module:add_files( "msdfgen.h" )
msdf_module:add_files( "core/*.h" )
msdf_module:add_files( "core/*.cpp" )
msdf_module:add_files( "ext/*.h" )
msdf_module:add_files( "ext/*.cpp" )

msdf_module:set_define( "MSDFGEN_PUBLIC", "" )

msdf_module:add_external( "https://github.com/freetype/freetype.git" )

if tiki.use_lib then
	msdf_module.import_func = function( project, solution )
		project:add_project_dependency( msdf_project )
		solution:add_project( msdf_project )
	end
end
