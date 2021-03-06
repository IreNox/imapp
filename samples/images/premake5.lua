-- samples/images

local project = Project:new( "imapp_images", ProjectTypes.WindowApplication )

project:add_files( 'src/*.c' )

project:add_external( "local://../.." )

project:add_post_build_step( "copy_files", { pattern = "assets/**" } )

finalize_default_solution( project )
