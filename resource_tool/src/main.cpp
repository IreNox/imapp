#include "imapp/imapp.h"

#include "resource_tool.h"

#include "./../assets/resource_tool_pak.h"

void* imappProgramInitialize( ImappParameters* parameters, int argc, char* argv[] )
{
	parameters->tickIntervalMs				= 15;
	parameters->resPath						= "./../../../../assets";
	parameters->defaultWindow.title			= "I'm App Resource Tool";
	parameters->defaultWindow.width			= 1280;
	parameters->defaultWindow.height		= 720;
	parameters->defaultWindow.clearColor	= imuiColorCreate( 0xf7, 0xf7, 0xf7, 0xff );

	parameters->defaultResPakName			= "resource_tool_pak";
	//parameters->defaultResPakData.data	= imappResPakResource_Tool;
	//parameters->defaultResPakData.size	= sizeof( imappResPakResource_Tool );
	parameters->defaultThemeName			= "config";

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

void imappProgramDoDefaultWindowUi( ImappContext* imapp, void* programContext, ImappWindow* appWindow, ImuiWindow* uiWindow )
{
	imapp::ResourceTool* context = (imapp::ResourceTool*)programContext;

	//imappResPak* respak = imappResourceGetDefaultPak( imapp );
	//imappResPakActivateTheme( imapp, respak, "config", false );

	imui::toolbox::UiToolboxWindow uiWindowClass( uiWindow );
	context->doUi( imapp, appWindow, uiWindowClass );
}

void imappProgramShutdown( ImappContext* imapp, void* programContext )
{
	imapp::ResourceTool* context = (imapp::ResourceTool*)programContext;
	delete context;
}