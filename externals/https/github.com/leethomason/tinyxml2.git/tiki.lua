-- https/github.com/leethomason/tinyxml2.git

local tinyxml_module = module
local tinyxml_project = nil
if tiki.use_lib then
	tinyxml_project = Project:new( "tinyxml2", ProjectTypes.StaticLibrary )
	tinyxml_module = tinyxml_project.module
end

tinyxml_module.module_type = ModuleTypes.FilesModule

tinyxml_module:add_files( "tinyxml2.h" )
tinyxml_module:add_files( "tinyxml2.cpp" )

tinyxml_module:add_include_dir( "." )

if tiki.use_lib then
	tinyxml_module.import_func = function( project, solution )
		project:add_project_dependency( tinyxml_project )
		solution:add_project( tinyxml_project )
	end
end