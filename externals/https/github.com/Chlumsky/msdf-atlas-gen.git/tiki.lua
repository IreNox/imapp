-- https://github.com/Chlumsky/msdf-atlas-gen.git

local msdf_module = module
local msdf_project = nil
if tiki.use_lib then
	msdf_project = Project:new( "msdf-adlas-gen", ProjectTypes.StaticLibrary )
	msdf_module = msdf_project.module
end

msdf_module.module_type = ModuleTypes.FilesModule

msdf_module:add_include_dir( "artery-font-format" )
msdf_module:add_include_dir( "msdf-atlas-gen" )
msdf_module:add_include_dir( "msdfgen" )

msdf_module:add_files( "artery-font-format/artery-font/*.h" )
msdf_module:add_files( "artery-font-format/artery-font/*.hpp" )

msdf_module:add_files( "msdf-atlas-gen/*.h" )
msdf_module:add_files( "msdf-atlas-gen/*.cpp" )

msdf_module:add_files( "msdfgen/msdfgen.h" )
msdf_module:add_files( "msdfgen/core/*.h" )
msdf_module:add_files( "msdfgen/core/*.cpp" )
msdf_module:add_files( "msdfgen/ext/*.h" )
msdf_module:add_files( "msdfgen/ext/*.cpp" )

msdf_module:set_define( "MSDFGEN_PUBLIC" )
msdf_module:set_define( "MSDFGEN_USE_LIBPNG" )
msdf_module:set_define( "MSDFGEN_USE_SKIA" )

msdf_module:add_external( "https://github.com/freetype/freetype.git" )

msdf_module:add_external( "vcpkg://skia" )
msdf_module:add_external( "vcpkg://libpng" )

if tiki.use_lib then
	msdf_module.import_func = function( project, solution )
		project:add_project_dependency( msdf_project )
		solution:add_project( msdf_project )
	end
end
