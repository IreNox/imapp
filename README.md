# I'm App
============

A Framework to create your own Immediate Mode UI Application with just a few lines of code.

## Features

- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear) integration
- Android support
- Windows support
- TODO: Linux support
- TODO: Image loading
- TODO: Resource packaging

## Example

```
void* ImAppProgramInitialize( ImAppParameters* pParameters )
{
	int* pProgramContext = malloc( sizeof( int ) );
	*pProgramContext = 0;
	return pProgramContext;
}

void ImAppProgramDoUi( ImAppContext* pImAppContext, void* pProgramContext )
{
	int* pCounter = (int*)pProgramContext;

	nk_layout_row_dynamic( pImAppContext->pNkContext, 0.0f, 1 );
	
	char buffer[ 32 ];
	sprintf( buffer, "Hello World %d", *pCounter );
	nk_label( pImAppContext->pNkContext, buffer, NK_TEXT_LEFT );
	
	*pCounter++;
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
	free( pProgramContext );
}
```

## Building

[tiki_build](https://github.com/IreNox/tiki_build) is used to generate project files. To create your own Project put tiki_build and premake5 in the root of your repro and write a `premake5.lua` file:

```
local project = Project:new( "my_imapp_program", ProjectTypes.WindowApplication );

project:add_files( 'src/*.c' )

project:add_external( "https://github.com/IreNox/imapp.git" )

finalize_default_solution( project )
```

To generate Visual Studio 2017 project files just execute: `premake5 --systemscript=tiki_build.lua --to=build vs2017`

## Used Libraries

- [Nuklear](https://github.com/Immediate-Mode-UI/Nuklear)
- [SDL](https://www.libsdl.org/)
- [tiki_build](https://github.com/IreNox/tiki_build)
