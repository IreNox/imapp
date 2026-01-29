-- samples/06_multi_window

local project = Project:new( ProjectTypes.WindowApplication )

project.module.module_type = ModuleTypes.FilesModule

project:add_files( 'src/*.c' )

project:add_external( "local://../.." )

finalize_default_solution( project )
