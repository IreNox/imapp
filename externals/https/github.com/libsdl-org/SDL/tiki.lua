-- https://github.com/libsdl-org/SDL

local repo_name = "libsdl-org/SDL"
if tiki.external.version == "latest" then
	local response, result_code = http.get( "https://api.github.com/repos/" .. repo_name .. "/releases/latest" )
	local response_json =  json.decode( response )

	tiki.external.version = response_json.name
end

-- url example: https://github.com/libsdl-org/SDL/releases/download/release-2.28.3/SDL2-2.28.3.zip

local release_name = "release-" .. tiki.external.version
local version_name = "SDL2-" .. tiki.external.version
local file_name = version_name .. ".zip"
local download_path = path.join( tiki.external.export_path, file_name )

if not os.isfile( download_path ) then
	local download_url = "https://github.com/" .. repo_name .. "/releases/download/" .. release_name .. "/" .. file_name

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

local sdl_module = module
local sdl_project = nil
if tiki.use_lib then
	sdl_project = Project:new( "SDL", ProjectTypes.StaticLibrary )
	sdk_module = sdl_project.module
end

sdl_module.module_type = ModuleTypes.FilesModule

sdl_module:add_include_dir( version_name .. "/include" )

sdl_module:add_files( version_name .. "/include/*.h" )
sdl_module:add_files( version_name .. "/src/*.h" )
sdl_module:add_files( version_name .. "/src/*.c" )

if tiki.target_platform == Platforms.Windows then
	sdl_module:add_library_file( "imm32" )
	sdl_module:add_library_file( "winmm" )
	sdl_module:add_library_file( "setupapi" )
	sdl_module:add_library_file( "version" )
elseif tiki.target_platform == Platforms.Android then
	sdl_module:set_define( "GL_GLEXT_PROTOTYPES" )

	sdl_module:add_library_file( "GLESv1_CM" )
	sdl_module:add_library_file( "GLESv3" )
	sdl_module:add_library_file( "OpenSLES" )
elseif tiki.target_platform == Platforms.Linux then
	sdl_module:set_define( "HAVE_STDIO_H" )
	sdl_module:set_define( "HAVE_DBUS_DBUS_H" )
	sdl_module:set_define( "HAVE_LINUX_INPUT_H" )

	--sdl_module:add_include_dir( "dbus-1.0" )
	--sdl_module:add_include_dir( "/usr/include/dbus-1.0" )
	--sdl_module:add_include_dir( "/nix/store/rj6yyr7yw2vm39plgsw5qpjx68lsg8vd-dbus-1.14.10-dev/include/dbus-1.0" )

	sdl_module:set_define( "SDL_TIMER_UNIX", "1" )
	sdl_module:set_define( "SDL_HAPTIC_LINUX", "1" )	
	sdl_module:set_define( "SDL_FILESYSTEM_UNIX", "1" )
	sdl_module:set_define( "SDL_LOADSO_DLOPEN", "1" )
	sdl_module:set_define( "SDL_VIDEO_OPENGL", "1" )
	sdl_module:set_define( "SDL_VIDEO_OPENGL_GLX", "1" )
	sdl_module:set_define( "SDL_VIDEO_RENDER_OGL", "1" )
	--sdl_module:set_define( "SDL_VIDEO_DRIVER_WAYLAND", "1" )

	sdl_module:set_define( "SDL_VIDEO_DRIVER_X11", "1" )
	sdl_module:set_define( "SDL_VIDEO_DRIVER_X11_SUPPORTS_GENERIC_EVENTS" )

	--sdl_module:add_files( version_name .. "/src/core/linux/SDL_fcitx.*", { exclude = true } )
end

sdl_modules = {
	atomic		= { header = false,	source = true,	platforms = {} },
	audio		= { header = true,	source = true,	platforms = {} },
	core		= { header = false,	source = false,	platforms = {} },
	cpuinfo		= { header = false,	source = true,	platforms = {} },
	dynapi		= { header = true,	source = true,	platforms = {} },
	events		= { header = true,	source = true,	platforms = {} },
	file		= { header = false,	source = true,	platforms = {} },
	filesystem	= { header = false,	source = false,	platforms = {} },
	haptic		= { header = true,	source = true,	platforms = {} },
	hidapi		= { header = true,	source = true,	platforms = {} },
	joystick	= { header = true,	source = true,	platforms = {} },
	libm		= { header = true,	source = true,	platforms = {} },
	loadso		= { header = false,	source = false,	platforms = {} },
	locale		= { header = true,	source = true,	platforms = {} },
	main		= { header = false,	source = false,	platforms = {} },
	misc		= { header = true,	source = true,	platforms = {} },
	power		= { header = true,	source = true,	platforms = {} },
	render		= { header = true,	source = true,	platforms = {} },
	sensor		= { header = true,	source = true,	platforms = {} },
	stdlib		= { header = true,	source = true,	platforms = {} },
	thread		= { header = true,	source = true,	platforms = {} },
	timer		= { header = true,	source = true,	platforms = {} },
	video		= { header = true,	source = true,	platforms = {} }
}

sdl_modules[ "audio" ].platforms[ Platforms.Windows ]		= { directsound = { header = true, source = true }, disk = { header = true, source = true }, dummy = { header = true, source = true }, wasapi = { header = false, source = true }, winmm = { header = true, source = true } }
sdl_modules[ "core" ].platforms[ Platforms.Windows ]		= { windows	= { header = true,	source = true } }
sdl_modules[ "filesystem" ].platforms[ Platforms.Windows ]	= { windows	= { header = false,	source = true } }
sdl_modules[ "haptic" ].platforms[ Platforms.Windows ]		= { windows	= { header = true,	source = true } }
sdl_modules[ "joystick" ].platforms[ Platforms.Windows ]	= { windows	= { header = true,	source = true }, hidapi = { header = true, source = true }, virtual = { header = true, source = true } }
sdl_modules[ "loadso" ].platforms[ Platforms.Windows ]		= { windows	= { header = false,	source = true } }
sdl_modules[ "locale" ].platforms[ Platforms.Windows ]		= { windows	= { header = false,	source = true } }
sdl_modules[ "main" ].platforms[ Platforms.Windows ]		= { windows	= { header = false,	source = true } }
sdl_modules[ "misc" ].platforms[ Platforms.Windows ]		= { windows	= { header = false,	source = true } }
sdl_modules[ "power" ].platforms[ Platforms.Windows ]		= { windows	= { header = false,	source = true } }
sdl_modules[ "render" ].platforms[ Platforms.Windows ]		= { direct3d = { header = true, source = true }, direct3d11 = { header = true, source = true }, direct3d12 = { header = true, source = true }, opengl = { header = true, source = true }, opengles2 = { header = true, source = true }, software = { header = true, source = true } }
sdl_modules[ "sensor" ].platforms[ Platforms.Windows ]		= { windows	= { header = true,	source = true } }
sdl_modules[ "thread" ].platforms[ Platforms.Windows ]		= { windows	= { header = true,	source = true } }
sdl_modules[ "timer" ].platforms[ Platforms.Windows ]		= { windows	= { header = false,	source = true } }
sdl_modules[ "video" ].platforms[ Platforms.Windows ]		= { windows	= { header = true,	source = true }, dummy = { header = true, source = true }, yuv2rgb = { header = true, source = true } }

sdl_modules[ "audio" ].platforms[ Platforms.Android ]		= { android = { header = true,	source = true }, dummy = { header = true, source = true }, openslES = { header = true, source = true } }
sdl_modules[ "core" ].platforms[ Platforms.Android ]		= { android	= { header = true,	source = true } }
sdl_modules[ "filesystem" ].platforms[ Platforms.Android ]	= { android	= { header = false,	source = true } }
sdl_modules[ "haptic" ].platforms[ Platforms.Android ]		= { android	= { header = false,	source = true } }
sdl_modules[ "joystick" ].platforms[ Platforms.Android ]	= { android	= { header = true,	source = true }, hidapi = { header = true, source = true }, virtual = { header = true, source = true } }
sdl_modules[ "loadso" ].platforms[ Platforms.Android ]		= { dlopen	= { header = false,	source = true } }
sdl_modules[ "main" ].platforms[ Platforms.Android ]		= { android	= { header = false,	source = true } }
sdl_modules[ "power" ].platforms[ Platforms.Android ]		= { android	= { header = false,	source = true } }
sdl_modules[ "render" ].platforms[ Platforms.Android ]		= { opengles = { header = true, source = true }, opengles2 = { header = true, source = true }, software = { header = true, source = true } }
sdl_modules[ "sensor" ].platforms[ Platforms.Android ]		= { android	= { header = true,	source = true } }
sdl_modules[ "thread" ].platforms[ Platforms.Android ]		= { pthread	= { header = true,	source = true }, generic = { header = true, source = true } }
sdl_modules[ "timer" ].platforms[ Platforms.Android ]		= { unix	= { header = false,	source = true } }
sdl_modules[ "video" ].platforms[ Platforms.Android ]		= { android	= { header = true,	source = true }, dummy = { header = true, source = true }, yuv2rgb = { header = true, source = true } }

sdl_modules[ "audio" ].platforms[ Platforms.Linux ]			= { pulseaudio = { header = true, source = true }, dummy = { header = true, source = true }, openslES = { header = true, source = true } }
sdl_modules[ "core" ].platforms[ Platforms.Linux ]			= { linux	= { header = true,	source = true }, unix = { header = true, source = true } }
sdl_modules[ "filesystem" ].platforms[ Platforms.Linux ]	= { unix	= { header = false,	source = true } }
sdl_modules[ "haptic" ].platforms[ Platforms.Linux ]		= { linux	= { header = false,	source = true } }
sdl_modules[ "joystick" ].platforms[ Platforms.Linux ]		= { linux	= { header = true,	source = true }, dummy = { header = false, source = true }, hidapi = { header = true, source = true }, virtual = { header = true, source = true } }
sdl_modules[ "loadso" ].platforms[ Platforms.Linux ]		= { dlopen	= { header = false,	source = true } }
sdl_modules[ "locale" ].platforms[ Platforms.Linux ]		= { unix	= { header = false,	source = true } }
--sdl_modules[ "main" ].platforms[ Platforms.Linux ]			= { android	= { header = false,	source = true } }
sdl_modules[ "misc" ].platforms[ Platforms.Linux ]			= { unix	= { header = false,	source = true } }
sdl_modules[ "power" ].platforms[ Platforms.Linux ]			= { linux	= { header = false,	source = true } }
sdl_modules[ "render" ].platforms[ Platforms.Linux ]		= { opengl	= { header = true, source = true }, software = { header = true, source = true } }
sdl_modules[ "sensor" ].platforms[ Platforms.Linux ]		= { dummy	= { header = true,	source = true } }
sdl_modules[ "thread" ].platforms[ Platforms.Linux ]		= { pthread	= { header = true,	source = true } }
sdl_modules[ "timer" ].platforms[ Platforms.Linux ]			= { unix	= { header = false,	source = true } }
sdl_modules[ "video" ].platforms[ Platforms.Linux ]			= { wayland	= { header = true,	source = true }, x11 = { header = true, source = true }, dummy = { header = true, source = true }, yuv2rgb = { header = true, source = true } }

for module_name, module_data in pairs( sdl_modules ) do
	local module_path = version_name .. "/src/" .. module_name

	if module_data.header then
		sdl_module:add_files( module_path .. "/*.h" )
	end

	if module_data.source then
		sdl_module:add_files( module_path .. "/*.c" )
	end

	local module_platform = module_data.platforms[ tiki.target_platform ]
	if module_platform then
		for platform_name, platform_data in pairs( module_platform ) do
			local platform_path = module_path .. "/" .. platform_name

			if platform_data.header then
				sdl_module:add_files( platform_path .. "/*.h" )
			end

			if platform_data.source then
				sdl_module:add_files( platform_path .. "/*.c" )
			end

			if platform_data.source_cpp then
				sdl_module:add_files( platform_path .. "/*.cpp" )
			end
		end
	end
end

if tiki.target_platform == Platforms.Windows then
	sdl_module:add_files( version_name .. "/src/thread/generic/SDL_syscond.c" )
end

module:add_include_dir( version_name .. "/include" )

module.import_func = function( project, solution )
	if tiki.use_lib then
		project:add_project_dependency( sdl_project )
	end
	
	if tiki.target_platform == Platforms.Linux then
		table.insert( project.buildoptions, "`pkg-config --cflags dbus-1 --cflags x11 --cflags xext --cflags gl`" )
		table.insert( project.linkoptions, "`pkg-config --libs dbus-1 --libs x11 --libs xext --libs gl`" )
	end
	
	if tiki.use_lib then
		solution:add_project( sdl_project )
	end
end
