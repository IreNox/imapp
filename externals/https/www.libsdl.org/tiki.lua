-- https/www.libsdl.org

if tiki.external.version == "latest" then
	-- TODO: extract from releases page
	tiki.external.version = "2.0.12"
end

-- url example: http://www.libsdl.org/release/SDL2-2.0.12.zip

local version_name = "SDL2-" .. tiki.external.version
local file_name = version_name .. ".zip"
local download_path = path.join( tiki.external.export_path, file_name )

if not os.isfile( download_path ) then
	local download_url = "https://www.libsdl.org/release/" .. file_name

	print( "Download: " .. download_url )
	local result_str, result_code = http.download( download_url, download_path )
	if result_code ~= 200 then
		os.remove( download_path )
		throw( "SQLite download failed with error " .. result_code .. ": " .. result_str )
	end
	
	if not zip.extract( download_path, tiki.external.export_path ) then
		os.remove( download_path )
		throw( "Failed to extract SQLite" )
	end
end

local sdl_project = Project:new(
	"sdl",
	{ "x32", "x64" },
	{ "Debug", "Release" },
	ProjectTypes.StaticLibrary
)

sdl_project.module.module_type = ModuleTypes.FilesModule

sdl_project:add_include_dir( version_name .. "/include" )

sdl_project:add_files( version_name .. "/src/*.h" )
sdl_project:add_files( version_name .. "/src/*.c" )

sdl_modules = {
	atomic		= { header = false,	source = true,	platforms = {} },
	audio		= { header = true,	source = true,	platforms = {} },
	core		= { header = false,	source = false,	platforms = {} },
	cpuinfo		= { header = false,	source = true,	platforms = {} },
	events		= { header = true,	source = true,	platforms = {} },
	filesystem	= { header = false,	source = false,	platforms = {} },
	haptic		= { header = true,	source = true,	platforms = {} },
	joystick	= { header = true,	source = true,	platforms = {} },
	loadso		= { header = false,	source = false,	platforms = {} },
	main		= { header = false,	source = false,	platforms = {} },
	power		= { header = true,	source = true,	platforms = {} },
	render		= { header = true,	source = true,	platforms = {} },
	sensor		= { header = true,	source = true,	platforms = {} },
	stdlib		= { header = false,	source = true,	platforms = {} },
	thread		= { header = true,	source = true,	platforms = {} },
	timer		= { header = true,	source = true,	platforms = {} },
	video		= { header = true,	source = true,	platforms = {} }
}

sdl_modules[ "audio" ].platforms[ Platforms.Windows ]	= { wasapi	= { header = false,	source = true } }
sdl_modules[ "main" ].platforms[ Platforms.Windows ]	= { windows	= { header = false,	source = true } }

for module_name, module_data in pairs( sdl_modules ) do
	local module_path = version_name .. "/src/" .. module_name

	if module_data.header then
		sdl_project:add_files( module_path .. "/*.h" )
	end

	if module_data.source then
		sdl_project:add_files( module_path .. "/*.c" )
	end

	local module_platform = module_data.platforms[ tiki.platform ]
	if module_platform then
		for platform_name, platform_data in pairs( module_platform ) do
			local platform_path = module_path .. "/" .. platform_name

			if platform_data.header then
				sdl_project:add_files( platform_path .. "/*.h" )
			end

			if platform_data.source then
				sdl_project:add_files( platform_path .. "/*.c" )
			end
		end
	end
end

module:add_include_dir( version_name .. "/include" )

module.import_func = function( project, solution )
	throw( "test" )
	project:add_project_dependency( sdl_project )
	solution:add_project( sdl_project )
end
