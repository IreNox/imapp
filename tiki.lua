-- imapp

module.module_type = ModuleTypes.UnityCModule

module:add_include_dir( "include" )

module:add_files( 'include/imapp/*.h' )
module:add_files( 'src/*.h' )
module:add_files( 'src/*.c' )

module:add_external( "https://www.libsdl.org@2.0.12" )
module:add_external( "https://github.com/Immediate-Mode-UI/Nuklear.git" )

if tiki.target_platform == Platforms.Windows then
	module:add_external( "https://glew.sourceforge.net/@2.1.0" )
	module:add_library_file( "opengl32" )
elseif tiki.target_platform == Platforms.Android then
	module:add_library_file( "m" )
	
	module.import_func = function( project, solution )
		kind( ProjectTypes.SharedLibrary )
		
		local package_project = Project:new( project.name .. "_package", ProjectTypes.Package )
		package_project.module.module_type = ModuleTypes.FilesModule
		
		package_project:add_files( "res/**" )
		package_project:add_files( "AndroidManifest.xml" )
		package_project:add_files( "build.xml" )
		package_project:add_files( "project.properties" )
		
		package_project.module.import_func = function()
			androidapplibname( project.name )
			links{ project.name }
		end
		
		solution:add_project( package_project )
	end 
end
