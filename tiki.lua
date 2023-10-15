-- imapp

local imapp_path = module.config.base_path

tiki.use_sdl = (tiki.target_platform == Platforms.Windows)
--tiki.use_lib = false

module:add_include_dir( "include" )

module:add_files( "include/imapp/*.h" )
module:add_files( "src/*.h" )
module:add_files( "src/*.c" )

module:add_external( "local://submodules/imui" )
module:add_external( "local://submodules/libspng" )

if tiki.use_sdl then
	module:set_define( "IMAPP_PLATFORM_SDL", "1" );

	module:add_external( "https://github.com/libsdl-org/SDL@2.28.3" )
end

if tiki.target_platform == Platforms.Windows then
	module:add_external( "https://github.com/nigels-com/glew@2.2.0" )

	module:add_library_file( "opengl32" )
	
	module.import_func = function( project, solution )
		project:set_flag( "MultiProcessorCompile" )
	end
elseif tiki.target_platform == Platforms.Android then
	module:add_library_file( "m" )
	module:add_library_file( "EGL" )
	module:add_library_file( "GLESv3" )

	module.import_func = function( project, solution )
		kind( ProjectTypes.SharedLibrary )
		
		local package_project = Project:new( project.name .. "_package", ProjectTypes.Package )
		package_project.module.module_type = ModuleTypes.FilesModule
		
		package_project:add_files( "assets/**", { optional = true } )
		package_project:add_files( "res/**", { optional = true } )
		package_project:add_files( "AndroidManifest.xml" )

		print( imapp_path .. "/android/build.xml" )
		print( imapp_path .. "/android/project.properties" )
		
		if os.isfile( "build.xml" ) then
			package_project:add_files( "build.xml" )
		else
			package_project:add_files( imapp_path .. "/android/build.xml" )
		end

		if os.isfile( "project.properties" ) then
			package_project:add_files( "project.properties" )
		else
			package_project:add_files( imapp_path .. "/android/project.properties" )
		end
		
		package_project.module.import_func = function()
			androidapplibname( project.name )
			links{ project.name }
		end
		
		solution:add_project( package_project )
	end 
end
