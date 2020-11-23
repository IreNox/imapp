#include "../imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_SDL )

#include <SDL.h>

//////////////////////////////////////////////////////////////////////////
// Main

struct ImAppPlatform
{
	int temp;
};

int SDL_main( int argc, char* argv[] )
{
	ImAppPlatform platform;

	if( SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	{
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to initialize SDL.", NULL );
		return 1;
	}

	const int result = ImAppMain( &platform, argc, argv );

	SDL_Quit();

	return result;
}

void ImAppShowError( ImAppPlatform* pPlatform, const char* pMessage )
{
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", pMessage, NULL );
}

//////////////////////////////////////////////////////////////////////////
// Shared Libraries

ImAppSharedLibHandle ImAppSharedLibOpen( const char* pSharedLibName )
{
	return (ImAppSharedLibHandle)SDL_LoadObject( pSharedLibName );
}

void ImAppSharedLibClose( ImAppSharedLibHandle libHandle )
{
	SDL_UnloadObject( libHandle );
}

void* ImAppSharedLibGetFunction( ImAppSharedLibHandle libHandle, const char* pFunctionName )
{
	return SDL_LoadFunction( libHandle, pFunctionName );
}

//////////////////////////////////////////////////////////////////////////
// Window

struct ImAppWindow
{
	SDL_Window*			pSdlWindow;
	SDL_GLContext		pGlContext;

	bool				isOpen;
};

ImAppWindow* ImAppWindowCreate( ImAppPlatform* pPlatform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* pWindow = IMAPP_NEW_ZERO( ImAppWindow );
	if( pWindow == NULL )
	{
		return NULL;
	}

	Uint32 flags = SDL_WINDOW_OPENGL;

	switch( state )
	{
	case ImAppWindowState_Default:
		flags |= SDL_WINDOW_SHOWN;
		break;

	case ImAppWindowState_Minimized:
		flags |= SDL_WINDOW_MINIMIZED;
		break;

	case ImAppWindowState_Maximized:
		flags |= SDL_WINDOW_MAXIMIZED;
		break;
	}

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	flags |= SDL_WINDOW_RESIZABLE;
#endif

	pWindow->pSdlWindow = SDL_CreateWindow( pWindowTitle, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags );
	if( pWindow->pSdlWindow == NULL )
	{
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to create Window.", NULL );

		ImAppFree( pWindow );
		return NULL;
	}


#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
#else
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
#endif

	pWindow->pGlContext = SDL_GL_CreateContext( pWindow->pSdlWindow );
	if( pWindow->pGlContext == NULL )
	{
		ImAppWindowDestroy( pWindow );
		return NULL;
	}

	if( SDL_GL_SetSwapInterval( 1 ) < 0 )
	{
		ImAppTrace( "[renderer] Unable to set VSync! SDL Error: %s\n", SDL_GetError() );
	}

	pWindow->isOpen = true;

	return pWindow;
}

void ImAppWindowDestroy( ImAppWindow* pWindow )
{
	if( pWindow->pGlContext != NULL )
	{
		SDL_GL_DeleteContext( pWindow->pGlContext );
		pWindow->pGlContext = NULL;
	}

	if( pWindow->pSdlWindow != NULL )
	{
		SDL_DestroyWindow( pWindow->pSdlWindow );
		pWindow->pSdlWindow = NULL;
	}

	ImAppFree( pWindow );
}

int64_t ImAppWindowTick( ImAppWindow* pWindow, int64_t lastTickValue, int64_t tickInterval )
{
	const int64_t nextTick = (int64_t)SDL_GetTicks();

	int64_t timeToWait = tickInterval - (nextTick - lastTickValue);
	timeToWait = timeToWait < 0 ? 0 : timeToWait;

	if( tickInterval == 0 )
	{
		timeToWait = INT_MAX;
	}

	SDL_WaitEventTimeout( NULL, (int)timeToWait );

	return (int64_t)nextTick;
}

bool ImAppWindowPresent( ImAppWindow* pWindow )
{
	SDL_GL_SwapWindow( pWindow->pSdlWindow );
	return true;
}

bool ImAppWindowIsOpen( ImAppWindow* pWindow )
{
	return pWindow->isOpen;
}

void ImAppWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	SDL_GetWindowSize( pWindow->pSdlWindow, pWidth, pHeight );
}

void ImAppWindowGetPosition( int* pX, int* pY, ImAppWindow* pWindow )
{
	SDL_GetWindowPosition( pWindow->pSdlWindow, pX, pY );
}

ImAppWindowState ImAppWindowGetState( ImAppWindow* pWindow )
{
	const Uint32 flags = SDL_GetWindowFlags( pWindow->pSdlWindow );
	if( flags & SDL_WINDOW_MINIMIZED )
	{
		return ImAppWindowState_Minimized;
	}
	if( flags & SDL_WINDOW_MAXIMIZED )
	{
		return ImAppWindowState_Maximized;
	}

	return ImAppWindowState_Default;
}

//////////////////////////////////////////////////////////////////////////
// Input

struct ImAppInput
{
	ImAppWindow*	pWindow;

	enum nk_keys	keyMapping[ SDL_NUM_SCANCODES ];
};

ImAppInput* ImAppInputCreate( ImAppPlatform* pPlatform, ImAppWindow* pWindow )
{
	ImAppInput* pInput = IMAPP_NEW_ZERO( ImAppInput );
	if( pInput == NULL )
	{
		return NULL;
	}

	pInput->pWindow = pWindow;

	return pInput;
}

void ImAppInputDestroy( ImAppInput* pInput )
{
	ImAppFree( pInput );
}

void ImAppInputApply( ImAppInput* pInput, struct nk_context* pNkContext )
{
	SDL_Event sdlEvent;
	while( SDL_PollEvent( &sdlEvent ) )
	{
		switch( sdlEvent.type )
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				const SDL_KeyboardEvent* pKeyEvent = &sdlEvent.key;

				const enum nk_keys nkKey = pInput->keyMapping[ pKeyEvent->keysym.scancode ];
				if( nkKey != NK_KEY_NONE )
				{
					nk_input_key( pNkContext, nkKey, pKeyEvent->type == SDL_KEYDOWN );
				}

				if( pKeyEvent->state == SDL_PRESSED &&
					pKeyEvent->keysym.sym >= ' ' &&
					pKeyEvent->keysym.sym <= '~' )
				{
					nk_input_char( pNkContext, (char)pKeyEvent->keysym.sym );
				}
			}
			break;

		case SDL_TEXTEDITING:
		case SDL_TEXTINPUT:
			break;

		case SDL_MOUSEMOTION:
			{
				const SDL_MouseMotionEvent* pMotionEvent = &sdlEvent.motion;
				nk_input_motion( pNkContext, pMotionEvent->x, pMotionEvent->y );
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			{
				const SDL_MouseButtonEvent* pButtonEvent = &sdlEvent.button;

				enum nk_buttons button;
				if( pButtonEvent->button == SDL_BUTTON_LEFT &&
					pButtonEvent->clicks == 2u )
				{
					button = NK_BUTTON_DOUBLE;
				}
				else if( pButtonEvent->button >= SDL_BUTTON_LEFT &&
					pButtonEvent->button <= SDL_BUTTON_RIGHT )
				{
					button = (enum nk_buttons)(pButtonEvent->button - 1);
				}
				else
				{
					return;
				}

				const nk_bool down = pButtonEvent->type == SDL_MOUSEBUTTONDOWN;
				nk_input_button( pNkContext, button, pButtonEvent->x, pButtonEvent->y, down );
			}
			break;

		case SDL_MOUSEWHEEL:
			{
				const SDL_MouseWheelEvent* pWheelEvent = &sdlEvent.wheel;
				nk_input_scroll( pNkContext, nk_vec2i( pWheelEvent->x, pWheelEvent->y ) );
			}
			break;

		case SDL_WINDOWEVENT:
			{
				const SDL_WindowEvent* pWindowEvent = &sdlEvent.window;
				switch( pWindowEvent->event )
				{
				case SDL_WINDOWEVENT_CLOSE:
					pInput->pWindow->isOpen = false;
					break;

				default:
					break;
				}
			}
			break;

		default:
			break;
		}
	}
}

#endif
