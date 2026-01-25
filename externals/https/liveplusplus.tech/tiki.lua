-- https://liveplusplus.tech/

if tiki.target_platform ~= Platforms.Windows then
	throw( "Live++ is only supported on Windows" )
end

local version_name = "LPP_" .. string.gsub( tiki.external.version, "%.", "_" )
local file_name = version_name .. ".zip"
local download_path = path.join( tiki.external.export_path, file_name )

if not os.isfile( download_path ) then
	local download_url = "https://liveplusplus.tech/downloads/" .. file_name

	print( "Download: " .. download_url )
	local result_str, result_code = http.download( download_url, download_path )
	if result_code ~= 200 then
		os.remove( download_path )
		throw( "Download of '" .. download_url .. "' failed with error " .. result_code .. ": " .. result_str )
	end
	
	if not zip.extract( download_path, path.join( tiki.external.export_path, version_name ) ) then
		os.remove( download_path )
		throw( "Failed to extract " .. download_path )
	end
end

module:add_include_dir( version_name )

module:add_files( version_name .. "/LivePP/API/x64/*.h" )

module.import_func = function( project, solution )
	editandcontinue( "Off" )
	
	table.insert( project.buildoptions, "/Gm-" )
	table.insert( project.linkoptions, "/FUNCTIONPADMIN /OPT:NOREF /OPT:NOICF /DEBUG:FULL" )
end
