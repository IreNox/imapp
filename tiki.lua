-- imapp

local imapp_module = module

local use_sdl = (tiki.target_platform == Platforms.Windows)

local nuklear_module = Module:new( "nuklear" );
nuklear_module.module_type = ModuleTypes.UnityCModule
nuklear_module:add_files( 'src/nuklear/*.c')

module.module_type = ModuleTypes.UnityCModule

module:add_include_dir( "include" )

module:add_files( "include/imapp/*.h" )
module:add_files( "src/*.h" )
module:add_files( "src/*.c" )

module:add_files( 'src/android/*.c' )
module:add_files( 'src/windows/*.c' )
module:add_files( 'src/sdl/*.c' )

module:add_dependency( "nuklear" );

module:add_external( "https://github.com/Immediate-Mode-UI/Nuklear.git" )

if use_sdl then
	module:set_define( "IMAPP_PLATFORM_SDL", "2" );

	module:add_external( "https://www.libsdl.org@2.0.12" )
end

if tiki.target_platform == Platforms.Windows then
	module:add_external( "https://glew.sourceforge.net/@2.1.0" )

	module:add_library_file( "opengl32" )
elseif tiki.target_platform == Platforms.Android then
	module:add_library_file( "m" )
	module:add_library_file( "EGL" )
	module:add_library_file( "GLESv3" )

	module.import_func = function( project, solution )
		kind( ProjectTypes.SharedLibrary )
		
		local package_project = Project:new( project.name .. "_package", ProjectTypes.Package )
		package_project.module.module_type = ModuleTypes.FilesModule
		
		package_project:add_files( "res/**" )
		package_project:add_files( "AndroidManifest.xml" )

		print( imapp_module.config.base_path .. "/android/build.xml" )
		print( imapp_module.config.base_path .. "/android/project.properties" )
		
		if os.isfile( "build.xml" ) then
			package_project:add_files( "build.xml" )
		else
			package_project:add_files( imapp_module.config.base_path .. "/android/build.xml" )
		end

		if os.isfile( "project.properties" ) then
			package_project:add_files( "project.properties" )
		else
			package_project:add_files( imapp_module.config.base_path .. "/android/project.properties" )
		end
		
		package_project.module.import_func = function()
			androidapplibname( project.name )
			links{ project.name }
		end
		
		solution:add_project( package_project )
	end 
end
