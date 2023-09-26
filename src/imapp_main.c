#include "imapp_main.h"

#include "imapp_event.h"
#include "imapp_event_queue.h"
#include "imapp_resource_storage.h"
#include "imapp_internal.h"
#include "imapp_platform.h"
#include "imapp_renderer.h"

#include <limits.h>
#include <string.h>

static void		ImAppFillDefaultParameters( ImAppParameters* parameters );
static bool		ImAppInitialize( ImAppInternal* imapp, const ImAppParameters* parameters );
static void		ImAppCleanup( ImAppInternal* imapp );
static void		ImAppHandleEvents( ImAppInternal* imapp );
static void		ImAppUpdateInputModifer( ImAppInternal* imapp, const ImAppEvent* pEvent, ImUiInputKey key1, ImUiInputKey key2, uint8_t* pMask, uint32_t modifierFlag );
static void		ImAppUpdateInputModiferFlag( ImAppInternal* imapp, uint8_t* pTargetMask, uint8_t maskChange, bool set, uint32_t modifierFlag );

int ImAppMain( ImAppPlatform* platform, int argc, char* argv[] )
{
	ImAppInternal* imapp = NULL;
	int tickIntervalMs = 0;
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

		if( !ImAppPlatformInitialize( platform, &imapp->allocator ) )
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

		tickIntervalMs = parameters.tickIntervalMs;
	}

	int64_t lastTickValue = 0;
	while( imapp->running )
	{
		lastTickValue = ImAppPlatformWindowTick( imapp->window, lastTickValue, tickIntervalMs );

		ImAppResourceStorageUpdate( imapp->resources );
		ImAppHandleEvents( imapp );

		ImAppPlatformWindowGetViewRect( &imapp->context.x, &imapp->context.y, &imapp->context.width, &imapp->context.height, imapp->window );

		// UI
		const ImUiDrawData* drawData = NULL;
		{
			const ImUiSize size		= ImUiSizeCreate( (float)imapp->context.width, (float)imapp->context.height );

			ImUiFrame* frame		= ImUiBegin( imapp->context.imui, lastTickValue / 1000.0f );
			ImUiSurface* surface	= ImUiSurfaceBegin( frame, ImUiStringViewCreate( "default" ), size, 1.0f );
			ImUiWindow* window		= ImUiWindowBegin( surface, ImUiStringViewCreate( "default" ), ImUiRectCreateSize( 0.0f, 0.0f, size ), 0u );

			ImAppProgramDoDefaultWindowUi( &imapp->context, imapp->programContext, window );

			ImUiWindowEnd( window );
			drawData = ImUiSurfaceEnd( surface );
			ImUiEnd( frame );
		}

		ImAppRendererDraw( imapp->renderer, imapp->window, drawData );

		if( !ImAppPlatformWindowPresent( imapp->window ) )
		{
			if( !ImAppRendererRecreateResources( imapp->renderer ) )
			{
				imapp->running = false;
				break;
			}

			if( !ImAppResourceStorageRecreateEverything( imapp->resources ) )
			{
				imapp->running = false;
				break;
			}
		}
	}

	ImAppCleanup( imapp );
	return 0;
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
	parameters->defaultFontName		= ImUiStringViewCreate( "arial.ttf" );
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

	if( parameters->defaultFontName.length > 0u )
	{
		imapp->defaultFont = ImAppResourceStorageFontCreate( imapp->resources, parameters->defaultFontName, parameters->defaultFontSize );

		ImUiToolboxConfig toolboxConfig;
		ImUiToolboxFillDefaultConfig( &toolboxConfig, imapp->defaultFont );
		ImUiToolboxSetConfig( &toolboxConfig );
	}

	//{
	//	struct nk_allocator allocator	= ImAppAllocatorGetNuklear( &imapp->parameters.allocator );
	//	struct nk_font* pFont			= ImAppRendererCreateDefaultFont( imapp->renderer );
	//	nk_init( &imapp->nkContext, &allocator, &pFont->handle );
	//}

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
			break;

		case ImAppEventType_KeyUp:
			ImUiInputPushKeyUp( input, windowEvent.key.key );
			break;

			//{
			//	const bool down = windowEvent.type == ImAppEventType_KeyDown;

			//	ImAppUpdateInputModifer( imapp, &windowEvent, ImUiInputKey_LeftShift, ImUiInputKey_RightShift, &imapp->inputMaskShift, ImAppInputModifier_Shift );
			//	ImAppUpdateInputModifer( imapp, &windowEvent, ImUiInputKey_LeftControl, ImUiInputKey_RightControl, &imapp->inputMaskControl, ImAppInputModifier_Ctrl );
			//	ImAppUpdateInputModifer( imapp, &windowEvent, ImUiInputKey_LeftAlt, ImUiInputKey_None, &imapp->inputMaskAlt, ImAppInputModifier_Alt );

			//	//if( windowEvent.key.key == ImUiInputKey_RightAlt )
			//	//{
			//	//	const uint8_t mask = 0x4u;
			//	//	ImAppUpdateInputModiferFlag( imapp, &imapp->inputMaskControl, mask, down, ImAppInputModifier_Ctrl );
			//	//	ImAppUpdateInputModiferFlag( imapp, &imapp->inputMaskAlt, mask, down, ImAppInputModifier_Alt );
			//	//}

			//	//for( size_t i = 0u; i < imapp->parameters.shortcutsLength; ++i )
			//	//{
			//	//	const ImAppInputShortcut* pShortcut = &imapp->parameters.shortcuts[ i ];
			//	//	const uint32_t nkMask	= 1u << pShortcut->nkKey;
			//	//	const bool nkDown		= (imapp->inputDownMask & nkMask) != 0u;
			//	//	if( pShortcut->key != windowEvent.key.key )
			//	//	{
			//	//		continue;
			//	//	}

			//	//	if( pShortcut->modifierMask != imapp->inputModifiers || !down )
			//	//	{
			//	//		continue;
			//	//	}

			//	//	if( !down && !nkDown )
			//	//	{
			//	//		continue;
			//	//	}

			//	//	imuiinputpush
			//	//	nk_input_key( &imapp->nkContext, pShortcut->nkKey, down );

			//	//	if( down )
			//	//	{
			//	//		imapp->inputDownMask |= nkMask;
			//	//	}
			//	//	else
			//	//	{
			//	//		imapp->inputDownMask &= ~nkMask;
			//	//	}
			//	//}
			//}
			//break;

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
			//{
			//	ImUiInputMouseButton button;
			//	switch( windowEvent.button.button )
			//	{
			//	case ImAppInputButton_Left:		button = ImUiInputMouseButton_Left; break;
			//	case ImAppInputButton_Middle:	button = ImUiInputMouseButton_Middle; break;
			//	case ImAppInputButton_Right:	button = ImUiInputMouseButton_Right; break;
			//	case ImAppInputButton_Middle:	button = ImUiInputMouseButton_Middle; break;
			//	case ImAppInputButton_Right:	button = ImUiInputMouseButton_Right; break;

			//	default:
			//		continue;
			//	}

			//	const nk_bool down = windowEvent.type == ImAppEventType_ButtonDown;
			//	nk_input_button( &imapp->nkContext, button, windowEvent.button.x, windowEvent.button.y, down );
			//}
			break;

		case ImAppEventType_Scroll:
			ImUiInputPushMouseScroll( input, (float)windowEvent.scroll.x, (float)windowEvent.scroll.y );
			break;
		}
	}

	ImUiInputEnd( imapp->context.imui );
}

static void ImAppUpdateInputModifer( ImAppInternal* imapp, const ImAppEvent* pEvent, ImUiInputKey key1, ImUiInputKey key2, uint8_t* pMask, uint32_t modifierFlag )
{
	if( pEvent->key.key != key1 &&
		pEvent->key.key != key2 )
	{
		return;
	}

	const uint8_t mask = pEvent->key.key == key1 ? 0x1u : 0x2u;
	const bool set = pEvent->type == ImAppEventType_KeyDown;
	ImAppUpdateInputModiferFlag( imapp, pMask, mask, set, modifierFlag );
}

static void ImAppUpdateInputModiferFlag( ImAppInternal* imapp, uint8_t* pTargetMask, uint8_t maskChange, bool set, uint32_t modifierFlag )
{
	if( set )
	{
		*pTargetMask |= maskChange;
	}
	else
	{
		*pTargetMask &= ~maskChange;
	}

	if( *pTargetMask )
	{
		imapp->inputModifiers |= modifierFlag;
	}
	else
	{
		imapp->inputModifiers &= ~modifierFlag;
	}
}
