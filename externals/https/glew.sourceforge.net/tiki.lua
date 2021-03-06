-- https/glew.sourceforge.net/

if tiki.external.version == "latest" then
	-- TODO: extract from releases page
	tiki.external.version = "2.1.0"
end

-- url example: https://downloads.sourceforge.net/project/glew/glew/2.1.0/glew-2.1.0.zip

local version_name = "glew-" .. tiki.external.version
local file_name = version_name .. ".zip"
local download_path = path.join( tiki.external.export_path, file_name )

if not os.isfile( download_path ) then
	local download_url = "https://downloads.sourceforge.net/project/glew/glew/" .. tiki.external.version .. "/" .. file_name

	print( "Download: " .. download_url )
	local result_str, result_code = http.download( download_url, download_path )
	if result_code ~= 200 then
		os.remove( download_path )
		throw( "download of '" .. download_url .. "' failed with error " .. result_code .. ": " .. result_str )
	end
	
	if not zip.extract( download_path, tiki.external.export_path ) then
		os.remove( download_path )
		throw( "Failed to extract glew" )
	end
end

local glew_module = module
if tiki.use_lib then
	local glew_project = Project:new( "GLEW", ProjectTypes.StaticLibrary )
	glew_module = glew_project.module

	module.import_func = function( project, solution )
		project:add_project_dependency( glew_project )	
		solution:add_project( glew_project )
	end
end

glew_module.module_type = ModuleTypes.FilesModule

glew_module:add_include_dir( version_name .. "/include" )

glew_module:add_files( version_name .. "/include/GL/*.h" )
glew_module:add_files( version_name .. "/src/glew.c" )

glew_module:set_define( "GLEW_STATIC" )

module:add_include_dir( version_name .. "/include" )

module:set_define( "GLEW_STATIC" )
