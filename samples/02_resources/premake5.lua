-- samples/01_hello_world

local project = Project:new( ProjectTypes.WindowApplication )

project:add_files( 'src/*.c' )

project:add_external( "local://../.." )

finalize_default_solution( project )
