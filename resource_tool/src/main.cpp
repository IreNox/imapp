#include "imapp/imapp.h"

#include "resource_tool.h"

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs		= 1;
	parameters->resourcePath		= "./../../../../assets";
	parameters->windowTitle			= "I'm App Resource Tool";
	parameters->windowWidth			= 1280;
	parameters->windowHeight		= 720;

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

	imui::UiSurface surfaceClass( surface );
	context->doUi( imapp, surfaceClass );
}

void ImAppProgramShutdown( ImAppContext* imapp, void* programContext )
{
	imapp::ResourceTool* context = (imapp::ResourceTool*)programContext;
	delete context;
}