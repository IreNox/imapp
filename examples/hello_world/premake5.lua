-- examples/hello_world

local project = Project:new(
	"imapp_hello_world",
	{ "x32", "x64" },
	{ "Debug", "Release" },
	ProjectTypes.ConsoleApplication
);

project:add_files( 'src/*.cpp' )

project:add_external( "https://www.libsdl.org/@2.0.12" )

finalize_default_solution( project )
