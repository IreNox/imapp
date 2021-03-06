#include "imapp_main.h"

#include "imapp/imapp.h"

#include "imapp_defines.h"
#include "imapp_memory.h"
#include "imapp_renderer.h"

#include <limits.h>

static void		ImAppFillDefaultParameters( ImAppParameters* pParameters );
static bool		ImAppInitialize( ImApp* pImApp );
static void		ImAppCleanup( ImApp* pImApp );
static void		ImAppHandleEvents( ImApp* pImApp );
static void		ImAppUpdateInputModifer( ImApp* pImApp, const ImAppEvent* pEvent, ImAppInputKey key1, ImAppInputKey key2, uint8_t* pMask, uint32_t modifierFlag );
static void		ImAppUpdateInputModiferFlag( ImApp* pImApp, uint8_t* pTargetMask, uint8_t maskChange, bool set, uint32_t modifierFlag );

int ImAppMain( ImAppPlatform* pPlatform, int argc, char* argv[] )
{
	ImApp* pImApp = NULL;
	{
		ImAppParameters parameters = { 0 };
		ImAppFillDefaultParameters( &parameters );

		pImApp = IMAPP_NEW_ZERO( &parameters.allocator, ImApp );

		pImApp->running				= true;
		pImApp->pPlatform			= pPlatform;
		pImApp->context.nkContext	= &pImApp->nkContext;
		pImApp->parameters			= parameters;

		if( !ImAppInitialize( pImApp ) )
		{
			ImAppCleanup( pImApp );
			return 1;
		}
	}

	int64_t lastTickValue = 0;
	while( pImApp->running )
	{
		lastTickValue = ImAppWindowTick( pImApp->pWindow, lastTickValue, pImApp->parameters.tickIntervalMs );

		ImAppImageStorageUpdate( pImApp->pImages );

		nk_input_begin( &pImApp->nkContext );
		ImAppHandleEvents( pImApp );
		nk_input_end( &pImApp->nkContext );

		ImAppWindowGetViewRect( &pImApp->context.x, &pImApp->context.y, &pImApp->context.width, &pImApp->context.height, pImApp->pWindow );

		// UI
		{
			if( pImApp->parameters.defaultFullWindow )
			{
				nk_begin( &pImApp->nkContext, "Default", nk_recti( pImApp->context.x, pImApp->context.y, pImApp->context.width, pImApp->context.height ), NK_WINDOW_NO_SCROLLBAR );
			}

			ImAppProgramDoUi( &pImApp->context, pImApp->pProgramContext );

			if( pImApp->parameters.defaultFullWindow )
			{
				nk_end( &pImApp->nkContext );
			}
		}

		int screenWidth;
		int screenHeight;
		ImAppWindowGetSize( &screenWidth, &screenHeight, pImApp->pWindow );

		ImAppRendererDraw( pImApp->pRenderer, &pImApp->nkContext, screenWidth, screenHeight );
		if( !ImAppWindowPresent( pImApp->pWindow ) )
		{
			if( !ImAppRendererRecreateResources( pImApp->pRenderer ) )
			{
				pImApp->running = false;
			}
		}
	}

	ImAppCleanup( pImApp );
	return 0;
}

static const ImAppInputShortcut s_inputShortcuts[] =
{
	{ 0u,													ImAppInputKey_LeftShift,	NK_KEY_SHIFT },
	{ 0u,													ImAppInputKey_RightShift,	NK_KEY_SHIFT },
	{ 0u,													ImAppInputKey_LeftControl,	NK_KEY_CTRL },
	{ 0u,													ImAppInputKey_RightControl,	NK_KEY_CTRL },
	{ 0u,													ImAppInputKey_Delete,		NK_KEY_DEL },
	{ 0u,													ImAppInputKey_Enter,		NK_KEY_ENTER },
	{ 0u,													ImAppInputKey_Tab,			NK_KEY_TAB },
	{ 0u,													ImAppInputKey_Backspace,	NK_KEY_BACKSPACE },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_C,			NK_KEY_COPY },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_X,			NK_KEY_CUT },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_V,			NK_KEY_PASTE },
	{ 0u,													ImAppInputKey_Up,			NK_KEY_UP },
	{ 0u,													ImAppInputKey_Down,			NK_KEY_DOWN },
	{ 0u,													ImAppInputKey_Left,			NK_KEY_LEFT },
	{ 0u,													ImAppInputKey_Right,		NK_KEY_RIGHT },
																						/* Shortcuts: text field */
	{ 0u,													ImAppInputKey_Insert,		NK_KEY_TEXT_INSERT_MODE },
	{ 0u,													ImAppInputKey_None,			NK_KEY_TEXT_REPLACE_MODE },
	{ 0u,													ImAppInputKey_None,			NK_KEY_TEXT_RESET_MODE },
	{ 0u,													ImAppInputKey_Home,			NK_KEY_TEXT_LINE_START },
	{ 0u,													ImAppInputKey_End,			NK_KEY_TEXT_LINE_END },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Home,			NK_KEY_TEXT_START },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_End,			NK_KEY_TEXT_END },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Z,			NK_KEY_TEXT_UNDO },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Y,			NK_KEY_TEXT_REDO },
	{ ImAppInputModifier_Ctrl | ImAppInputModifier_Shift,	ImAppInputKey_Z,			NK_KEY_TEXT_REDO },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_A,			NK_KEY_TEXT_SELECT_ALL },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Left,			NK_KEY_TEXT_WORD_LEFT },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Right,		NK_KEY_TEXT_WORD_RIGHT },
																						/* Shortcuts: scrollbar */
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Home,			NK_KEY_SCROLL_START },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_End,			NK_KEY_SCROLL_END },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Down,			NK_KEY_SCROLL_DOWN },
	{ ImAppInputModifier_Ctrl,								ImAppInputKey_Up,			NK_KEY_SCROLL_UP },
};

static void ImAppFillDefaultParameters( ImAppParameters* pParameters )
{
	pParameters->allocator				= *ImAppAllocatorGetDefault();
	pParameters->tickIntervalMs			= 0;
	pParameters->defaultFullWindow		= true;
	pParameters->windowTitle			= "I'm App";
	pParameters->windowWidth			= 1280;
	pParameters->windowHeight			= 720;
	pParameters->shortcuts				= s_inputShortcuts;
	pParameters->shortcutsLength		= IMAPP_ARRAY_COUNT( s_inputShortcuts );
}

static bool ImAppInitialize( ImApp* pImApp )
{
	pImApp->pProgramContext = ImAppProgramInitialize( &pImApp->parameters );
	if( pImApp->pProgramContext == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to initialize Program." );
		return false;
	}

	pImApp->pWindow = ImAppWindowCreate( &pImApp->parameters.allocator, pImApp->pPlatform, pImApp->parameters.windowTitle, 0, 0, pImApp->parameters.windowWidth, pImApp->parameters.windowHeight, ImAppWindowState_Default );
	if( pImApp->pWindow == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to create Window." );
		return false;
	}

	pImApp->pRenderer = ImAppRendererCreate( &pImApp->parameters.allocator, pImApp->pPlatform, pImApp->pWindow );
	if( pImApp->pRenderer == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to create Renderer." );
		return false;
	}

	pImApp->pImages = ImAppImageStorageCreate( &pImApp->parameters.allocator, pImApp->pPlatform, pImApp->pRenderer );
	if( pImApp->pImages == NULL )
	{
		ImAppShowError( pImApp->pPlatform, "Failed to create Image Storage." );
		return false;
	}

	{
		struct nk_font* pFont = ImAppRendererCreateDefaultFont( pImApp->pRenderer, &pImApp->nkContext );
		nk_init_default( &pImApp->nkContext, &pFont->handle );
	}

	return true;
}

static void ImAppCleanup( ImApp* pImApp )
{
	if( pImApp->pProgramContext != NULL )
	{
		ImAppProgramShutdown( &pImApp->context, pImApp->pProgramContext );
		pImApp->pProgramContext = NULL;
	}

	if( pImApp->pImages != NULL )
	{
		ImAppImageStorageDestroy( pImApp->pImages );
		pImApp->pImages = NULL;
	}

	if( pImApp->pRenderer != NULL )
	{
		ImAppRendererDestroy( pImApp->pRenderer );
		pImApp->pRenderer = NULL;
	}

	if( pImApp->pWindow != NULL )
	{
		ImAppWindowDestroy( pImApp->pWindow );
		pImApp->pWindow = NULL;
	}

	ImAppFree( &pImApp->parameters.allocator, pImApp );
}

static void ImAppHandleEvents( ImApp* pImApp )
{
	ImAppEventQueue* pEventQueue = ImAppWindowGetEventQueue( pImApp->pWindow );

	ImAppEvent windowEvent;
	while( ImAppEventQueuePop( pEventQueue, &windowEvent ) )
	{
		switch( windowEvent.type )
		{
		case ImAppEventType_WindowClose:
			pImApp->running = false;
			break;

		case ImAppEventType_KeyDown:
		case ImAppEventType_KeyUp:
			{
				const bool down = windowEvent.type == ImAppEventType_KeyDown;

				ImAppUpdateInputModifer( pImApp, &windowEvent, ImAppInputKey_LeftShift, ImAppInputKey_RightShift, &pImApp->inputMaskShift, ImAppInputModifier_Shift );
				ImAppUpdateInputModifer( pImApp, &windowEvent, ImAppInputKey_LeftControl, ImAppInputKey_RightControl, &pImApp->inputMaskControl, ImAppInputModifier_Ctrl );
				ImAppUpdateInputModifer( pImApp, &windowEvent, ImAppInputKey_LeftAlt, ImAppInputKey_None, &pImApp->inputMaskAlt, ImAppInputModifier_Alt );

				if( windowEvent.key.key == ImAppInputKey_RightAlt )
				{
					const uint8_t mask = 0x4u;
					ImAppUpdateInputModiferFlag( pImApp, &pImApp->inputMaskControl, mask, down, ImAppInputModifier_Ctrl );
					ImAppUpdateInputModiferFlag( pImApp, &pImApp->inputMaskAlt, mask, down, ImAppInputModifier_Alt );
				}

				for( size_t i = 0u; i < pImApp->parameters.shortcutsLength; ++i )
				{
					const ImAppInputShortcut* pShortcut = &pImApp->parameters.shortcuts[ i ];
					const uint32_t nkMask	= 1u << pShortcut->nkKey;
					const bool nkDown		= (pImApp->inputDownMask & nkMask) != 0u;
					if( pShortcut->key != windowEvent.key.key )
					{
						continue;
					}

					if( pShortcut->modifierMask != pImApp->inputModifiers || !down )
					{
						continue;
					}

					if( !down && !nkDown )
					{
						continue;
					}

					nk_input_key( &pImApp->nkContext, pShortcut->nkKey, down );

					if( down )
					{
						pImApp->inputDownMask |= nkMask;
					}
					else
					{
						pImApp->inputDownMask &= ~nkMask;
					}
				}
			}
			break;

		case ImAppEventType_Character:
			nk_input_char( &pImApp->nkContext, windowEvent.character.character );
			break;

		case ImAppEventType_Motion:
			nk_input_motion( &pImApp->nkContext, windowEvent.motion.x, windowEvent.motion.y );
			break;

		case ImAppEventType_ButtonDown:
		case ImAppEventType_ButtonUp:
			{
				enum nk_buttons button;
				if( windowEvent.button.button == ImAppInputButton_Left &&
					windowEvent.button.repeateCount == 2u )
				{
					button = NK_BUTTON_DOUBLE;
				}
				else
				{
					switch( windowEvent.button.button )
					{
					case ImAppInputButton_Left:		button = NK_BUTTON_LEFT; break;
					case ImAppInputButton_Middle:	button = NK_BUTTON_MIDDLE; break;
					case ImAppInputButton_Right:	button = NK_BUTTON_RIGHT; break;

					default:
						continue;
					}
				}

				const nk_bool down = windowEvent.type == ImAppEventType_ButtonDown;
				nk_input_button( &pImApp->nkContext, button, windowEvent.button.x, windowEvent.button.y, down );
			}
			break;

		case ImAppEventType_Scroll:
			nk_input_scroll( &pImApp->nkContext, nk_vec2i( windowEvent.scroll.x, windowEvent.scroll.y ) );
			break;
		}
	}
}

static void ImAppUpdateInputModifer( ImApp* pImApp, const ImAppEvent* pEvent, ImAppInputKey key1, ImAppInputKey key2, uint8_t* pMask, uint32_t modifierFlag )
{
	if( pEvent->key.key != key1 &&
		pEvent->key.key != key2 )
	{
		return;
	}

	const uint8_t mask = pEvent->key.key == key1 ? 0x1u : 0x2u;
	const bool set = pEvent->type == ImAppEventType_KeyDown;
	ImAppUpdateInputModiferFlag( pImApp, pMask, mask, set, modifierFlag );
}

static void ImAppUpdateInputModiferFlag( ImApp* pImApp, uint8_t* pTargetMask, uint8_t maskChange, bool set, uint32_t modifierFlag )
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
		pImApp->inputModifiers |= modifierFlag;
	}
	else
	{
		pImApp->inputModifiers &= ~modifierFlag;
	}
}
