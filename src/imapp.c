#include "imapp/imapp.h"

#include "imapp_event_queue.h"
#include "imapp_internal.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"
#include "imapp_res_sys.h"

#if IMAPP_ENABLED(  IMAPP_PLATFORM_WEB )
#	include <emscripten.h>
#endif

#include <stdio.h>
#include <string.h>

static void		ImAppFillDefaultParameters( ImAppParameters* parameters );
static bool		ImAppInitialize( ImAppInternal* imapp, const ImAppParameters* parameters );
static void		ImAppCleanup( ImAppInternal* imapp );
static void		ImAppHandleWindowEvents( ImAppInternal* imapp, ImAppWindow* window );
static void		ImAppTick( void* arg );
static void		ImAppTickWindow( ImAppWindow* window, void* arg );

int ImAppMain( ImAppPlatform* platform, int argc, char* argv[] )
{
	ImAppInternal* imapp = NULL;
	{
		void* programContext = NULL;
		ImAppParameters parameters;
		ImAppFillDefaultParameters( &parameters );

		programContext = ImAppProgramInitialize( &parameters, argc, argv );
		if( parameters.shutdownAfterInit )
		{
			return parameters.exitCode;
		}
		else if( programContext == NULL )
		{
			ImAppPlatformShowError( platform, "Failed to initialize Program." );
			return 1;
		}

		ImUiAllocator allocator;
		ImUiMemoryAllocatorPrepare( &allocator, &parameters.allocator );

		imapp = IMUI_MEMORY_NEW_ZERO( &allocator, ImAppInternal );
		if( !imapp )
		{
			ImAppPlatformShowError( platform, "Failed to create ImApp." );
			return 1;
		}

		ImUiMemoryAllocatorFinalize( &imapp->allocator, &allocator );

		imapp->running			= true;
		imapp->platform			= platform;
		imapp->programContext	= programContext;

		if( !ImAppPlatformInitialize( platform, &imapp->allocator, parameters.resPath ) )
		{
			ImAppPlatformShowError( imapp->platform, "Failed to initialize Platform." );
			ImAppCleanup( imapp );
			return 1;
		}

		if( !ImAppInitialize( imapp, &parameters ) )
		{
			ImAppCleanup( imapp );
			return 1;
		}

		imapp->tickIntervalMs = parameters.tickIntervalMs;
	}

#if IMAPP_ENABLED(  IMAPP_PLATFORM_WEB )
	emscripten_set_main_loop_arg( ImAppTick, imapp, 0, 1 );
#else
	while( imapp->running )
	{
		ImAppTick( imapp );
	}
#endif

	ImAppCleanup( imapp );
	return 0;
}

static void ImAppTick( void* arg )
{
	ImAppInternal* imapp = (ImAppInternal*)arg;

	imapp->lastTickValue = ImAppPlatformTick( imapp->platform, imapp->lastTickValue, imapp->tickIntervalMs );

	ImAppResSysUpdate( imapp->ressys );
	ImAppPlatformWindowUpdate( imapp->window, ImAppTickWindow, imapp );

	//ImAppTickWindow( imapp->window, arg );
}

static void ImAppTickWindow( ImAppWindow* window, void* arg )
{
	ImAppInternal* imapp = (ImAppInternal*)arg;

	ImAppHandleWindowEvents( imapp, window );

	if( ImUiInputGetShortcut( imapp->context.imui ) == ImUiInputShortcut_Paste )
	{
		ImAppPlatformGetClipboardText( imapp->platform, imapp->context.imui );
	}

	ImAppPlatformWindowGetViewRect( imapp->window, &imapp->context.x, &imapp->context.y, &imapp->context.width, &imapp->context.height );

	{
		const ImUiSize size		= ImUiSizeCreate( (float)imapp->context.width, (float)imapp->context.height );
		ImUiFrame* frame		= ImUiBegin( imapp->context.imui, imapp->lastTickValue / 1000.0f );
		ImUiSurface* surface	= ImUiSurfaceBegin( frame, "default", size, ImAppPlatformWindowGetDpiScale( imapp->window ) );

		ImAppProgramDoDefaultWindowUi( &imapp->context, imapp->programContext, surface );

		ImUiSurfaceEnd( surface );

		ImAppRendererDraw( imapp->renderer, imapp->window, surface );

		ImUiEnd( frame );
	}

	const ImUiInputMouseCursor cursor = ImUiInputGetMouseCursor( imapp->context.imui );
	if( cursor != imapp->lastCursor )
	{
		ImAppPlatformSetMouseCursor( imapp->platform, cursor );
		imapp->lastCursor = cursor;
	}

	const char* copyText = ImUiInputGetCopyText( imapp->context.imui );
	if( copyText )
	{
		ImAppPlatformSetClipboardText( imapp->platform, copyText );
		ImUiInputSetCopyText( imapp->context.imui, NULL, 0u );
	}

	if( !ImAppPlatformWindowPresent( imapp->window ) )
	{
		if( !ImAppRendererRecreateResources( imapp->renderer ) )
		{
			ImAppQuit( &imapp->context, 2 );
			return;
		}

		if( !ImAppResSysRecreateEverything( imapp->ressys ) )
		{
			ImAppQuit( &imapp->context, 2 );
			return;
		}
	}
}

static void ImAppFillDefaultParameters( ImAppParameters* parameters )
{
	memset( parameters, 0, sizeof( *parameters ) );

	parameters->tickIntervalMs		= 0;
	parameters->resPath				= "./assets";

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
    parameters->defaultFontName		= "Roboto-Regular.ttf";
#elif IMAPP_ENABLED( IMAPP_PLATFORM_LINUX )
    parameters->defaultFontName		= "NotoSans[wdth,wght].ttf";
#else
    parameters->defaultFontName		= "Arial.ttf";
#endif
	parameters->defaultFontSize		= 16.0f;
	parameters->shutdownAfterInit	= false;
	parameters->exitCode			= 0;
#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID ) || IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	parameters->windowMode			= ImAppDefaultWindow_Fullscreen;
#else
	parameters->windowMode			= ImAppDefaultWindow_Resizable;
#endif
	parameters->windowTitle			= "I'm App";
	parameters->windowWidth			= 1280;
	parameters->windowHeight		= 720;
	parameters->windowClearColor	= ImUiColorCreate( 0x11u, 0x44u, 0xaau, 0xffu );

	static const ImUiInputShortcutConfig s_inputShortcuts[] =
	{
		{ ImUiInputShortcut_Home,					0u,								ImUiInputKey_Home },
		{ ImUiInputShortcut_End,					0u,								ImUiInputKey_End },
		{ ImUiInputShortcut_Undo,					ImUiInputModifier_LeftCtrl,		ImUiInputKey_Z },
		{ ImUiInputShortcut_Undo,					ImUiInputModifier_RightCtrl,	ImUiInputKey_Z },
		{ ImUiInputShortcut_Redo,					ImUiInputModifier_LeftCtrl,		ImUiInputKey_Y },
		{ ImUiInputShortcut_Redo,					ImUiInputModifier_RightCtrl,	ImUiInputKey_Y },
		{ ImUiInputShortcut_Cut,					ImUiInputModifier_LeftCtrl,		ImUiInputKey_X },
		{ ImUiInputShortcut_Cut,					ImUiInputModifier_RightCtrl,	ImUiInputKey_X },
		{ ImUiInputShortcut_Cut,					ImUiInputModifier_LeftShift,	ImUiInputKey_Delete },
		{ ImUiInputShortcut_Cut,					ImUiInputModifier_RightShift,	ImUiInputKey_Delete },
		{ ImUiInputShortcut_Copy,					ImUiInputModifier_LeftCtrl,		ImUiInputKey_C },
		{ ImUiInputShortcut_Copy,					ImUiInputModifier_RightCtrl,	ImUiInputKey_C },
		{ ImUiInputShortcut_Paste,					ImUiInputModifier_LeftCtrl,		ImUiInputKey_V },
		{ ImUiInputShortcut_Paste,					ImUiInputModifier_RightCtrl,	ImUiInputKey_V },
		{ ImUiInputShortcut_Paste,					ImUiInputModifier_LeftShift,	ImUiInputKey_Insert },
		{ ImUiInputShortcut_Paste,					ImUiInputModifier_RightShift,	ImUiInputKey_Insert },
		{ ImUiInputShortcut_SelectAll,				ImUiInputModifier_LeftCtrl,		ImUiInputKey_A },
		{ ImUiInputShortcut_SelectAll,				ImUiInputModifier_RightCtrl,	ImUiInputKey_A },
		{ ImUiInputShortcut_ToggleInsertReplace,	0u,								ImUiInputKey_Insert }
	};

	parameters->shortcuts			= s_inputShortcuts;
	parameters->shortcutCount		= IMAPP_ARRAY_COUNT( s_inputShortcuts );
}

static bool ImAppInitialize( ImAppInternal* imapp, const ImAppParameters* parameters )
{
	if( parameters->windowMode != ImAppDefaultWindow_Disabled )
	{
		ImAppWindowStyle style = ImAppWindowState_Resizable;
		ImAppWindowState state = ImAppWindowState_Default;
		switch( parameters->windowMode )
		{
		case ImAppDefaultWindow_Resizable:
			break;

		case ImAppDefaultWindow_Fullscreen:
			style = ImAppWindowState_Borderless;
			state = ImAppWindowState_Maximized;
			break;

		case ImAppDefaultWindow_Disabled:
			break;
		}

		imapp->window = ImAppPlatformWindowCreate( imapp->platform, parameters->windowTitle, 0, 0, parameters->windowWidth, parameters->windowHeight, style, state );
		if( !imapp->window )
		{
			ImAppPlatformShowError( imapp->platform, "Failed to create Window." );
			ImAppCleanup( imapp );
			return 1;
		}

		imapp->context.defaultWindow = imapp->window;
	}

	imapp->renderer = ImAppRendererCreate( &imapp->allocator, imapp->platform, imapp->window, parameters->windowClearColor );
	if( imapp->renderer == NULL )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create Renderer." );
		return false;
	}

	ImUiParameters uiParameters;
	memset( &uiParameters, 0, sizeof( uiParameters ) );

	uiParameters.allocator		= imapp->allocator;
	uiParameters.vertexType		= ImUiVertexType_IndexedVertexList;
	uiParameters.vertexFormat	= ImAppRendererGetVertexFormat();
	uiParameters.shortcuts		= parameters->shortcuts;
	uiParameters.shortcutCount	= parameters->shortcutCount;

	imapp->context.imui = ImUiCreate( &uiParameters );
	if( !imapp->context.imui )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create ImUi." );
		return false;
	}

	imapp->ressys = ImAppResSysCreate( &imapp->allocator, imapp->platform, imapp->renderer, imapp->context.imui );
	if( imapp->ressys == NULL )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create Resource System." );
		return false;
	}

	if( parameters->defaultFontName )
	{
		imapp->defaultFont = ImAppResSysFontCreateSystem( imapp->ressys, parameters->defaultFontName, parameters->defaultFontSize, &imapp->defaultFontTexture );

		ImUiToolboxConfig toolboxConfig;
		ImUiToolboxFillDefaultConfig( &toolboxConfig, imapp->defaultFont );
		ImUiToolboxSetConfig( &toolboxConfig );
	}

	if( parameters->defaultResPakData.data && parameters->defaultResPakData.size )
	{
		imapp->defaultResPak = ImAppResSysAdd( imapp->ressys, parameters->defaultResPakData.data, parameters->defaultResPakData.size );
	}
	else if( parameters->defaultResPakName )
	{
		char buffer[ 256u ];
		snprintf( buffer, IMAPP_ARRAY_COUNT( buffer ), "%s.iarespak", parameters->defaultResPakName );

		imapp->defaultResPak = ImAppResSysOpen( imapp->ressys, buffer );
	}

	return true;
}

static void ImAppCleanup( ImAppInternal* imapp )
{
	if( imapp->programContext != NULL )
	{
		ImAppProgramShutdown( &imapp->context, imapp->programContext );
		imapp->programContext = NULL;
	}

	if( imapp->defaultFont )
	{
		ImUiFontDestroy( imapp->context.imui, imapp->defaultFont );
		imapp->defaultFont = NULL;
	}

	if( imapp->defaultFontTexture )
	{
		ImAppRendererTextureDestroy( imapp->renderer, imapp->defaultFontTexture );
		imapp->defaultFontTexture = NULL;
	}

	if( imapp->defaultResPak )
	{
		ImAppResSysClose( imapp->ressys, imapp->defaultResPak );
		imapp->defaultResPak = NULL;
	}

	if( imapp->ressys != NULL )
	{
		ImAppResSysDestroy( imapp->ressys );
		imapp->ressys = NULL;
	}

	if( imapp->context.imui )
	{
		ImUiDestroy( imapp->context.imui );
		imapp->context.imui = NULL;
	}

	if( imapp->renderer != NULL )
	{
		ImAppRendererDestroy( imapp->renderer );
		imapp->renderer = NULL;
	}

	if( imapp->window != NULL )
	{
		ImAppPlatformWindowDestroy( imapp->window );
		imapp->window = NULL;
	}

	ImAppPlatformShutdown( imapp->platform );

	ImUiMemoryFree( &imapp->allocator, imapp );
}

static void ImAppHandleWindowEvents( ImAppInternal* imapp, ImAppWindow* window )
{
	ImAppEventQueue* pEventQueue = ImAppPlatformWindowGetEventQueue( window );
	ImUiInput* input = ImUiInputBegin( imapp->context.imui );

	ImAppEvent windowEvent;
	while( ImAppEventQueuePop( pEventQueue, &windowEvent ) )
	{
		switch( windowEvent.type )
		{
		case ImAppEventType_WindowClose:
			imapp->running = false;
			break;

		case ImAppEventType_KeyDown:
			ImUiInputPushKeyDown( input, windowEvent.key.key );

			if( windowEvent.key.repeat )
			{
				ImUiInputPushKeyRepeat( input, windowEvent.key.key );
			}
			break;

		case ImAppEventType_KeyUp:
			ImUiInputPushKeyUp( input, windowEvent.key.key );
			break;

		case ImAppEventType_Character:
			ImUiInputPushTextChar( input, windowEvent.character.character );
			break;

		case ImAppEventType_Motion:
			ImUiInputPushMouseMove( input, (float)windowEvent.motion.x, (float)windowEvent.motion.y );
			break;

		case ImAppEventType_ButtonDown:
			ImUiInputPushMouseDown( input, windowEvent.button.button );
			break;

		case ImAppEventType_ButtonUp:
			ImUiInputPushMouseUp( input, windowEvent.button.button );
			break;

		case ImAppEventType_Scroll:
			ImUiInputPushMouseScroll( input, (float)windowEvent.scroll.x, (float)windowEvent.scroll.y );
			break;
		}
	}

	ImUiInputEnd( imapp->context.imui );
}

bool ImAppWindowPopDropData( ImAppWindow* window, ImAppDropData* outData )
{
	return ImAppPlatformWindowPopDropData( window, outData );
}

void ImAppQuit( ImAppContext* imapp, int exitCode )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;

#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	emscripten_cancel_main_loop();
#endif
	imappInt->running	= false;
	imappInt->exitCode	= exitCode;
}

ImAppResPak* ImAppResourceGetDefaultPak( ImAppContext* imapp )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	return imappInt->defaultResPak;
}

ImAppResPak* ImAppResourceAddMemoryPak( ImAppContext* imapp, const void* pakData, size_t dataLength )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	return ImAppResSysAdd( imappInt->ressys, pakData, dataLength );
}

ImAppResPak* ImAppResourceOpenPak( ImAppContext* imapp, const char* resourcePath )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	return ImAppResSysOpen( imappInt->ressys, resourcePath );
}

void ImAppResourceClosePak( ImAppContext* imapp, ImAppResPak* pak )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	ImAppResSysClose( imappInt->ressys, pak );
}

ImAppImage* ImAppImageLoadResource( ImAppContext* imapp, const char* resourceName )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	ImAppImage* pImage = ImAppResSysImageLoadResource( imappInt->ressys, resourceName );
	if( pImage == NULL )
	{
		return NULL;
	}

	return pImage;
}

ImAppImage* ImAppImageCreateRaw( ImAppContext* imapp, const void* imageData, size_t imageDataSize, int width, int height )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	return ImAppResSysImageCreateRaw( imappInt->ressys, imageData, width, height );
}

ImAppImage* ImAppImageCreatePng( ImAppContext* imapp, const void* imageData, size_t imageDataSize )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	return ImAppResSysImageCreatePng( imappInt->ressys, imageData, imageDataSize );
}

ImAppImage* ImAppImageCreateJpeg( ImAppContext* imapp, const void* imageData, size_t imageDataSize )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	return ImAppResSysImageCreateJpeg( imappInt->ressys, imageData, imageDataSize );
}

ImAppResState ImAppImageGetState( ImAppContext* imapp, ImAppImage* image )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	return ImAppResSysImageGetState( imappInt->ressys, image );
}

void ImAppImageFree( ImAppContext* imapp, ImAppImage* image )
{
	ImAppInternal* imappInt = (ImAppInternal*)imapp;
	ImAppResSysImageFree( imappInt->ressys, image );
}

ImUiImage ImAppImageGetImage( const ImAppImage* image )
{
	ImUiImage result;
	if( image && image->state == ImAppResState_Ready )
	{
		return image->data;
	}
	else
	{
		memset( &result, 0, sizeof( result ) );
	}

	return result;
}
