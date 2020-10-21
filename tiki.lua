-- imapp

module.module_type = ModuleTypes.UnityCModule

module:add_include_dir( "include" )

module:add_files( 'include/imapp/*.h' )
module:add_files( 'src/*.h' )
module:add_files( 'src/*.c' )

module:add_external( "https://www.libsdl.org@2.0.12" )
module:add_external( "https://github.com/Immediate-Mode-UI/Nuklear.git" )
module:add_external( "https://github.com/nigels-com/glew.git@glew-2.1.0" )