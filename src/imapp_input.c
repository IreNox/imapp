#include "imapp_input.h"

#include "imapp/imapp.h"

#include <SDL.h>

struct ImAppInput
{
	SDL_Window*			pSdlWindow;
	struct nk_context*	pNkContext;
	
	enum nk_keys		keyMapping[ SDL_NUM_SCANCODES ];
};

ImAppInput* ImAppInputCreate( SDL_Window* pSdlWindow, struct nk_context* pNkContext )
{
	ImAppInput* pInput = IMAPP_NEW_ZERO( ImAppInput );
	if( pInput == NULL )
	{
		return NULL;
	}

	pInput->pSdlWindow	= pSdlWindow;
	pInput->pNkContext	= pNkContext;

	return pInput;
}

void ImAppInputDestroy( ImAppInput* pInput )
{
	ImAppFree( pInput );
}

void ImAppInputBegin( ImAppInput* pInput )
{
	nk_input_begin( pInput->pNkContext );
}

void ImAppInputHandleEvent( ImAppInput* pInput, const SDL_Event* pSdlEvent )
{
	switch( pSdlEvent->type )
	{
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		{
			const SDL_KeyboardEvent* pKeyEvent = &pSdlEvent->key;

			const enum nk_keys nkKey = pInput->keyMapping[ pKeyEvent->keysym.scancode ];
			if( nkKey != NK_KEY_NONE )
			{
				nk_input_key( pInput->pNkContext, nkKey, pKeyEvent->type == SDL_KEYDOWN );
			}

			if( pKeyEvent->state == SDL_PRESSED &&
				pKeyEvent->keysym.sym >= ' ' &&
				pKeyEvent->keysym.sym <= '~' )
			{
				nk_input_char( pInput->pNkContext, (char)pKeyEvent->keysym.sym );
			}
		}
		break;

	case SDL_TEXTEDITING:
	case SDL_TEXTINPUT:
		break;

	case SDL_MOUSEMOTION:
		{
			const SDL_MouseMotionEvent* pMotionEvent = &pSdlEvent->motion;
			nk_input_motion( pInput->pNkContext, pMotionEvent->x, pMotionEvent->y );
		}
		break;

	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		{
			const SDL_MouseButtonEvent* pButtonEvent = &pSdlEvent->button;
			
			enum nk_buttons button;
			if( pButtonEvent->button == SDL_BUTTON_LEFT &&
				pButtonEvent->clicks == 2u )
			{
				button = NK_BUTTON_DOUBLE;
			}
			else if( pButtonEvent->button >= SDL_BUTTON_LEFT &&
				pButtonEvent->button <= SDL_BUTTON_RIGHT )
			{
				button = (enum nk_buttons)(pButtonEvent->button - 1 );
			}
			else
			{
				return;
			}

			const nk_bool down = pButtonEvent->type == SDL_MOUSEBUTTONDOWN;
			nk_input_button( pInput->pNkContext, button, pButtonEvent->x, pButtonEvent->y, down );
		}
		break;

	case SDL_MOUSEWHEEL:
		{
			const SDL_MouseWheelEvent* pWheelEvent = &pSdlEvent->wheel;
			nk_input_scroll( pInput->pNkContext, nk_vec2i( pWheelEvent->x, pWheelEvent->y ) );
		}
		break;

	default:
		break;
	}
}

void ImAppInputEnd( ImAppInput* pInput )
{
	nk_input_end( pInput->pNkContext );
}
