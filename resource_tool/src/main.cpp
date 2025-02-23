#include "imapp/imapp.h"

#include "resource_tool.h"

#include "./../assets/resource_tool_pak.h"

void* ImAppProgramInitialize( ImAppParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs		= 15;
	parameters->resPath				= "./../../../../assets";
	parameters->windowTitle			= "I'm App Resource Tool";
	parameters->windowWidth			= 1280;
	parameters->windowHeight		= 720;
	parameters->windowClearColor	= ImUiColorCreate( 0xf7, 0xf7, 0xf7, 0xff );

	parameters->defaultResPakName		= "resource_tool_pak";
	parameters->defaultResPakData.data	= ImAppResPakResource_Tool;
	parameters->defaultResPakData.size	= sizeof( ImAppResPakResource_Tool );

	imapp::ResourceTool* context = new imapp::ResourceTool();

	if( !context->handleArgs( argc, argv, parameters->shutdownAfterInit ) )
	{
		parameters->shutdownAfterInit	= true;
		parameters->exitCode			= 1;
	}

	if( parameters->shutdownAfterInit )
	{
		delete context;
		return nullptr;
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