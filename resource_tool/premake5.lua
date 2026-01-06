-- resource_tool

newoption {
	trigger     = "use_msdf",
	description = "Choose a if MSDF atlas",
	default     = "off",
	allowed = {
		{ "off",	"Disabled" },
		{ "on",		"Enabled" }
	}
}

add_module_include_path( "modules" );

local project = Project:new( ProjectTypes.WindowApplication )

project:add_files( 'src/*.h' )
project:add_files( 'src/*.cpp' )

project:add_dependency( "slow_compile" );

project:add_external( "local://.." )
project:add_external( "local://../submodules/tiki_core" )
project:add_external( "https://github.com/leethomason/tinyxml2.git" )
project:add_external( "https://github.com/FelixK15/k15_image_atlas.git" )

if _OPTIONS[ "use_msdf" ] == "on" then
	--project:add_external( "https://github.com/Chlumsky/msdfgen.git" )
	project:add_external( "https://github.com/Chlumsky/msdf-atlas-gen.git" )
	
	project:set_define( "IMAPP_RESOURCE_TOOL_MSDF", "TIKI_ON" )
else
	project:set_define( "IMAPP_RESOURCE_TOOL_MSDF", "TIKI_OFF" )
end

project:set_define( "_CRT_SECURE_NO_WARNINGS" );

finalize_default_solution( project )
