#include "imapp/imapp.h"

#include "resource_tool.h"

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs		= 15;
	parameters->resPath				= "./../../../../assets";
	parameters->defaultResPak		= "resource_tool";
	parameters->windowTitle			= "I'm App Resource Tool";
	parameters->windowWidth			= 1280;
	parameters->windowHeight		= 720;
	parameters->windowClearColor	= ImUiColorCreate( 0xf7, 0xf7, 0xf7, 0xff );

	imapp::ResourceTool* context = new imapp::ResourceTool();

	if( argc > 1u )
	{
		context->load( argv[ 1u ] );
	}

	return context;
}

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImUiSurface* surface )
{
	imapp::ResourceTool* context = (imapp::ResourceTool*)programContext;

	ImAppResPak* respak = ImAppResourceGetDefaultPak( imapp );
	ImAppResPakActivateTheme( respak, "config" );

	imui::UiSurface surfaceClass( surface );
	context->doUi( imapp, surfaceClass );
}

void ImAppProgramShutdown( ImAppContext* imapp, void* programContext )
{
	imapp::ResourceTool* context = (imapp::ResourceTool*)programContext;
	delete context;
}