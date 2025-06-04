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

static void		ImAppFillDefaultParameters( ImAppParameters* parameters );
static bool		ImAppInitialize( ImAppContext* imapp, const ImAppParameters* parameters );
static void		ImAppCleanup( ImAppContext* imapp );
static void		ImAppHandleWindowEvents( ImAppContext* imapp, ImAppWindow* window );
static void		ImAppTick( void* arg );
static void		ImAppTickUi( ImAppWindow* appWindow, void* arg );
static void		ImAppTickWindowUi( ImAppContext* imapp, ImAppWindow* appWindow );

int ImAppMain( ImAppPlatform* platform, int argc, char* argv[] )
{
	ImAppContext* imapp = NULL;
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

		imapp = IMUI_MEMORY_NEW_ZERO( &allocator, ImAppContext );
		if( !imapp )
		{
			ImAppPlatformShowError( platform, "Failed to create ImApp." );
			return 1;
		}

		ImUiMemoryAllocatorFinalize( &imapp->allocator, &allocator );

		imapp->running			= true;
		imapp->isFullscrene		= parameters.windowMode == ImAppDefaultWindow_Fullscreen;
		imapp->useWindowStyle	= parameters.useWindowStyle;
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

		if( imapp->defaultResPak &&
			parameters.defaultThemeName )
		{
			uint16_t themeResIndex = IMAPP_RES_PAK_INVALID_INDEX;
			while( true )
			{
				ImAppResSysUpdate( imapp->ressys, true );

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
	ImAppContext* imapp = (ImAppContext*)arg;

	imapp->lastTickValue = ImAppPlatformTick( imapp->platform, imapp->lastTickValue, imapp->tickIntervalMs );

	ImAppResSysUpdate( imapp->ressys, false );

	for( uintsize i = 0u; i < imapp->windowsCount; ++i )
	{
		ImAppWindow* window = imapp->windows[ i ];
		ImAppPlatformWindowUpdate( window, ImAppTickUi, imapp );
	}

	ImAppTickUi( NULL, arg );
}

static void ImAppTickUi( ImAppWindow* appWindow, void* arg )
{
	IMAPP_USE( appWindow );

	ImAppContext* imapp = (ImAppContext*)arg;

	const double time = ImAppPlatformTicksToSeconds( imapp->platform, imapp->lastTickValue );
	imapp->frame = ImUiBegin( imapp->imui, time );

	for( uintsize i = 0u; i < imapp->windowsCount; ++i )
	{
		ImAppWindow* window = imapp->windows[ i ];
		ImAppTickWindowUi( imapp, window );
	}

	ImUiEnd( imapp->frame );
	imapp->frame = NULL;
}

static void ImAppTickWindowUi( ImAppContext* imapp, ImAppWindow* appWindow )
{
	ImAppHandleWindowEvents( imapp, appWindow );

	if( ImUiInputGetShortcut( imapp->imui ) == ImUiInputShortcut_Paste )
	{
		ImAppPlatformGetClipboardText( imapp->platform, imapp->imui );
	}

	const ImAppWindowDeviceState deviceState = ImAppPlatformWindowGetGlContextState( appWindow );
	if( deviceState == ImAppWindowDeviceState_DeviceLost )
	{
		ImAppResSysDestroyDeviceResources( imapp->ressys );
		ImAppRendererDestroyResources( imapp->renderer );

		ImAppPlatformWindowPresent( appWindow );
		return;
	}
	else if( deviceState == ImAppWindowDeviceState_NewDevice )
	{
		ImAppRendererCreateResources( imapp->renderer );
		ImAppResSysCreateDeviceResources( imapp->ressys );
	}
	else if( deviceState == ImAppWindowDeviceState_NoDevice )
	{
		return;
	}

	int width;
	int height;
	ImAppPlatformWindowGetSize( appWindow, &width, &height );

	{
		const ImUiSize size		= ImUiSizeCreate( (float)width, (float)height );
		ImUiSurface* surface	= ImUiSurfaceBegin( imapp->frame, "default", size, ImAppPlatformWindowGetDpiScale( appWindow ) );

		ImUiRect windowRect;
		if( !imapp->isFullscrene && imapp->useWindowStyle )
		{
			windowRect = ImAppWindowThemeDoUi( appWindow, surface );
		}
		else
		{
			windowRect = ImUiSurfaceGetRect( surface );
		}

		{
			ImUiWindow* uiWindow = ImUiWindowBegin( surface, "main", windowRect, 2 );

			ImAppWindowDoUiFunc uiFunc = ImAppPlatformWindowGetUiFunc( appWindow );
			uiFunc( imapp, imapp->programContext, appWindow, uiWindow );

			ImUiWindowEnd( uiWindow );
		}

		ImUiSurfaceEnd( surface );

		ImAppRendererDraw( imapp->renderer, appWindow, surface );
	}

	const ImUiInputMouseCursor cursor = ImUiInputGetMouseCursor( imapp->imui );
	if( cursor != imapp->lastCursor )
	{
		ImAppPlatformSetMouseCursor( imapp->platform, cursor );
		imapp->lastCursor = cursor;
	}

	const char* copyText = ImUiInputGetCopyText( imapp->imui );
	if( copyText )
	{
		ImAppPlatformSetClipboardText( imapp->platform, copyText );
		ImUiInputSetCopyText( imapp->imui, NULL, 0u );
	}

	ImAppPlatformWindowPresent( appWindow );
}

static void ImAppFillDefaultParameters( ImAppParameters* parameters )
{
	memset( parameters, 0, sizeof( *parameters ) );

	parameters->resPath				= "./assets";

#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
    parameters->defaultFontName		= "Roboto-Regular.ttf";
#elif IMAPP_ENABLED( IMAPP_PLATFORM_LINUX )
    parameters->defaultFontName		= "NotoSans[wdth,wght].ttf";
#else
    parameters->defaultFontName		= "Arial.ttf";
#endif
	parameters->defaultFontSize		= 16.0f;
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

void ImAppDefaultWindowDoUi( ImAppContext* imapp, void* programContext, ImAppWindow* appWindow, ImUiWindow* uiWindow )
{
	ImAppProgramDoDefaultWindowUi( imapp, programContext, appWindow, uiWindow );
}

static bool ImAppInitialize( ImAppContext* imapp, const ImAppParameters* parameters )
{
	if( parameters->windowMode != ImAppDefaultWindow_Disabled )
	{
		ImAppWindowStyle style = ImAppWindowStyle_Resizable;
		ImAppWindowState state = ImAppWindowState_Default;
		switch( parameters->windowMode )
		{
		case ImAppDefaultWindow_Resizable:
			break;

		case ImAppDefaultWindow_Fullscreen:
			style = ImAppWindowStyle_Borderless;
			state = ImAppWindowState_Maximized;
			break;

		case ImAppDefaultWindow_Disabled:
			break;
		}

		if( parameters->useWindowStyle )
		{
			style = ImAppWindowStyle_Custom;
		}

		ImAppWindow* window = ImAppPlatformWindowCreate( imapp->platform, parameters->windowTitle, 0, 0, parameters->windowWidth, parameters->windowHeight, style, state, ImAppDefaultWindowDoUi );
		if( !window )
		{
			ImAppPlatformShowError( imapp->platform, "Failed to create Window." );
			ImAppCleanup( imapp );
			return false;
		}

		if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( &imapp->allocator, imapp->windows, imapp->windowsCapacity, imapp->windowsCount + 1u ) )
		{
			ImAppCleanup( imapp );
			return false;
		}

		imapp->windows[ imapp->windowsCount ] = window;
		imapp->windowsCount++;
	}

	// TODO: don't use window to create renderer
	imapp->renderer = ImAppRendererCreate( &imapp->allocator, imapp->platform, imapp->windows[ 0u ], parameters->windowClearColor );
	if( imapp->renderer == NULL )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create Renderer." );
		return false;
	}

	ImUiParameters uiParameters;
	memset( &uiParameters, 0, sizeof( uiParameters ) );

	uiParameters.allocator		= parameters->allocator;
	uiParameters.vertexType		= ImUiVertexType_IndexedVertexList;
	uiParameters.vertexFormat	= ImAppRendererGetVertexFormat();
	uiParameters.shortcuts		= parameters->shortcuts;
	uiParameters.shortcutCount	= parameters->shortcutCount;

	imapp->imui = ImUiCreate( &uiParameters );
	if( !imapp->imui )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create ImUi." );
		return false;
	}

	imapp->ressys = ImAppResSysCreate( &imapp->allocator, imapp->platform, imapp->renderer, imapp->imui );
	if( imapp->ressys == NULL )
	{
		ImAppPlatformShowError( imapp->platform, "Failed to create Resource System." );
		return false;
	}

	if( parameters->defaultFontName )
	{
		imapp->defaultFont = ImAppResSysFontCreateSystem( imapp->ressys, parameters->defaultFontName, parameters->defaultFontSize );
	}

	{
		ImUiToolboxThemeFillDefault( ImUiToolboxThemeGet(), imapp->defaultFont ? imapp->defaultFont->uiFont : NULL );
		ImAppWindowThemeFillDefault( ImAppWindowThemeGet() );
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

static void ImAppCleanup( ImAppContext* imapp )
{
	if( imapp->programContext != NULL )
	{
		ImAppProgramShutdown( imapp, imapp->programContext );
		imapp->programContext = NULL;
	}

	if( imapp->defaultFont )
	{
		ImAppResSysFontDestroy( imapp->ressys, imapp->defaultFont );
		imapp->defaultFont = NULL;
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

	if( imapp->imui )
	{
		ImUiDestroy( imapp->imui );
		imapp->imui = NULL;
	}

	if( imapp->renderer != NULL )
	{
		ImAppRendererDestroy( imapp->renderer );
		imapp->renderer = NULL;
	}

	for( uintsize i = 0u; i < imapp->windowsCount; ++i )
	{
		ImAppPlatformWindowDestroy( imapp->windows[ i ] );
	}
	ImUiMemoryFree( &imapp->allocator, imapp->windows );
	imapp->windows = NULL;

	ImAppPlatformShutdown( imapp->platform );

	ImUiMemoryFree( &imapp->allocator, imapp );
}

static void ImAppHandleWindowEvents( ImAppContext* imapp, ImAppWindow* window )
{
	ImAppEventQueue* pEventQueue = ImAppPlatformWindowGetEventQueue( window );
	ImUiInput* input = ImUiInputBegin( imapp->imui );

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
		const double time = ImAppPlatformTicksToSeconds( imapp->platform, imapp->lastTickValue );
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


	ImUiInputEnd( imapp->imui );
}

bool ImAppWindowHasFocus( const ImAppWindow* window )
{
	return ImAppPlatformWindowHasFocus( window );
}

void ImAppWindowGetPosition( const ImAppWindow* window, int* outX, int* outY )
{
	ImAppPlatformWindowGetPosition( window, outX, outY );
}

void ImAppWindowSetPosition( ImAppWindow* window, int x, int y )
{
	ImAppPlatformWindowSetPosition( window, x, y );
}

void ImAppWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	ImAppPlatformWindowGetViewRect( window, outX, outY, outWidth, outHeight );
}

bool ImAppWindowPopDropData( ImAppWindow* window, ImAppDropData* outData )
{
	return ImAppPlatformWindowPopDropData( window, outData );
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
	return ImAppResSysAdd( imapp->ressys, pakData, dataLength );
}

ImAppResPak* ImAppResourceOpenPak( ImAppContext* imapp, const char* resourcePath )
{
	return ImAppResSysOpen( imapp->ressys, resourcePath );
}

void ImAppResourceClosePak( ImAppContext* imapp, ImAppResPak* pak )
{
	ImAppResSysClose( imapp->ressys, pak );
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
	ImAppImage* pImage = ImAppResSysImageLoadResource( imapp->ressys, resourceName );
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

	return ImAppResSysImageCreateRaw( imapp->ressys, imageData, width, height );
}

ImAppImage* ImAppImageCreatePng( ImAppContext* imapp, const void* imageData, size_t imageDataSize )
{
	return ImAppResSysImageCreatePng( imapp->ressys, imageData, imageDataSize );
}

ImAppImage* ImAppImageCreateJpeg( ImAppContext* imapp, const void* imageData, size_t imageDataSize )
{
	return ImAppResSysImageCreateJpeg( imapp->ressys, imageData, imageDataSize );
}

ImAppResState ImAppImageGetState( ImAppContext* imapp, ImAppImage* image )
{
	return ImAppResSysImageGetState( imapp->ressys, image );
}

void ImAppImageFree( ImAppContext* imapp, ImAppImage* image )
{
	ImAppResSysImageFree( imapp->ressys, image );
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
