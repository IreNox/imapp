#include "../imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_SDL )

#include <SDL.h>

//////////////////////////////////////////////////////////////////////////
// Main

struct ImAppPlatform
{
	ImAppInputKey	inputKeyMapping[ SDL_NUM_SCANCODES ];
};

int SDL_main( int argc, char* argv[] )
{
	ImAppPlatform platform = { 0 };

	if( SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	{
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to initialize SDL.", NULL );
		return 1;
	}

	for( size_t i = 0u; i < ImAppInputKey_MAX; ++i )
	{
		const enum ImAppInputKey keyValue = (ImAppInputKey)i;

		SDL_Scancode scanCode = SDL_SCANCODE_UNKNOWN;
		switch( keyValue )
		{
		case ImAppInputKey_None:			scanCode = SDL_SCANCODE_UNKNOWN; break;
		case ImAppInputKey_A:				scanCode = SDL_SCANCODE_A; break;
		case ImAppInputKey_B:				scanCode = SDL_SCANCODE_B; break;
		case ImAppInputKey_C:				scanCode = SDL_SCANCODE_C; break;
		case ImAppInputKey_D:				scanCode = SDL_SCANCODE_D; break;
		case ImAppInputKey_E:				scanCode = SDL_SCANCODE_E; break;
		case ImAppInputKey_F:				scanCode = SDL_SCANCODE_F; break;
		case ImAppInputKey_G:				scanCode = SDL_SCANCODE_G; break;
		case ImAppInputKey_H:				scanCode = SDL_SCANCODE_H; break;
		case ImAppInputKey_I:				scanCode = SDL_SCANCODE_I; break;
		case ImAppInputKey_J:				scanCode = SDL_SCANCODE_J; break;
		case ImAppInputKey_K:				scanCode = SDL_SCANCODE_K; break;
		case ImAppInputKey_L:				scanCode = SDL_SCANCODE_L; break;
		case ImAppInputKey_M:				scanCode = SDL_SCANCODE_M; break;
		case ImAppInputKey_N:				scanCode = SDL_SCANCODE_N; break;
		case ImAppInputKey_O:				scanCode = SDL_SCANCODE_O; break;
		case ImAppInputKey_P:				scanCode = SDL_SCANCODE_P; break;
		case ImAppInputKey_Q:				scanCode = SDL_SCANCODE_Q; break;
		case ImAppInputKey_R:				scanCode = SDL_SCANCODE_R; break;
		case ImAppInputKey_S:				scanCode = SDL_SCANCODE_S; break;
		case ImAppInputKey_T:				scanCode = SDL_SCANCODE_T; break;
		case ImAppInputKey_U:				scanCode = SDL_SCANCODE_U; break;
		case ImAppInputKey_V:				scanCode = SDL_SCANCODE_V; break;
		case ImAppInputKey_W:				scanCode = SDL_SCANCODE_W; break;
		case ImAppInputKey_X:				scanCode = SDL_SCANCODE_X; break;
		case ImAppInputKey_Y:				scanCode = SDL_SCANCODE_Y; break;
		case ImAppInputKey_Z:				scanCode = SDL_SCANCODE_Z; break;
		case ImAppInputKey_1:				scanCode = SDL_SCANCODE_1; break;
		case ImAppInputKey_2:				scanCode = SDL_SCANCODE_2; break;
		case ImAppInputKey_3:				scanCode = SDL_SCANCODE_3; break;
		case ImAppInputKey_4:				scanCode = SDL_SCANCODE_4; break;
		case ImAppInputKey_5:				scanCode = SDL_SCANCODE_5; break;
		case ImAppInputKey_6:				scanCode = SDL_SCANCODE_6; break;
		case ImAppInputKey_7:				scanCode = SDL_SCANCODE_7; break;
		case ImAppInputKey_8:				scanCode = SDL_SCANCODE_8; break;
		case ImAppInputKey_9:				scanCode = SDL_SCANCODE_9; break;
		case ImAppInputKey_0:				scanCode = SDL_SCANCODE_0; break;
		case ImAppInputKey_Enter:			scanCode = SDL_SCANCODE_RETURN; break;
		case ImAppInputKey_Escape:			scanCode = SDL_SCANCODE_ESCAPE; break;
		case ImAppInputKey_Backspace:		scanCode = SDL_SCANCODE_BACKSPACE; break;
		case ImAppInputKey_Tab:				scanCode = SDL_SCANCODE_TAB; break;
		case ImAppInputKey_Space:			scanCode = SDL_SCANCODE_SPACE; break;
		case ImAppInputKey_LeftShift:		scanCode = SDL_SCANCODE_LSHIFT; break;
		case ImAppInputKey_RightShift:		scanCode = SDL_SCANCODE_RSHIFT; break;
		case ImAppInputKey_LeftControl:		scanCode = SDL_SCANCODE_LCTRL; break;
		case ImAppInputKey_RightControl:	scanCode = SDL_SCANCODE_RCTRL; break;
		case ImAppInputKey_LeftAlt:			scanCode = SDL_SCANCODE_LALT; break;
		case ImAppInputKey_RightAlt:		scanCode = SDL_SCANCODE_RALT; break;
		case ImAppInputKey_Minus:			scanCode = SDL_SCANCODE_MINUS; break;
		case ImAppInputKey_Equals:			scanCode = SDL_SCANCODE_EQUALS; break;
		case ImAppInputKey_LeftBracket:		scanCode = SDL_SCANCODE_LEFTBRACKET; break;
		case ImAppInputKey_RightBracket:	scanCode = SDL_SCANCODE_RIGHTBRACKET; break;
		case ImAppInputKey_Backslash:		scanCode = SDL_SCANCODE_BACKSLASH; break;
		case ImAppInputKey_Semicolon:		scanCode = SDL_SCANCODE_SEMICOLON; break;
		case ImAppInputKey_Apostrophe:		scanCode = SDL_SCANCODE_APOSTROPHE; break;
		case ImAppInputKey_Grave:			scanCode = SDL_SCANCODE_GRAVE; break;
		case ImAppInputKey_Comma:			scanCode = SDL_SCANCODE_COMMA; break;
		case ImAppInputKey_Period:			scanCode = SDL_SCANCODE_PERIOD; break;
		case ImAppInputKey_Slash:			scanCode = SDL_SCANCODE_SLASH; break;
		case ImAppInputKey_F1:				scanCode = SDL_SCANCODE_F1; break;
		case ImAppInputKey_F2:				scanCode = SDL_SCANCODE_F2; break;
		case ImAppInputKey_F3:				scanCode = SDL_SCANCODE_F3; break;
		case ImAppInputKey_F4:				scanCode = SDL_SCANCODE_F4; break;
		case ImAppInputKey_F5:				scanCode = SDL_SCANCODE_F5; break;
		case ImAppInputKey_F6:				scanCode = SDL_SCANCODE_F6; break;
		case ImAppInputKey_F7:				scanCode = SDL_SCANCODE_F7; break;
		case ImAppInputKey_F8:				scanCode = SDL_SCANCODE_F8; break;
		case ImAppInputKey_F9:				scanCode = SDL_SCANCODE_F9; break;
		case ImAppInputKey_F10:				scanCode = SDL_SCANCODE_F10; break;
		case ImAppInputKey_F11:				scanCode = SDL_SCANCODE_F11; break;
		case ImAppInputKey_F12:				scanCode = SDL_SCANCODE_F12; break;
		case ImAppInputKey_Print:			scanCode = SDL_SCANCODE_PRINTSCREEN; break;
		case ImAppInputKey_Pause:			scanCode = SDL_SCANCODE_PAUSE; break;
		case ImAppInputKey_Insert:			scanCode = SDL_SCANCODE_INSERT; break;
		case ImAppInputKey_Delete:			scanCode = SDL_SCANCODE_DELETE; break;
		case ImAppInputKey_Home:			scanCode = SDL_SCANCODE_HOME; break;
		case ImAppInputKey_End:				scanCode = SDL_SCANCODE_END; break;
		case ImAppInputKey_PageUp:			scanCode = SDL_SCANCODE_PAGEUP; break;
		case ImAppInputKey_PageDown:		scanCode = SDL_SCANCODE_PAGEDOWN; break;
		case ImAppInputKey_Up:				scanCode = SDL_SCANCODE_UP; break;
		case ImAppInputKey_Left:			scanCode = SDL_SCANCODE_LEFT; break;
		case ImAppInputKey_Down:			scanCode = SDL_SCANCODE_DOWN; break;
		case ImAppInputKey_Right:			scanCode = SDL_SCANCODE_RIGHT; break;
		case ImAppInputKey_Numpad_Divide:	scanCode = SDL_SCANCODE_KP_DIVIDE; break;
		case ImAppInputKey_Numpad_Multiply:	scanCode = SDL_SCANCODE_KP_MULTIPLY; break;
		case ImAppInputKey_Numpad_Minus:	scanCode = SDL_SCANCODE_KP_MINUS; break;
		case ImAppInputKey_Numpad_Plus:		scanCode = SDL_SCANCODE_KP_PLUS; break;
		case ImAppInputKey_Numpad_Enter:	scanCode = SDL_SCANCODE_KP_ENTER; break;
		case ImAppInputKey_Numpad_1:		scanCode = SDL_SCANCODE_KP_1; break;
		case ImAppInputKey_Numpad_2:		scanCode = SDL_SCANCODE_KP_2; break;
		case ImAppInputKey_Numpad_3:		scanCode = SDL_SCANCODE_KP_3; break;
		case ImAppInputKey_Numpad_4:		scanCode = SDL_SCANCODE_KP_4; break;
		case ImAppInputKey_Numpad_5:		scanCode = SDL_SCANCODE_KP_5; break;
		case ImAppInputKey_Numpad_6:		scanCode = SDL_SCANCODE_KP_6; break;
		case ImAppInputKey_Numpad_7:		scanCode = SDL_SCANCODE_KP_7; break;
		case ImAppInputKey_Numpad_8:		scanCode = SDL_SCANCODE_KP_8; break;
		case ImAppInputKey_Numpad_9:		scanCode = SDL_SCANCODE_KP_9; break;
		case ImAppInputKey_Numpad_0:		scanCode = SDL_SCANCODE_KP_0; break;
		case ImAppInputKey_Numpad_Period:	scanCode = SDL_SCANCODE_KP_PERIOD; break;
		}

		platform.inputKeyMapping[ scanCode ] = keyValue;
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
	ImAppAllocator*		pAllocator;
	ImAppPlatform*		pPlatform;
	ImAppEventQueue*	pEventQueue;

	SDL_Window*			pSdlWindow;
	SDL_GLContext		pGlContext;
};

ImAppWindow* ImAppWindowCreate( ImAppAllocator* pAllocator, ImAppPlatform* pPlatform, const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* pWindow = IMAPP_NEW_ZERO( pAllocator, ImAppWindow );
	if( pWindow == NULL )
	{
		return NULL;
	}

	pWindow->pAllocator		= pAllocator;
	pWindow->pPlatform		= pPlatform;
	pWindow->pEventQueue	= ImAppEventQueueCreate( pAllocator );

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

		ImAppFree( pAllocator, pWindow );
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

	if( pWindow->pEventQueue != NULL )
	{
		ImAppEventQueueDestroy( pWindow->pEventQueue );
		pWindow->pEventQueue = NULL;
	}

	ImAppFree( pWindow->pAllocator, pWindow );
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

	SDL_Event sdlEvent;
	while( SDL_PollEvent( &sdlEvent ) )
	{
		switch( sdlEvent.type )
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				const SDL_KeyboardEvent* pKeyEvent = &sdlEvent.key;

				const ImAppInputKey mappedKey = pWindow->pPlatform->inputKeyMapping[ pKeyEvent->keysym.scancode ];
				if( mappedKey != ImAppInputKey_None )
				{
					const ImAppEventType eventType	= pKeyEvent->type == SDL_KEYDOWN ? ImAppEventType_KeyDown : ImAppEventType_KeyUp;
					const ImAppEvent keyEvent		= { .key = { .type = eventType, .key = mappedKey } };
					ImAppEventQueuePush( pWindow->pEventQueue, &keyEvent );
				}
			}
			break;

		case SDL_TEXTINPUT:
			{
				const SDL_TextInputEvent* pTextInputEvent = &sdlEvent.text;

				for( const char* pText = pTextInputEvent->text; *pText != '\0'; ++pText )
				{
					const ImAppEvent charEvent = { .character = { .type = ImAppEventType_Character, .character = *pText } };
					ImAppEventQueuePush( pWindow->pEventQueue, &charEvent );
				}
			}
			break;

		case SDL_MOUSEMOTION:
			{
				const SDL_MouseMotionEvent* pMotionEvent = &sdlEvent.motion;

				const ImAppEvent motionEvent = { .motion = { .type = ImAppEventType_Motion, .x = pMotionEvent->x, .y = pMotionEvent->y } };
				ImAppEventQueuePush( pWindow->pEventQueue, &motionEvent );
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			{
				const SDL_MouseButtonEvent* pButtonEvent = &sdlEvent.button;

				ImAppInputButton button;
				switch( pButtonEvent->button )
				{
				case SDL_BUTTON_LEFT:	button = ImAppInputButton_Left; break;
				case SDL_BUTTON_MIDDLE:	button = ImAppInputButton_Middle; break;
				case SDL_BUTTON_RIGHT:	button = ImAppInputButton_Right; break;
				case SDL_BUTTON_X1:		button = ImAppInputButton_X1; break;
				case SDL_BUTTON_X2:		button = ImAppInputButton_X2; break;

				default:
					continue;
				}

				const ImAppEventType eventType	= pButtonEvent->type == SDL_MOUSEBUTTONDOWN ? ImAppEventType_ButtonDown : ImAppEventType_ButtonUp;
				const ImAppEvent buttonEvent	= { .button = { .type = eventType, .x = pButtonEvent->x, .y = pButtonEvent->y, .button = button, .repeateCount = pButtonEvent->clicks } };
				ImAppEventQueuePush( pWindow->pEventQueue, &buttonEvent );
			}
			break;

		case SDL_MOUSEWHEEL:
			{
				const SDL_MouseWheelEvent* pWheelEvent = &sdlEvent.wheel;

				const ImAppEvent scrollEvent = { .scroll = { .type = ImAppEventType_Scroll, .x = pWheelEvent->x, .y = pWheelEvent->y } };
				ImAppEventQueuePush( pWindow->pEventQueue, &scrollEvent );
			}
			break;

		case SDL_WINDOWEVENT:
			{
				const SDL_WindowEvent* pWindowEvent = &sdlEvent.window;
				switch( pWindowEvent->event )
				{
				case SDL_WINDOWEVENT_CLOSE:
					{
						const ImAppEvent closeEvent = { .type = ImAppEventType_WindowClose };
						ImAppEventQueuePush( pWindow->pEventQueue, &closeEvent );
					}
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

	return (int64_t)nextTick;
}

bool ImAppWindowPresent( ImAppWindow* pWindow )
{
	SDL_GL_SwapWindow( pWindow->pSdlWindow );
	return true;
}

ImAppEventQueue* ImAppWindowGetEventQueue( ImAppWindow* pWindow )
{
	return pWindow->pEventQueue;
}

void ImAppWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	*pX = 0;
	*pY = 0;

	ImAppWindowGetSize( pWidth, pHeight, pWindow );
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

#endif

