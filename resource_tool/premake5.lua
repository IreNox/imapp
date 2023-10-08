-- resource_tool

local project = Project:new( ProjectTypes.WindowApplication )

project:add_files( 'src/*.h' )
project:add_files( 'src/*.cpp' )

project:add_external( "local://.." )
project:add_external( "local://../submodules/tiki_core" )
project:add_external( "https://github.com/leethomason/tinyxml2.git" )

finalize_default_solution( project )
