-- https://github.com/richgel999/miniz

local repo_name = "richgel999/miniz"
if tiki.external.version == "latest" then
	local response, result_code = http.get( "https://api.github.com/repos/" .. repo_name .. "/releases/latest" )
	local response_json =  json.decode( response )

	tiki.external.version = response_json.tag_name
end

-- url example: https://github.com/richgel999/miniz/releases/download/3.0.2/miniz-3.0.2.zip

local file_name = "miniz-" .. tiki.external.version .. ".zip"
local download_path = path.join( tiki.external.export_path, file_name )

if not os.isfile( download_path ) then
	local download_url = "https://github.com/" .. repo_name .. "/releases/download/" .. tiki.external.version .. "/" .. file_name

	print( "Download: " .. download_url )
	local result_str, result_code = http.download( download_url, download_path )
	if result_code ~= 200 then
		os.remove( download_path )
		throw( "download of '" .. download_url .. "' failed with error " .. result_code .. ": " .. result_str )
	end
	
	if not zip.extract( download_path, tiki.external.export_path ) then
		os.remove( download_path )
		throw( "Failed to extract " .. download_path )
	end
end

local miniz_module = module
local sdl_project = nil
if tiki.use_lib then
	sdl_project = Project:new( "miniz", ProjectTypes.StaticLibrary )
	sdk_module = sdl_project.module
end

miniz_module.module_type = ModuleTypes.FilesModule

miniz_module:add_include_dir( "." )

miniz_module:set_define( "MINIZ_NO_STDIO" );

miniz_module:add_files( "*.h" )
miniz_module:add_files( "*.c" )
