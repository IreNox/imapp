-- resource_tool

add_module_include_path( "modules" );

local project = Project:new( ProjectTypes.WindowApplication )

project:add_files( 'src/*.h' )
project:add_files( 'src/*.cpp' )

project:add_dependency( "file_dialog" );

project:add_external( "local://.." )
project:add_external( "local://../submodules/tiki_core" )
project:add_external( "https://github.com/leethomason/tinyxml2.git" )

project:set_define( "_CRT_SECURE_NO_WARNINGS" );


finalize_default_solution( project )
