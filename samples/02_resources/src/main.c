#include "imapp/imapp.h"

#include "imapp/../../src/imapp_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define IMUI_NO_SAMPLE_FRAMEWORK
#include "imui/../../samples/src/00_samples.h"
#include "imui/../../samples/src/04_widgets.c"

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	IMAPP_USE( argc );
	IMAPP_USE( argv );

	parameters->tickIntervalMs		= 15;
	parameters->windowTitle			= "I'm App - Resources";
	parameters->windowWidth			= 960;
	parameters->windowHeight		= 600;
	parameters->windowClearColor	= ImUiColorCreate( 0xaau, 0xaau, 0xaau, 0xffu );
	parameters->windowMode			= ImAppDefaultWindow_Resizable;
	parameters->defaultResPakName	= "02_resources";
	parameters->defaultThemeName	= "config";
	parameters->useWindowStyle		= true;
	parameters->resPath				= "./../../../../assets";

	return (void*)1;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow )
{
	IMAPP_USE( programContext );
	IMAPP_USE( appWindow );

	ImAppResPakActivateTheme( imapp, ImAppResourceGetDefaultPak( imapp ), "config" );
	ImUiToolboxSampleTick( uiWindow );
}

void ImAppProgramShutdown( ImAppContext* imapp, void* programContext )
{
	IMAPP_USE( imapp );
	IMAPP_USE( programContext );
}
