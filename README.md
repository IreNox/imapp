# I'm App

A Framework to create your own Immediate Mode UI Application with just a few lines of code.

## Features

- [I'm UI](https://github.com/IreNox/imui) integration
- Android support(without Java)
- Windows support
- Resource system to load image, fonts and themes
- Resource Tool to configure resource packages
- TODO: Linux support

## Example

```
void* ImAppProgramInitialize( ImAppParameters* parameters )
{
	int* programContext = malloc( sizeof( int ) );
	*programContext = 0;
	return programContext;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiSurface* surface )
{
	int* counter = (int*)programContext;

	ImUiWindow* window = ImUiWindowBegin( surface, IMUI_STR( "main" ), ImUiRectCenterSize( 0.0f, 0.0f, ImUiSurfaceGetSize( surface ) ), 1u );

	char buffer[ 32 ];
	sprintf( buffer, "Hello World %d", *counter );
	ImUiToolboxLabel( window, ImUiStringViewCreate( buffer ) );
	
	ImUiWindowEnd( window );
	
	*counter++;
}

void ImAppProgramShutdown( ImAppContext* imapp, void* programContext )
{
	free( programContext );
}
```

## Building

[tiki_build](https://github.com/IreNox/tiki_build) is used to generate project files. To create your own Project put premake_tb executable in the root of your repro and write a `premake5.lua` file:

```
local project = Project:new( "my_imapp_program", ProjectTypes.WindowApplication );

project:add_files( 'src/*.c' )

project:add_external( "https://github.com/IreNox/imapp.git" )

finalize_default_solution( project )
```

To generate Visual Studio 2017 project files just execute: `premake_tb --to=build vs2017`

## Used Libraries

- [I'm UI](https://github.com/IreNox/imui)
- [Portable File Dialogs](https://github.com/samhocevar/portable-file-dialogs)
- [SDL](https://www.libsdl.org/)
- [k15_image_atlas](https://github.com/FelixK15/k15_image_atlas)
- [libspng](https://github.com/randy408/libspng)
- [miniz](https://github.com/richgel999/miniz)
- [tiki_core](https://github.com/IreNox/tiki_core)
- [tiki_build](https://github.com/IreNox/tiki_build)
