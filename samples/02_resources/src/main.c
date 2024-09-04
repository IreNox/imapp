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
	parameters->tickIntervalMs		= 15;
	parameters->windowTitle			= "I'm App - Resources";
	parameters->windowWidth			= 960;
	parameters->windowHeight		= 600;
	parameters->windowClearColor	= ImUiColorCreate( 0xaau, 0xaau, 0xaau, 0xffu );
	parameters->windowMode			= ImAppDefaultWindow_Resizable;
	parameters->defaultResPak		= "02_resources";
	parameters->resPath				= "./../../../../assets";

	return (void*)1;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiSurface* surface )
{
	ImAppResPakActivateTheme( ImAppResourceGetDefaultPak( imapp ), "config" );
	ImUiToolboxSampleTick( surface );
}

void ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext )
{
}
