-- samples/hello_world

local project = Project:new( "imapp_hello_world", ProjectTypes.WindowApplication )

project:add_files( 'src/*.c' )

project:add_external( "local://../.." )

finalize_default_solution( project )
