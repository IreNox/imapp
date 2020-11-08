# I'm App
============

A Framework to create your own Immediate Mode UI Application with just a few lines of code.

## Features

- TODO

## Example

```
void* ImAppProgramInitialize( ImAppParameters* pParameters )
{
	pParameters->pWindowTitle = "MyApp";
	
	void* pProgramContext = malloc( 1 );
	return pProgramContext;
}

void ImAppProgramDoUi( ImAppContext* pImAppContext, void* pProgramContext )
{
	nk_layout_row_dynamic( pImAppContext->pNkContext, 0.0f, 1 );
	nk_label( pImAppContext->pNkContext, "Hello World", NK_TEXT_LEFT );
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
```

## Building

[tiki_build](https://github.com/IreNox/tiki_build) is used to generate project files. To create your own Project put tiki_build and premake5 in the root of your repro and write a `premake5.lua` file:

```
local project = Project:new(
	"my_imapp_program",
	{ "x86", "x64" },
	{ "Debug", "Release" },
	ProjectTypes.WindowApplication
);

project:add_files( 'src/*.c' )

project:add_external( "https://github.com/IreNox/imapp.git" )

finalize_default_solution( project )
```

To generate Visual Studio 2017 project files just execute: `premake5 --systemscript=tiki_build.lua --to=build vs2017`

## Used Libraries

- [SDL](https://www.libsdl.org/)
- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)
- [tiki_build](https://github.com/IreNox/tiki_build)
