#include "imapp/imapp.h"

#include "imapp_event_queue.h"
#include "imapp_internal.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"
#include "imapp_resource_storage.h"

#if IMAPP_ENABLED(  IMAPP_PLATFORM_WEB )
#	include <emscripten.h>
#endif

#include <string.h>

static void		ImAppFillDefaultParameters( ImAppParameters* parameters );
static bool		ImAppInitialize( ImAppInternal* imapp, const ImAppParameters* parameters );
static void		ImAppCleanup( ImAppInternal* imapp );
static void		ImAppHandleEvents( ImAppInternal* imapp );
static void		ImAppTick( void* arg );

int ImAppMain( ImAppPlatform* platform, int argc, char* argv[] )
{
	ImAppInternal* imapp = NULL;
	{
		void* programContext = NULL;
		ImAppParameters parameters;
		ImAppFillDefaultParameters( &parameters );

		programContext = ImAppProgramInitialize( &parameters, argc, argv );
		if( programContext == NULL )
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

		ImUiMemoryAllocatorPrepare( &imapp->allocator, &allocator );

		imapp->running			= true;
		imapp->platform			= platform;
		imapp->programContext	= programContext;

		if( !ImAppPlatformInitialize( platform, &imapp->allocator, parameters.resourcePath ) )
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

void ImAppTick( void* arg )
{
	ImAppInternal* imapp = (ImAppInternal*)arg;

	imapp->lastTickValue = ImAppPlatformWindowTick( imapp->window, imapp->lastTickValue, imapp->tickIntervalMs );

	ImAppResourceStorageUpdate( imapp->resources );
	ImAppHandleEvents( imapp );

	ImAppPlatformWindowGetViewRect( &imapp->context.x, &imapp->context.y, &imapp->context.width, &imapp->context.height, imapp->window );

	// UI
	const ImUiDrawData* drawData = NULL;
	{
		const ImUiSize size		= ImUiSizeCreate( (float)imapp->context.width, (float)imapp->context.height );

		ImUiFrame* frame		= ImUiBegin( imapp->context.imui, imapp->lastTickValue / 1000.0f );
		ImUiSurface* surface	= ImUiSurfaceBegin( frame, ImUiStringViewCreate( "default" ), size, 1.0f );

		ImAppProgramDoDefaultWindowUi( &imapp->context, imapp->programContext, surface );

		drawData = ImUiSurfaceEnd( surface );
		ImUiEnd( frame );
	}

	const ImUiInputMouseCursor cursor = ImUiInputGetMouseCursor( imapp->context.imui );
	if( cursor != imapp->lastCursor )
	{
		ImAppPlatformSetMouseCursor( imapp->platform, cursor );
		imapp->lastCursor = cursor;
	}

	ImAppRendererDraw( imapp->renderer, imapp->window, drawData );

	if( !ImAppPlatformWindowPresent( imapp->window ) )
	{
		if( !ImAppRendererRecreateResources( imapp->renderer ) )
		{
			ImAppQuit( &imapp->context );
			return;
		}

		if( !ImAppResourceStorageRecreateEverything( imapp->resources ) )
		{
			ImAppQuit( &imapp->context );
			return;
		}
	}
}

//static const ImAppInputShortcut s_inputShortcuts[] =
//{
//	{ 0u,													ImUiInputKey_LeftShift,	NK_KEY_SHIFT },
//	{ 0u,													ImUiInputKey_RightShift,	NK_KEY_SHIFT },
//	{ 0u,													ImUiInputKey_LeftControl,	NK_KEY_CTRL },
//	{ 0u,													ImUiInputKey_RightControl,	NK_KEY_CTRL },
//	{ 0u,													ImUiInputKey_Delete,		NK_KEY_DEL },
//	{ 0u,													ImUiInputKey_Enter,		NK_KEY_ENTER },
//	{ 0u,													ImUiInputKey_Tab,			NK_KEY_TAB },
//	{ 0u,													ImUiInputKey_Backspace,	NK_KEY_BACKSPACE },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_C,			NK_KEY_COPY },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_X,			NK_KEY_CUT },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_V,			NK_KEY_PASTE },
//	{ 0u,													ImUiInputKey_Up,			NK_KEY_UP },
//	{ 0u,													ImUiInputKey_Down,			NK_KEY_DOWN },
//	{ 0u,													ImUiInputKey_Left,			NK_KEY_LEFT },
//	{ 0u,													ImUiInputKey_Right,		NK_KEY_RIGHT },
//																						/* Shortcuts: text field */
//	{ 0u,													ImUiInputKey_Insert,		NK_KEY_TEXT_INSERT_MODE },
//	{ 0u,													ImUiInputKey_None,			NK_KEY_TEXT_REPLACE_MODE },
//	{ 0u,													ImUiInputKey_None,			NK_KEY_TEXT_RESET_MODE },
//	{ 0u,													ImUiInputKey_Home,			NK_KEY_TEXT_LINE_START },
//	{ 0u,													ImUiInputKey_End,			NK_KEY_TEXT_LINE_END },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Home,			NK_KEY_TEXT_START },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_End,			NK_KEY_TEXT_END },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Z,			NK_KEY_TEXT_UNDO },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Y,			NK_KEY_TEXT_REDO },
//	{ ImAppInputModifier_Ctrl | ImAppInputModifier_Shift,	ImUiInputKey_Z,			NK_KEY_TEXT_REDO },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_A,			NK_KEY_TEXT_SELECT_ALL },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Left,			NK_KEY_TEXT_WORD_LEFT },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Right,		NK_KEY_TEXT_WORD_RIGHT },
//																						/* Shortcuts: scrollbar */
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Home,			NK_KEY_SCROLL_START },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_End,			NK_KEY_SCROLL_END },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Down,			NK_KEY_SCROLL_DOWN },
//	{ ImAppInputModifier_Ctrl,								ImUiInputKey_Up,			NK_KEY_SCROLL_UP },
//};

static void ImAppFillDefaultParameters( ImAppParameters* parameters )
{
	memset( parameters, 0, sizeof( *parameters ) );

	parameters->tickIntervalMs		= 0;
	parameters->resourcePath		= "./assets";
	parameters->defaultFontName		= "arial.ttf";
	parameters->defaultFontSize		= 16.0f;
	parameters->windowEnable		= true;
	parameters->windowTitle			= "I'm App";
	parameters->windowWidth			= 1280;
	parameters->windowHeight		= 720;
	//pParameters->shortcuts				= s_inputShortcuts;
	//pParameters->shortcutsLength		= IMAPP_ARRAY_COUNT( s_inputShortcuts );
}

static bool ImAppInitialize( ImAppInternal* imapp, const ImAppParameters* parameters )
{
	if( parameters->windowEnable )
	{
		imapp->window = ImAppPlatformWindowCreate( imapp->platform, parameters->windowTitle, 0, 0, parameters->windowWidth, parameters->windowHeight, ImAppWindowState_Default );
		if( !imapp->window )
		{
			ImAppPlatformShowError( imapp->platform, "Failed to create Window." );
			ImAppCleanup( imapp );
			return 1;
		}
	}

	imapp->renderer = ImAppRendererCreate( &imapp->allocator, imapp->platform, imapp->window );
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

	imapp->context.imui = ImUiCreate( &uiParameters );
	if( !imapp->context.imui )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create ImUi." );
		return false;
	}

	imapp->resources = ImAppResourceStorageCreate( &imapp->allocator, imapp->platform, imapp->renderer, imapp->context.imui );
	if( imapp->resources == NULL )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create Image Storage." );
		return false;
	}

	if( parameters->defaultFontName )
	{
		const ImUiStringView resourceName = ImUiStringViewCreate( parameters->defaultFontName );

		imapp->defaultFont = ImAppResourceStorageFontCreate( imapp->resources, resourceName, parameters->defaultFontSize );

		ImUiToolboxConfig toolboxConfig;
		ImUiToolboxFillDefaultConfig( &toolboxConfig, imapp->defaultFont );
		ImUiToolboxSetConfig( &toolboxConfig );
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

	if( imapp->resources != NULL )
	{
		ImAppResourceStorageDestroy( imapp->resources );
		imapp->resources = NULL;
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

static void ImAppHandleEvents( ImAppInternal* imapp )
{
	ImAppEventQueue* pEventQueue = ImAppPlatformWindowGetEventQueue( imapp->window );
	ImUiInput* input = ImUiInputBegin( imapp->context.imui );

	imapp->context.dropData = NULL;

	ImAppEvent windowEvent;
	while( ImAppEventQueuePop( pEventQueue, &windowEvent ) )
	{
		switch( windowEvent.type )
		{
		case ImAppEventType_WindowClose:
			imapp->running = false;
			break;

		case ImAppEventType_DropFile:
		case ImAppEventType_DropText:
			imapp->context.dropData = windowEvent.drop.pathOrText;
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

void ImAppQuit( ImAppContext* imapp )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	emscripten_cancel_main_loop();
#endif
	pImApp->running = false;
}

ImAppImage* ImAppImageLoadResource( ImAppContext* imapp, ImUiStringView resourceName )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

	ImAppImage* pImage = ImAppResourceStorageImageFindOrLoad( pImApp->resources, resourceName, false );
	if( pImage == NULL )
	{
		return NULL;
	}

	return pImage;
}

ImAppImage* ImAppImageCreateRaw( ImAppContext* imapp, const void* imageData, size_t imageDataSize, int width, int height )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;
	return ImAppResourceStorageImageCreateRaw( pImApp->resources, imageData, width, height );
}

ImAppImage* ImAppImageCreatePng( ImAppContext* imapp, const void* imageData, size_t imageDataSize )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;
	return ImAppResourceStorageImageCreatePng( pImApp->resources, imageData, imageDataSize );
}

void ImAppImageFree( ImAppContext* imapp, ImAppImage* image )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;
	ImAppResourceStorageImageFree( pImApp->resources, image );
}

ImUiTexture ImAppImageGetTexture( const ImAppImage* image )
{
	ImUiTexture texture;
	if( image )
	{
		texture.data	= image->texture;
		texture.width	= image->width;
		texture.height	= image->height;
	}
	else
	{
		memset( &texture, 0, sizeof( texture ) );
	}

	return texture;
}

ImUiTexture ImAppImageGet( ImAppContext* imapp, ImUiStringView resourceName, int defaultWidth, int defaultHeight, ImUiColor defaultColor )
{
	// TODO: implement async loading
	return ImAppImageGetBlocking( imapp, resourceName );
}

ImUiTexture ImAppImageGetBlocking( ImAppContext* imapp, ImUiStringView resourceName )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

	const ImAppImage* pImage = ImAppResourceStorageImageFindOrLoad( pImApp->resources, resourceName, false );
	return ImAppImageGetTexture( pImage );
}

ImUiFont* ImAppFontGet( ImAppContext* imapp, ImUiStringView fontName, float fontSize )
{
	ImAppInternal* pImApp = (ImAppInternal*)imapp;

	return ImAppResourceStorageFontCreate( pImApp->resources, fontName, fontSize );
}
