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
	//parameters->defaultResPakData.data	= ImAppResPakResource_Tool;
	//parameters->defaultResPakData.size	= sizeof( ImAppResPakResource_Tool );
	parameters->defaultThemeName		= "config";
	parameters->useWindowStyle			= false;

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

void ImAppProgramDoDefaultWindowUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow )
{
	imapp::ResourceTool* context = (imapp::ResourceTool*)programContext;

	//ImAppResPak* respak = ImAppResourceGetDefaultPak( imapp );
	//ImAppResPakActivateTheme( imapp, respak, "config", false );

	imui::toolbox::UiToolboxWindow uiWindowClass( uiWindow );
	context->doUi( imapp, appWindow, uiWindowClass );
}

void ImAppProgramShutdown( ImAppContext* imapp, void* programContext )
{
	imapp::ResourceTool* context = (imapp::ResourceTool*)programContext;
	delete context;
}