#include "imapp/imapp.h"

#include "imapp_debug.h"
#include "imapp_event_queue.h"
#include "imapp_internal.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"
#include "imapp_res_sys.h"
#include "imapp_window_theme.h"

#if IMAPP_ENABLED(  IMAPP_PLATFORM_WEB )
#	include <emscripten.h>
#endif

#include <stdio.h>
#include <string.h>

static void		imappFillDefaultParameters( ImAppParameters* parameters );
static bool		imappInitialize( ImAppContext* imapp, const ImAppParameters* parameters );
static void		imappCleanup( ImAppContext* imapp );
static bool		imappHandleWindowEvents( ImAppContext* imapp, ImAppWindow* window, ImUiInput* input );
static void		imappTick( void* arg );
static void		imappTickUi( ImAppWindow* appWindow, void* arg );
static void		imappTickWindowUi( ImAppContext* imapp, ImAppContextWindowInfo* windowInfo );

int imappMain( ImAppPlatform* platform, int argc, char* argv[] )
{
	ImAppContext* imapp = NULL;
	{
		void* programContext = NULL;
		ImAppParameters parameters;
		imappFillDefaultParameters( &parameters );

		programContext = ImAppProgramInitialize( &parameters, argc, argv );
		if( parameters.shutdownAfterInit )
		{
			return parameters.exitCode;
		}
		else if( programContext == NULL )
		{
			imappPlatformShowError( platform, "Failed to initialize Program." );
			return 1;
		}

		ImUiAllocator allocator;
		ImUiMemoryAllocatorPrepare( &allocator, &parameters.allocator );

		imapp = IMUI_MEMORY_NEW_ZERO( &allocator, ImAppContext );
		if( !imapp )
		{
			imappPlatformShowError( platform, "Failed to create ImApp." );
			return 1;
		}

		ImUiMemoryAllocatorFinalize( &imapp->allocator, &allocator );

		imapp->running			= true;
		imapp->platform			= platform;
		imapp->programContext	= programContext;

		if( !imappPlatformInitialize( platform, &imapp->allocator, parameters.resPath ) )
		{
			imappPlatformShowError( imapp->platform, "Failed to initialize Platform." );
			imappCleanup( imapp );
			return 1;
		}

		if( !imappInitialize( imapp, &parameters ) )
		{
			imappCleanup( imapp );
			return 1;
		}

		if( imapp->defaultResPak &&
			parameters.defaultThemeName )
		{
			uint16_t themeResIndex = IMAPP_RES_PAK_INVALID_INDEX;
			while( true )
			{
				imappResSysUpdate( imapp->ressys, true );

				const ImAppResState pakState = ImAppResPakGetState( imapp->defaultResPak );
				if( pakState == ImAppResState_Loading )
				{
					continue;
				}
				else if( pakState == ImAppResState_Error )
				{
					break;
				}

				if( themeResIndex == IMAPP_RES_PAK_INVALID_INDEX )
				{
					themeResIndex = ImAppResPakFindResourceIndex( imapp->defaultResPak, ImAppResPakType_Theme, parameters.defaultThemeName );
					if( themeResIndex == IMAPP_RES_PAK_INVALID_INDEX )
					{
						break;
					}
				}

				const ImAppResState resState = ImAppResPakLoadResourceIndex( imapp->defaultResPak, themeResIndex );
				if( resState == ImAppResState_Loading )
				{
					continue;
				}
				else if( resState == ImAppResState_Error )
				{
					break;
				}

				break;
			}

			if( themeResIndex != IMAPP_RES_PAK_INVALID_INDEX )
			{
				const ImAppTheme* theme = ImAppResPakGetThemeIndex( imapp->defaultResPak, themeResIndex );
				if( theme )
				{
					ImUiToolboxThemeSet( &theme->uiTheme );
					ImAppWindowThemeSet( &theme->windowTheme );
				}
			}
		}

		imapp->tickIntervalMs = parameters.tickIntervalMs;
	}

#if IMAPP_ENABLED(  IMAPP_PLATFORM_WEB )
	emscripten_set_main_loop_arg( imappTick, imapp, 0, 1 );
#else
	while( imapp->running )
	{
		imappTick( imapp );
	}
#endif

	imappCleanup( imapp );
	return 0;
}

static void imappTick( void* arg )
{
	ImAppContext* imapp = (ImAppContext*)arg;

	imapp->lastTickValue = imappPlatformTick( imapp->platform, imapp->lastTickValue, imapp->tickIntervalMs );

	imappResSysUpdate( imapp->ressys, false );

	ImUiInput* input = ImUiInputBegin( imapp->imui );
	for( uintsize i = 0u; i < imapp->windowsCount; ++i )
	{
		ImAppContextWindowInfo* windowInfo = &imapp->windows[ i ];

		if( windowInfo->isDestroyed )
		{
			imappRendererDestructWindow( imapp->renderer, &windowInfo->rendererWindow );
			imappPlatformWindowDestroy( windowInfo->window );

			IMUI_MEMORY_ARRAY_REMOVE_UNSORTED_ZERO( imapp->windows, imapp->windowsCount, i );

			if( imapp->windowsCount == 0 )
			{
				imapp->running = false;
			}

			i--;
			continue;
		}

		imappPlatformWindowUpdate( windowInfo->window, imappTickUi, imapp );

		if( !imappHandleWindowEvents( imapp, windowInfo->window, input ) )
		{
			ImAppWindowDestroy( imapp, windowInfo->window );
			i--;
		}
	}
	ImUiInputEnd( imapp->imui );

	imappTickUi( NULL, arg );
}

static void imappTickUi( ImAppWindow* appWindow, void* arg )
{
	IMAPP_USE( appWindow );

	ImAppContext* imapp = (ImAppContext*)arg;

	const double time = imappPlatformTicksToSeconds( imapp->platform, imapp->lastTickValue );
	imapp->frame = ImUiBegin( imapp->imui, time );

	for( uintsize i = 0u; i < imapp->windowsCount; ++i )
	{
		ImAppContextWindowInfo* windowInfo = &imapp->windows[ i ];
		imappTickWindowUi( imapp, windowInfo );
	}

	ImUiEnd( imapp->frame );
	imapp->frame = NULL;
}

static void imappTickWindowUi( ImAppContext* imapp, ImAppContextWindowInfo* windowInfo )
{
	ImAppWindow* appWindow = windowInfo->window;

	if( ImUiInputGetShortcut( imapp->imui ) == ImUiInputShortcut_Paste )
	{
		imappPlatformGetClipboardText( imapp->platform, imapp->imui );
	}

	const ImAppWindowDeviceState deviceState = imappPlatformWindowGetGlContextState( appWindow );
	if( deviceState == ImAppWindowDeviceState_DeviceLost )
	{
		imappResSysDestroyDeviceResources( imapp->ressys );
		imappRendererDestroyResources( imapp->renderer );

		// ???
		imappPlatformWindowBeginRender( appWindow );
		imappPlatformWindowEndRender( appWindow );
		return;
	}
	else if( deviceState == ImAppWindowDeviceState_NewDevice )
	{
		imappRendererCreateResources( imapp->renderer );
		imappResSysCreateDeviceResources( imapp->ressys );
	}
	else if( deviceState == ImAppWindowDeviceState_NoDevice )
	{
		return;
	}

	int width;
	int height;
	imappPlatformWindowGetSize( appWindow, &width, &height );

	imappPlatformWindowBeginRender( appWindow );

	if( !windowInfo->isRendererCreated )
	{
		imappRendererConstructWindow( imapp->renderer, &windowInfo->rendererWindow );
		windowInfo->isRendererCreated = true;
	}

	{
		const ImUiSize size		= ImUiSizeCreate( (float)width, (float)height );
		ImUiSurface* surface	= ImUiSurfaceBegin( imapp->frame, imappPlatformWindowGetTitle( appWindow ), size, imappPlatformWindowGetDpiScale( appWindow ) );

		ImUiRect windowRect;
		if( imappPlatformWindowGetStyle( appWindow ) == ImAppWindowStyle_Custom )
		{
			windowRect = imappWindowThemeDoUi( appWindow, surface );
		}
		else
		{
			windowRect = ImUiSurfaceGetRect( surface );
		}

		{
			ImUiWindow* uiWindow = ImUiWindowBegin( surface, "main", windowRect, 2 );

			windowInfo->uiFunc( imapp, imapp->programContext, appWindow, uiWindow, windowInfo->uiContext );

			ImUiWindowEnd( uiWindow );
		}

		ImUiSurfaceEnd( surface );

		imappRendererDraw( imapp->renderer, &windowInfo->rendererWindow, surface, width, height, windowInfo->clearColor );
	}

	const ImUiInputMouseCursor cursor = ImUiInputGetMouseCursor( imapp->imui );
	if( cursor != imapp->lastCursor )
	{
		imappPlatformSetMouseCursor( imapp->platform, cursor );
		imapp->lastCursor = cursor;
	}

	const char* copyText = ImUiInputGetCopyText( imapp->imui );
	if( copyText )
	{
		imappPlatformSetClipboardText( imapp->platform, copyText );
		ImUiInputSetCopyText( imapp->imui, NULL, 0u );
	}

	imappPlatformWindowEndRender( appWindow );
}

static void imappFillDefaultParameters( ImAppParameters* parameters )
{
	memset( parameters, 0, sizeof( *parameters ) );

	parameters->resPath						= "./assets";

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
    parameters->defaultFontName				= "Roboto-Regular.ttf";
#elif IMAPP_ENABLED( IMAPP_PLATFORM_LINUX )
    parameters->defaultFontName				= "NotoSans[wdth,wght].ttf";
#else
    parameters->defaultFontName				= "Arial.ttf";
#endif
	parameters->defaultFontSize				= 16.0f;

	parameters->useDefaultWindow			= true;
#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID ) || IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	parameters->window.style				= ImAppWindowStyle_Borderless;
	parameters->window.state				= ImAppWindowState_Maximized;
#else
	parameters->defaultWindow.style			= ImAppWindowStyle_Resizable;
	parameters->defaultWindow.state			= ImAppWindowState_Default;
#endif
	parameters->defaultWindow.title			= "I'm App";
	parameters->defaultWindow.width			= 1280;
	parameters->defaultWindow.height		= 720;
	parameters->defaultWindow.clearColor	= ImUiColorCreate( 0x11u, 0x44u, 0xaau, 0xffu );

	static const ImUiInputShortcutConfig s_inputShortcuts[] =
	{
		{ ImUiInputShortcut_Home,					ImUiInputModifier_None,			ImUiInputKey_Home },
		{ ImUiInputShortcut_End,					ImUiInputModifier_None,			ImUiInputKey_End },
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
		{ ImUiInputShortcut_ToggleInsertReplace,	ImUiInputModifier_None,			ImUiInputKey_Insert },
		{ ImUiInputShortcut_Confirm,				ImUiInputModifier_None,			ImUiInputKey_Gamepad_A },
		{ ImUiInputShortcut_Back,					ImUiInputModifier_None,			ImUiInputKey_Gamepad_B },
		{ ImUiInputShortcut_FocusPrevious,			ImUiInputModifier_LeftShift,	ImUiInputKey_Tab },
		{ ImUiInputShortcut_FocusPrevious,			ImUiInputModifier_RightShift,	ImUiInputKey_Tab },
		{ ImUiInputShortcut_FocusNext,				ImUiInputModifier_None,			ImUiInputKey_Tab },
	};

	parameters->shortcuts			= s_inputShortcuts;
	parameters->shortcutCount		= IMAPP_ARRAY_COUNT( s_inputShortcuts );
}

void ImAppDefaultWindowDoUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow, void* uiContext )
{
	IMAPP_USE( uiContext );

	ImAppProgramDoDefaultWindowUi( imapp, programContext, appWindow, uiWindow );
}

static bool imappInitialize( ImAppContext* imapp, const ImAppParameters* parameters )
{
	imapp->renderer = imappRendererCreate( &imapp->allocator, imapp->platform );
	if( imapp->renderer == NULL )
	{
		imappPlatformShowError( imapp->platform, "Failed to create Renderer." );
		return false;
	}

	if( parameters->useDefaultWindow )
	{
		ImAppWindow* window = ImAppWindowCreate( imapp, &parameters->defaultWindow, ImAppDefaultWindowDoUi, NULL );
		if( !window )
		{
			imappPlatformShowError( imapp->platform, "Failed to create Window." );
			return false;
		}
	}

	ImUiParameters uiParameters;
	memset( &uiParameters, 0, sizeof( uiParameters ) );

	uiParameters.allocator		= parameters->allocator;
	uiParameters.vertexType		= ImUiVertexType_IndexedVertexList;
	uiParameters.vertexFormat	= imappRendererGetVertexFormat();
	uiParameters.shortcuts		= parameters->shortcuts;
	uiParameters.shortcutCount	= parameters->shortcutCount;

	imapp->imui = ImUiCreate( &uiParameters );
	if( !imapp->imui )
	{
		imappPlatformShowError( imapp->platform, "Failed to create ImUi." );
		return false;
	}

	imapp->ressys = imappResSysCreate( &imapp->allocator, imapp->platform, imapp->renderer, imapp->imui );
	if( imapp->ressys == NULL )
	{
		imappPlatformShowError( imapp->platform, "Failed to create Resource System." );
		return false;
	}

	if( parameters->defaultFontName )
	{
		imapp->defaultFont = imappResSysFontCreateSystem( imapp->ressys, parameters->defaultFontName, parameters->defaultFontSize );
	}

	{
		ImUiToolboxThemeFillDefault( ImUiToolboxThemeGet(), imapp->defaultFont ? imapp->defaultFont->uiFont : NULL );
		ImAppWindowThemeFillDefault( ImAppWindowThemeGet() );
	}

	if( parameters->defaultResPakData.data && parameters->defaultResPakData.size )
	{
		imapp->defaultResPak = imappResSysAdd( imapp->ressys, parameters->defaultResPakData.data, parameters->defaultResPakData.size );
	}
	else if( parameters->defaultResPakName )
	{
		char buffer[ 256u ];
		snprintf( buffer, IMAPP_ARRAY_COUNT( buffer ), "%s.iarespak", parameters->defaultResPakName );

		imapp->defaultResPak = imappResSysOpen( imapp->ressys, buffer );
	}

	return true;
}

static void imappCleanup( ImAppContext* imapp )
{
	if( imapp->programContext != NULL )
	{
		ImAppProgramShutdown( imapp, imapp->programContext );
		imapp->programContext = NULL;
	}

	if( imapp->defaultFont )
	{
		imappResSysFontDestroy( imapp->ressys, imapp->defaultFont );
		imapp->defaultFont = NULL;
	}

	if( imapp->defaultResPak )
	{
		imappResSysClose( imapp->ressys, imapp->defaultResPak );
		imapp->defaultResPak = NULL;
	}

	if( imapp->ressys != NULL )
	{
		imappResSysDestroy( imapp->ressys );
		imapp->ressys = NULL;
	}

	if( imapp->imui )
	{
		ImUiDestroy( imapp->imui );
		imapp->imui = NULL;
	}

	while( imapp->windowsCount > 0 )
	{
		ImAppWindowDestroy( imapp, imapp->windows[ 0 ].window );
	}
	ImUiMemoryFree( &imapp->allocator, imapp->windows );
	imapp->windows = NULL;

	if( imapp->renderer != NULL )
	{
		imappRendererDestroy( imapp->renderer );
		imapp->renderer = NULL;
	}

	imappPlatformShutdown( imapp->platform );

	ImUiMemoryFree( &imapp->allocator, imapp );
}

static bool imappHandleWindowEvents( ImAppContext* imapp, ImAppWindow* window, ImUiInput* input )
{
	ImAppEventQueue* pEventQueue = imappPlatformWindowGetEventQueue( window );

	ImAppEvent windowEvent;
	while( imappEventQueuePop( pEventQueue, &windowEvent ) )
	{
		switch( windowEvent.type )
		{
		case ImAppEventType_WindowClose:
			return false;

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

		case ImAppEventType_DoubleClick:
			ImUiInputPushMouseDoubleClick( input, windowEvent.button.button );
			break;

		case ImAppEventType_Scroll:
			ImUiInputPushMouseScroll( input, (float)windowEvent.scroll.x, (float)windowEvent.scroll.y );
			break;

		case ImAppEventType_Direction:
			ImUiInputPushDirection( input, windowEvent.direction.x, windowEvent.direction.y );
			break;
		}
	}

	const ImUiPos focusDirection = ImUiInputGetDirection( imapp->imui );
	if( focusDirection.x != 0.0f || focusDirection.y != 0.0f )
	{
		const double time = imappPlatformTicksToSeconds( imapp->platform, imapp->lastTickValue );
		if( (time - imapp->lastFocusExecuteTime) > 0.5 )
		{
			ImUiInputPushFocusExecute( input );
			imapp->lastFocusExecuteTime = time;
		}
	}
	else
	{
		imapp->lastFocusExecuteTime = 0.0f;
	}

	return true;
}

ImAppWindow* ImAppWindowCreate( ImAppContext* imapp, const ImAppWindowParameters* parameters, ImAppWindowDoUiFunc uiFunc, void* uiContext )
{
	if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY_ZERO( &imapp->allocator, imapp->windows, imapp->windowsCapacity, imapp->windowsCount + 1 ) )
	{
		return NULL;
	}

	ImAppWindow* window = imappPlatformWindowCreate( imapp->platform, parameters );
	if( !window )
	{
		return NULL;
	}

	ImAppContextWindowInfo* windowInfo = &imapp->windows[ imapp->windowsCount++ ];
	windowInfo->window		= window;
	windowInfo->uiFunc		= uiFunc;
	windowInfo->uiContext	= uiContext;

	windowInfo->clearColor[ 0 ]	= (float)parameters->clearColor.red / 255.0f;
	windowInfo->clearColor[ 1 ]	= (float)parameters->clearColor.green / 255.0f;
	windowInfo->clearColor[ 2 ]	= (float)parameters->clearColor.blue / 255.0f;
	windowInfo->clearColor[ 3 ]	= (float)parameters->clearColor.alpha / 255.0f;

	return window;
}

void ImAppWindowDestroy( ImAppContext* imapp, ImAppWindow* window )
{
	ImAppContextWindowInfo* windowInfo = NULL;
	for( uintsize i = 0; i < imapp->windowsCount; ++i )
	{
		if( imapp->windows[ i ].window != window )
		{
			continue;
		}

		windowInfo = &imapp->windows[ i ];
		break;
	}

	if( !windowInfo )
	{
		return;
	}

	windowInfo->isDestroyed = true;
}

bool ImAppWindowIsOpen( const ImAppContext* imapp, const ImAppWindow* window )
{
	for( uintsize i = 0; i < imapp->windowsCount; ++i )
	{
		if( imapp->windows[ i ].window == window &&
			!imapp->windows[ i ].isDestroyed )
		{
			return true;
		}
	}

	return false;
}

bool ImAppWindowHasFocus( const ImAppWindow* window )
{
	return imappPlatformWindowHasFocus( window );
}

void ImAppWindowGetPosition( const ImAppWindow* window, int* outX, int* outY )
{
	imappPlatformWindowGetPosition( window, outX, outY );
}

void ImAppWindowSetPosition( ImAppWindow* window, int x, int y )
{
	imappPlatformWindowSetPosition( window, x, y );
}

void ImAppWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	imappPlatformWindowGetViewRect( window, outX, outY, outWidth, outHeight );
}

ImAppWindowStyle ImAppWindowGetStyle( const ImAppWindow* window )
{
	return imappPlatformWindowGetStyle( window );
}

ImAppWindowState ImAppWindowGetState( const ImAppWindow* window )
{
	return imappPlatformWindowGetState( window );
}

void ImAppWindowSetState( ImAppWindow* window, ImAppWindowState state )
{
	imappPlatformWindowSetState( window, state );
}

bool ImAppWindowPopDropData( ImAppWindow* window, ImAppDropData* outData )
{
	return imappPlatformWindowPopDropData( window, outData );
}

ImUiContext* ImAppGetUi( ImAppContext* imapp )
{
	return imapp->imui;
}

void ImAppQuit( ImAppContext* imapp, int exitCode )
{
#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	emscripten_cancel_main_loop();
#endif
	imapp->running	= false;
	imapp->exitCode	= exitCode;
}

ImAppResPak* ImAppResourceGetDefaultPak( ImAppContext* imapp )
{
	return imapp->defaultResPak;
}

ImAppResPak* ImAppResourceAddMemoryPak( ImAppContext* imapp, const void* pakData, size_t dataLength )
{
	return imappResSysAdd( imapp->ressys, pakData, dataLength );
}

ImAppResPak* ImAppResourceOpenPak( ImAppContext* imapp, const char* resourcePath )
{
	return imappResSysOpen( imapp->ressys, resourcePath );
}

void ImAppResourceClosePak( ImAppContext* imapp, ImAppResPak* pak )
{
	imappResSysClose( imapp->ressys, pak );
}

void ImAppResPakActivateTheme( ImAppContext* imapp, ImAppResPak* pak, const char* name )
{
	const ImAppTheme* theme = ImAppResPakGetTheme( pak, name );
	if( !theme )
	{
		return;
	}

	ImUiToolboxThemeSet( &theme->uiTheme );
	ImAppWindowThemeSet( &theme->windowTheme );

	if( !theme->uiTheme.font &&
		imapp->defaultFont )
	{
		ImUiToolboxThemeGet()->font = imapp->defaultFont->uiFont;
	}
}

ImAppImage* ImAppImageLoadResource( ImAppContext* imapp, const char* resourceName )
{
	ImAppImage* pImage = imappResSysImageLoadResource( imapp->ressys, resourceName );
	if( pImage == NULL )
	{
		return NULL;
	}

	return pImage;
}

ImAppImage* ImAppImageCreateRaw( ImAppContext* imapp, const void* imageData, size_t imageDataSize, int width, int height )
{
	if( imageDataSize < (size_t)(width * height * 4) )
	{
		IMAPP_DEBUG_LOGW( "Insuficiant data to create image." );
		return NULL;
	}

	return imappResSysImageCreateRaw( imapp->ressys, imageData, width, height );
}

ImAppImage* ImAppImageCreatePng( ImAppContext* imapp, const void* imageData, size_t imageDataSize )
{
	return imappResSysImageCreatePng( imapp->ressys, imageData, imageDataSize );
}

ImAppImage* ImAppImageCreateJpeg( ImAppContext* imapp, const void* imageData, size_t imageDataSize )
{
	return imappResSysImageCreateJpeg( imapp->ressys, imageData, imageDataSize );
}

ImAppResState ImAppImageGetState( ImAppContext* imapp, ImAppImage* image )
{
	return imappResSysImageGetState( imapp->ressys, image );
}

void ImAppImageFree( ImAppContext* imapp, ImAppImage* image )
{
	imappResSysImageFree( imapp->ressys, image );
}

ImUiImage ImAppImageGetImage( const ImAppImage* image )
{
	ImUiImage result;
	if( image && image->state == ImAppResState_Ready )
	{
		return image->uiImage;
	}
	else
	{
		memset( &result, 0, sizeof( result ) );
	}

	return result;
}
