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

static void		imappFillDefaultParameters( ImappParameters* parameters );
static bool		imappInitialize( ImappContext* imapp, const ImappParameters* parameters );
static void		imappCleanup( ImappContext* imapp );
static bool		imappHandleWindowEvents( ImappContext* imapp, ImappWindow* window, ImuiInput* input );
static void		imappTick( void* arg );
static void		imappTickUi( ImappWindow* appWindow, void* arg );
static void		imappTickWindowUi( ImappContext* imapp, ImappContextWindowInfo* windowInfo );
static void		imappWindowDestroyInternal( ImappContext* imapp, ImappContextWindowInfo* windowInfo, uintsize windowIndex );

int imappMain( ImappPlatform* platform, int argc, char* argv[] )
{
	ImappContext* imapp = NULL;
	{
		void* programContext = NULL;
		ImappParameters parameters;
		imappFillDefaultParameters( &parameters );

		programContext = imappProgramInitialize( &parameters, argc, argv );
		if( parameters.shutdownAfterInit )
		{
			return parameters.exitCode;
		}
		else if( programContext == NULL )
		{
			imappPlatformShowError( platform, "Failed to initialize Program." );
			return 1;
		}

		ImuiAllocator allocator;
		imuiMemoryAllocatorPrepare( &allocator, &parameters.allocator );

		imapp = IMUI_MEMORY_NEW_ZERO( &allocator, ImappContext );
		if( !imapp )
		{
			imappPlatformShowError( platform, "Failed to create imapp." );
			return 1;
		}

		imuiMemoryAllocatorFinalize( &imapp->allocator, &allocator );

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

				const ImappResState pakState = imappResPakGetState( imapp->defaultResPak );
				if( pakState == ImappResState_Loading )
				{
					continue;
				}
				else if( pakState == ImappResState_Error )
				{
					break;
				}

				if( themeResIndex == IMAPP_RES_PAK_INVALID_INDEX )
				{
					themeResIndex = imappResPakFindResourceIndex( imapp->defaultResPak, ImappResPakType_Theme, parameters.defaultThemeName );
					if( themeResIndex == IMAPP_RES_PAK_INVALID_INDEX )
					{
						break;
					}
				}

				const ImappResState resState = imappResPakLoadResourceIndex( imapp->defaultResPak, themeResIndex );
				if( resState == ImappResState_Loading )
				{
					continue;
				}
				else if( resState == ImappResState_Error )
				{
					break;
				}

				break;
			}

			if( themeResIndex != IMAPP_RES_PAK_INVALID_INDEX )
			{
				const ImappTheme* theme = imappResPakGetThemeIndex( imapp->defaultResPak, themeResIndex );
				if( theme )
				{
					imuiToolboxThemeSet( &theme->uiTheme );
					imappWindowThemeSet( &theme->windowTheme );
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
	ImappContext* imapp = (ImappContext*)arg;

	imapp->lastTickValue = imappPlatformTick( imapp->platform, imapp->lastTickValue, imapp->tickIntervalMs );

	imappResSysUpdate( imapp->ressys, false );
	imappRendererUpdate( imapp->renderer );

	for( uintsize i = 0u; i < imapp->windowsCount; ++i )
	{
		ImappContextWindowInfo* windowInfo = &imapp->windows[ i ];

		if( windowInfo->isDestroyed )
		{
			imappWindowDestroyInternal( imapp, windowInfo, i );

			i--;
			continue;
		}

		imappPlatformWindowUpdate( windowInfo->window, imappTickUi, imapp );

		ImuiInput* input = imuiInputBegin( imapp->imui, windowInfo->inputState );

		if( !imappHandleWindowEvents( imapp, windowInfo->window, input ) )
		{
			imappWindowDestroy( imapp, windowInfo->window );
			i--;
		}

		windowInfo->inputState = imuiInputEnd( imapp->imui );
	}

	imappTickUi( NULL, arg );
}

static void imappTickUi( ImappWindow* appWindow, void* arg )
{
	IMAPP_USE( appWindow );

	ImappContext* imapp = (ImappContext*)arg;

	const double time = imappPlatformTicksToSeconds( imapp->platform, imapp->lastTickValue );
	imapp->frame = imuiBegin( imapp->imui, time );

	for( uintsize i = 0u; i < imapp->windowsCount; ++i )
	{
		ImappContextWindowInfo* windowInfo = &imapp->windows[ i ];
		imappTickWindowUi( imapp, windowInfo );
	}

	imuiEnd( imapp->frame );
	imapp->frame = NULL;
}

static void imappTickWindowUi( ImappContext* imapp, ImappContextWindowInfo* windowInfo )
{
	ImappWindow* appWindow = windowInfo->window;

	const ImappWindowDeviceState deviceState = imappPlatformWindowGetGlContextState( appWindow );
	if( deviceState == ImappWindowDeviceState_DeviceLost )
	{
		for( uintsize i = 0u; i < imapp->windowsCount; ++i )
		{
			ImappContextWindowInfo* otherWindowInfo = &imapp->windows[ i ];
			if( !otherWindowInfo->isRendererCreated )
			{
				continue;
			}

			imappRendererDestructWindow( imapp->renderer, &otherWindowInfo->rendererWindow );
			otherWindowInfo->isRendererCreated = false;
		}

		imappResSysDestroyDeviceResources( imapp->ressys );
		imappRendererDestroyResources( imapp->renderer );

		// ???
		imappPlatformWindowBeginRender( appWindow );
		imappPlatformWindowEndRender( appWindow );
		return;
	}
	else if( deviceState == ImappWindowDeviceState_NewDevice )
	{
		imappRendererCreateResources( imapp->renderer );
		imappResSysCreateDeviceResources( imapp->ressys );
	}
	else if( deviceState == ImappWindowDeviceState_NoDevice )
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

	if( windowInfo->inputState )
	{
		const ImuiSize size		= imuiSizeCreate( (float)width, (float)height );
		ImuiSurface* surface	= imuiSurfaceBegin( imapp->frame, imappPlatformWindowGetTitle( appWindow ), size, windowInfo->inputState, imappPlatformWindowGetDpiScale( appWindow ) );

		if( imuiInputGetShortcut( imuiSurfaceGetInput( surface ) ) == ImuiInputShortcut_Paste )
		{
			imappPlatformGetClipboardText( imapp->platform, imapp->imui );
		}

		ImuiRect windowRect;
		if( imappPlatformWindowGetStyle( appWindow ) == ImappWindowStyle_Custom )
		{
			windowRect = imappWindowThemeDoUi( appWindow, surface );
		}
		else
		{
			windowRect = imuiSurfaceGetRect( surface );
		}

		{
			ImuiWindow* uiWindow = imuiWindowBegin( surface, "main", windowRect, 2 );

			windowInfo->uiFunc( imapp, imapp->programContext, appWindow, uiWindow, windowInfo->uiContext );

			imuiWindowEnd( uiWindow );
		}

		imuiSurfaceEnd( surface );

		imappRendererDraw( imapp->renderer, &windowInfo->rendererWindow, surface, width, height, windowInfo->clearColor );
	}

	const ImuiInputMouseCursor cursor = imuiInputGetMouseCursor( imapp->imui );
	if( cursor != imapp->lastCursor )
	{
		imappPlatformSetMouseCursor( imapp->platform, cursor );
		imapp->lastCursor = cursor;
	}

	const char* copyText = imuiInputGetCopyText( imapp->imui );
	if( copyText )
	{
		imappPlatformSetClipboardText( imapp->platform, copyText );
		imuiInputSetCopyText( imapp->imui, NULL, 0u );
	}

	imappPlatformWindowEndRender( appWindow );
}

static void imappFillDefaultParameters( ImappParameters* parameters )
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
	parameters->defaultWindow.style			= ImappWindowStyle_Borderless;
	parameters->defaultWindow.state			= ImappWindowState_Maximized;
#else
	parameters->defaultWindow.style			= ImappWindowStyle_Resizable;
	parameters->defaultWindow.state			= ImappWindowState_Default;
#endif
	parameters->defaultWindow.title			= "I'm App";
	parameters->defaultWindow.x				= IMAPP_WINDOW_DEFAULT_POSITION;
	parameters->defaultWindow.y				= IMAPP_WINDOW_DEFAULT_POSITION;
	parameters->defaultWindow.width			= 1280;
	parameters->defaultWindow.height		= 720;
	parameters->defaultWindow.clearColor	= imuiColorCreate( 0x11u, 0x44u, 0xaau, 0xffu );

	static const ImuiInputShortcutConfig s_inputShortcuts[] =
	{
		{ ImuiInputShortcut_Home,					ImuiInputModifier_None,			ImuiInputKey_Home },
		{ ImuiInputShortcut_End,					ImuiInputModifier_None,			ImuiInputKey_End },
		{ ImuiInputShortcut_Undo,					ImuiInputModifier_LeftCtrl,		ImuiInputKey_Z },
		{ ImuiInputShortcut_Undo,					ImuiInputModifier_RightCtrl,	ImuiInputKey_Z },
		{ ImuiInputShortcut_Redo,					ImuiInputModifier_LeftCtrl,		ImuiInputKey_Y },
		{ ImuiInputShortcut_Redo,					ImuiInputModifier_RightCtrl,	ImuiInputKey_Y },
		{ ImuiInputShortcut_Cut,					ImuiInputModifier_LeftCtrl,		ImuiInputKey_X },
		{ ImuiInputShortcut_Cut,					ImuiInputModifier_RightCtrl,	ImuiInputKey_X },
		{ ImuiInputShortcut_Cut,					ImuiInputModifier_LeftShift,	ImuiInputKey_Delete },
		{ ImuiInputShortcut_Cut,					ImuiInputModifier_RightShift,	ImuiInputKey_Delete },
		{ ImuiInputShortcut_Copy,					ImuiInputModifier_LeftCtrl,		ImuiInputKey_C },
		{ ImuiInputShortcut_Copy,					ImuiInputModifier_RightCtrl,	ImuiInputKey_C },
		{ ImuiInputShortcut_Paste,					ImuiInputModifier_LeftCtrl,		ImuiInputKey_V },
		{ ImuiInputShortcut_Paste,					ImuiInputModifier_RightCtrl,	ImuiInputKey_V },
		{ ImuiInputShortcut_Paste,					ImuiInputModifier_LeftShift,	ImuiInputKey_Insert },
		{ ImuiInputShortcut_Paste,					ImuiInputModifier_RightShift,	ImuiInputKey_Insert },
		{ ImuiInputShortcut_SelectAll,				ImuiInputModifier_LeftCtrl,		ImuiInputKey_A },
		{ ImuiInputShortcut_SelectAll,				ImuiInputModifier_RightCtrl,	ImuiInputKey_A },
		{ ImuiInputShortcut_ToggleInsertReplace,	ImuiInputModifier_None,			ImuiInputKey_Insert },
		{ ImuiInputShortcut_Confirm,				ImuiInputModifier_None,			ImuiInputKey_Gamepad_A },
		{ ImuiInputShortcut_Back,					ImuiInputModifier_None,			ImuiInputKey_Gamepad_B },
		{ ImuiInputShortcut_FocusPrevious,			ImuiInputModifier_LeftShift,	ImuiInputKey_Tab },
		{ ImuiInputShortcut_FocusPrevious,			ImuiInputModifier_RightShift,	ImuiInputKey_Tab },
		{ ImuiInputShortcut_FocusNext,				ImuiInputModifier_None,			ImuiInputKey_Tab },
	};

	parameters->shortcuts			= s_inputShortcuts;
	parameters->shortcutCount		= IMAPP_ARRAY_COUNT( s_inputShortcuts );
}

void imappDefaultWindowDoUi( ImappContext* imapp, void* programContext, ImappWindow* appWindow, ImuiWindow* uiWindow, void* uiContext )
{
	IMAPP_USE( uiContext );

	imappProgramDoDefaultWindowUi( imapp, programContext, appWindow, uiWindow );
}

static bool imappInitialize( ImappContext* imapp, const ImappParameters* parameters )
{
	imapp->renderer = imappRendererCreate( &imapp->allocator, imapp->platform );
	if( imapp->renderer == NULL )
	{
		imappPlatformShowError( imapp->platform, "Failed to create Renderer." );
		return false;
	}

	if( parameters->useDefaultWindow )
	{
		ImappWindow* window = imappWindowCreate( imapp, &parameters->defaultWindow, imappDefaultWindowDoUi, NULL );
		if( !window )
		{
			imappPlatformShowError( imapp->platform, "Failed to create Window." );
			return false;
		}
	}

	ImuiParameters uiParameters;
	memset( &uiParameters, 0, sizeof( uiParameters ) );

	uiParameters.allocator		= parameters->allocator;
	uiParameters.vertexType		= ImuiVertexType_IndexedVertexList;
	uiParameters.vertexFormat	= imappRendererGetVertexFormat();
	uiParameters.shortcuts		= parameters->shortcuts;
	uiParameters.shortcutCount	= parameters->shortcutCount;

	imapp->imui = imuiCreate( &uiParameters );
	if( !imapp->imui )
	{
		imappPlatformShowError( imapp->platform, "Failed to create imui." );
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
		imuiToolboxThemeFillDefault( imuiToolboxThemeGet(), imapp->defaultFont ? imapp->defaultFont->uiFont : NULL );
		imappWindowThemeFillDefault( imappWindowThemeGet() );
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

static void imappCleanup( ImappContext* imapp )
{
	if( imapp->programContext != NULL )
	{
		imappProgramShutdown( imapp, imapp->programContext );
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
		imuiDestroy( imapp->imui );
		imapp->imui = NULL;
	}

	while( imapp->windowsCount > 0 )
	{
		imappWindowDestroyInternal( imapp, &imapp->windows[ 0 ], 0 );
	}
	imuiMemoryFree( &imapp->allocator, imapp->windows );
	imapp->windows = NULL;

	if( imapp->renderer != NULL )
	{
		imappRendererDestroy( imapp->renderer );
		imapp->renderer = NULL;
	}

	imappPlatformShutdown( imapp->platform );

	imuiMemoryFree( &imapp->allocator, imapp );
}

static bool imappHandleWindowEvents( ImappContext* imapp, ImappWindow* window, ImuiInput* input )
{
	ImappEventQueue* pEventQueue = imappPlatformWindowGetEventQueue( window );

	ImappEvent windowEvent;
	while( imappEventQueuePop( pEventQueue, &windowEvent ) )
	{
		switch( windowEvent.type )
		{
		case ImappEventType_WindowClose:
			return false;

		case ImappEventType_KeyDown:
			imuiInputPushKeyDown( input, windowEvent.key.key );

			if( windowEvent.key.repeat )
			{
				imuiInputPushKeyRepeat( input, windowEvent.key.key );
			}
			break;

		case ImappEventType_KeyUp:
			imuiInputPushKeyUp( input, windowEvent.key.key );
			break;

		case ImappEventType_Character:
			imuiInputPushTextChar( input, windowEvent.character.character );
			break;

		case ImappEventType_Motion:
			imuiInputPushMouseMove( input, (float)windowEvent.motion.x, (float)windowEvent.motion.y );
			break;

		case ImappEventType_ButtonDown:
			imuiInputPushMouseDown( input, windowEvent.button.button );
			break;

		case ImappEventType_ButtonUp:
			imuiInputPushMouseUp( input, windowEvent.button.button );
			break;

		case ImappEventType_DoubleClick:
			imuiInputPushMouseDoubleClick( input, windowEvent.button.button );
			break;

		case ImappEventType_Scroll:
			imuiInputPushMouseScroll( input, (float)windowEvent.scroll.x, (float)windowEvent.scroll.y );
			break;

		case ImappEventType_Direction:
			imuiInputPushDirection( input, windowEvent.direction.x, windowEvent.direction.y );
			break;
		}
	}

	const ImuiPos focusDirection = imuiInputGetDirection( imuiInputGetPushState( input ) );
	if( focusDirection.x != 0.0f || focusDirection.y != 0.0f )
	{
		const double time = imappPlatformTicksToSeconds( imapp->platform, imapp->lastTickValue );
		if( (time - imapp->lastFocusExecuteTime) > 0.5 )
		{
			imuiInputPushFocusExecute( input );
			imapp->lastFocusExecuteTime = time;
		}
	}
	else
	{
		imapp->lastFocusExecuteTime = 0.0f;
	}

	return true;
}

ImappWindow* imappWindowCreate( ImappContext* imapp, const ImappWindowParameters* parameters, ImappWindowDoUiFunc uiFunc, void* uiContext )
{
	if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY_ZERO( &imapp->allocator, imapp->windows, imapp->windowsCapacity, imapp->windowsCount + 1 ) )
	{
		return NULL;
	}

	ImappWindow* window = imappPlatformWindowCreate( imapp->platform, parameters );
	if( !window )
	{
		return NULL;
	}

	ImappContextWindowInfo* windowInfo = &imapp->windows[ imapp->windowsCount++ ];
	windowInfo->window		= window;
	windowInfo->uiFunc		= uiFunc;
	windowInfo->uiContext	= uiContext;

	windowInfo->clearColor[ 0 ]	= (float)parameters->clearColor.red / 255.0f;
	windowInfo->clearColor[ 1 ]	= (float)parameters->clearColor.green / 255.0f;
	windowInfo->clearColor[ 2 ]	= (float)parameters->clearColor.blue / 255.0f;
	windowInfo->clearColor[ 3 ]	= (float)parameters->clearColor.alpha / 255.0f;

	return window;
}

void imappWindowDestroy( ImappContext* imapp, ImappWindow* window )
{
	ImappContextWindowInfo* windowInfo = NULL;
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

static void imappWindowDestroyInternal( ImappContext* imapp, ImappContextWindowInfo* windowInfo, uintsize windowIndex )
{
	imappRendererDestructWindow( imapp->renderer, &windowInfo->rendererWindow );
	imappPlatformWindowDestroy( windowInfo->window );

	IMUI_MEMORY_ARRAY_REMOVE_UNSORTED_ZERO( imapp->windows, imapp->windowsCount, windowIndex );

	if( imapp->windowsCount == 0 )
	{
		imapp->running = false;
	}
}

bool imappWindowIsOpen( const ImappContext* imapp, const ImappWindow* window )
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

bool imappWindowHasFocus( const ImappWindow* window )
{
	return imappPlatformWindowHasFocus( window );
}

void imappWindowGetPosition( const ImappWindow* window, int* outX, int* outY )
{
	imappPlatformWindowGetPosition( window, outX, outY );
}

void imappWindowSetPosition( ImappWindow* window, int x, int y )
{
	imappPlatformWindowSetPosition( window, x, y );
}

void imappWindowGetViewRect( const ImappWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	imappPlatformWindowGetViewRect( window, outX, outY, outWidth, outHeight );
}

ImappWindowStyle imappWindowGetStyle( const ImappWindow* window )
{
	return imappPlatformWindowGetStyle( window );
}

ImappWindowState imappWindowGetState( const ImappWindow* window )
{
	return imappPlatformWindowGetState( window );
}

void imappWindowSetState( ImappWindow* window, ImappWindowState state )
{
	imappPlatformWindowSetState( window, state );
}

bool imappWindowPopDropData( ImappWindow* window, ImappDropData* outData )
{
	return imappPlatformWindowPopDropData( window, outData );
}

ImuiContext* imappGetUi( ImappContext* imapp )
{
	return imapp->imui;
}

void imappQuit( ImappContext* imapp, int exitCode )
{
#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	emscripten_cancel_main_loop();
#endif
	imapp->running	= false;
	imapp->exitCode	= exitCode;
}

ImuiFont* imappGetDefaultFont( const ImappContext* imapp )
{
	return imapp->defaultFont ? imapp->defaultFont->uiFont : NULL;
}

ImappResPak* imappResourceGetDefaultPak( ImappContext* imapp )
{
	return imapp->defaultResPak;
}

ImappResPak* imappResourceAddMemoryPak( ImappContext* imapp, const void* pakData, size_t dataLength )
{
	return imappResSysAdd( imapp->ressys, pakData, dataLength );
}

ImappResPak* imappResourceOpenPak( ImappContext* imapp, const char* resourcePath )
{
	return imappResSysOpen( imapp->ressys, resourcePath );
}

void imappResourceClosePak( ImappContext* imapp, ImappResPak* pak )
{
	imappResSysClose( imapp->ressys, pak );
}

void imappResPakActivateTheme( ImappContext* imapp, ImappResPak* pak, const char* name )
{
	const ImappTheme* theme = imappResPakGetTheme( pak, name );
	if( !theme )
	{
		return;
	}

	imuiToolboxThemeSet( &theme->uiTheme );
	imappWindowThemeSet( &theme->windowTheme );

	if( !theme->uiTheme.font &&
		imapp->defaultFont )
	{
		imuiToolboxThemeGet()->font = imapp->defaultFont->uiFont;
	}
}

ImappImage* imappImageLoadResource( ImappContext* imapp, const char* resourceName )
{
	ImappImage* pImage = imappResSysImageLoadResource( imapp->ressys, resourceName );
	if( pImage == NULL )
	{
		return NULL;
	}

	return pImage;
}

ImappImage* imappImageCreateRaw( ImappContext* imapp, const void* imageData, size_t imageDataSize, int width, int height )
{
	if( imageDataSize < (size_t)(width * height * 4) )
	{
		IMAPP_DEBUG_LOGW( "Insuficiant data to create image." );
		return NULL;
	}

	return imappResSysImageCreateRaw( imapp->ressys, imageData, width, height );
}

ImappImage* imappImageCreatePng( ImappContext* imapp, const void* imageData, size_t imageDataSize )
{
	return imappResSysImageCreatePng( imapp->ressys, imageData, imageDataSize );
}

ImappImage* imappImageCreateJpeg( ImappContext* imapp, const void* imageData, size_t imageDataSize )
{
	return imappResSysImageCreateJpeg( imapp->ressys, imageData, imageDataSize );
}

ImappResState imappImageGetState( ImappContext* imapp, ImappImage* image )
{
	return imappResSysImageGetState( imapp->ressys, image );
}

void imappImageFree( ImappContext* imapp, ImappImage* image )
{
	imappResSysImageFree( imapp->ressys, image );
}

ImuiImage imappImageGetImage( const ImappImage* image )
{
	ImuiImage result;
	if( image && image->state == ImappResState_Ready )
	{
		return image->uiImage;
	}
	else
	{
		memset( &result, 0, sizeof( result ) );
	}

	return result;
}
