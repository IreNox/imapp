-- https://github.com/FelixK15/k15_image_atlas.git

local kia_module = module
local kia_project = nil
if tiki.use_lib then
	kia_project = Project:new( "k15_image_atlas", ProjectTypes.StaticLibrary )
	kia_module = kia_project.module
end

kia_module.module_type = ModuleTypes.FilesModule

kia_module:add_include_dir( "." )

kia_module:add_files( "*.h" )

if tiki.use_lib then
	kia_module.import_func = function( project, solution )
		project:add_project_dependency( kia_project )
		solution:add_project( kia_project )
	end
end
