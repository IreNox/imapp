#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_LINUX )

#include "imapp_debug.h"
#include "imapp_drop_queue.h"
#include "imapp_event_queue.h"
#include "imapp_internal.h"


//////////////////////////////////////////////////////////////////////////
// Main

struct ImAppPlatform
{
	ImUiAllocator*	allocator;

	ImUiInputKey	inputKeyMapping[ SDL_NUM_SCANCODES ];

	char*			resourceBasePath;
	size_t			resourceBasePathLength;

	char*			fontBasePath;
	size_t			fontBasePathLength;

	SDL_Cursor*		systemCursors[ ImUiInputMouseCursor_MAX ];
};

struct ImAppWindow
{
	ImUiAllocator*		allocator;
	ImAppPlatform*		platform;
	ImAppEventQueue		eventQueue;

	SDL_Window*			sdlWindow;
	SDL_GLContext		glContext;

	ImAppWindowDropQueue	newDrops;
	ImAppWindowDropQueue	poppedDrops;
};

struct ImAppThread
{
	ImAppPlatform*	platform;

	SDL_Thread*		sdlThread;

	ImAppThreadFunc	func;
	void*			arg;

	SDL_atomic_t	isRunning;
};

static int SDLCALL	ImAppPlatformThreadEntry( void* voidThread );

// static const SDL_SystemCursor s_sdlSystemCursorMapping[] =
// {
// 	SDL_SYSTEM_CURSOR_ARROW,
// 	SDL_SYSTEM_CURSOR_WAIT,
// 	SDL_SYSTEM_CURSOR_WAITARROW,
// 	SDL_SYSTEM_CURSOR_IBEAM,
// 	SDL_SYSTEM_CURSOR_CROSSHAIR,
// 	SDL_SYSTEM_CURSOR_HAND,
// 	SDL_SYSTEM_CURSOR_SIZENWSE,
// 	SDL_SYSTEM_CURSOR_SIZENESW,
// 	SDL_SYSTEM_CURSOR_SIZEWE,
// 	SDL_SYSTEM_CURSOR_SIZENS,
// 	SDL_SYSTEM_CURSOR_SIZEALL
// };
// static_assert( IMAPP_ARRAY_COUNT( s_sdlSystemCursorMapping ) == ImUiInputMouseCursor_MAX, "more cursors" );

int main( int argc, char* argv[] )
{
	ImAppPlatform platform = { 0 };

	// if( SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	// {
	// 	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to initialize SDL.", NULL );
	// 	return 1;
	// }

	// for( size_t i = 0u; i < ImUiInputKey_MAX; ++i )
	// {
	// 	const ImUiInputKey keyValue = (ImUiInputKey)i;

	// 	SDL_Scancode scanCode = SDL_SCANCODE_UNKNOWN;
	// 	switch( keyValue )
	// 	{
	// 	case ImUiInputKey_None:				scanCode = SDL_SCANCODE_UNKNOWN; break;
	// 	case ImUiInputKey_A:				scanCode = SDL_SCANCODE_A; break;
	// 	case ImUiInputKey_B:				scanCode = SDL_SCANCODE_B; break;
	// 	case ImUiInputKey_C:				scanCode = SDL_SCANCODE_C; break;
	// 	case ImUiInputKey_D:				scanCode = SDL_SCANCODE_D; break;
	// 	case ImUiInputKey_E:				scanCode = SDL_SCANCODE_E; break;
	// 	case ImUiInputKey_F:				scanCode = SDL_SCANCODE_F; break;
	// 	case ImUiInputKey_G:				scanCode = SDL_SCANCODE_G; break;
	// 	case ImUiInputKey_H:				scanCode = SDL_SCANCODE_H; break;
	// 	case ImUiInputKey_I:				scanCode = SDL_SCANCODE_I; break;
	// 	case ImUiInputKey_J:				scanCode = SDL_SCANCODE_J; break;
	// 	case ImUiInputKey_K:				scanCode = SDL_SCANCODE_K; break;
	// 	case ImUiInputKey_L:				scanCode = SDL_SCANCODE_L; break;
	// 	case ImUiInputKey_M:				scanCode = SDL_SCANCODE_M; break;
	// 	case ImUiInputKey_N:				scanCode = SDL_SCANCODE_N; break;
	// 	case ImUiInputKey_O:				scanCode = SDL_SCANCODE_O; break;
	// 	case ImUiInputKey_P:				scanCode = SDL_SCANCODE_P; break;
	// 	case ImUiInputKey_Q:				scanCode = SDL_SCANCODE_Q; break;
	// 	case ImUiInputKey_R:				scanCode = SDL_SCANCODE_R; break;
	// 	case ImUiInputKey_S:				scanCode = SDL_SCANCODE_S; break;
	// 	case ImUiInputKey_T:				scanCode = SDL_SCANCODE_T; break;
	// 	case ImUiInputKey_U:				scanCode = SDL_SCANCODE_U; break;
	// 	case ImUiInputKey_V:				scanCode = SDL_SCANCODE_V; break;
	// 	case ImUiInputKey_W:				scanCode = SDL_SCANCODE_W; break;
	// 	case ImUiInputKey_X:				scanCode = SDL_SCANCODE_X; break;
	// 	case ImUiInputKey_Y:				scanCode = SDL_SCANCODE_Y; break;
	// 	case ImUiInputKey_Z:				scanCode = SDL_SCANCODE_Z; break;
	// 	case ImUiInputKey_1:				scanCode = SDL_SCANCODE_1; break;
	// 	case ImUiInputKey_2:				scanCode = SDL_SCANCODE_2; break;
	// 	case ImUiInputKey_3:				scanCode = SDL_SCANCODE_3; break;
	// 	case ImUiInputKey_4:				scanCode = SDL_SCANCODE_4; break;
	// 	case ImUiInputKey_5:				scanCode = SDL_SCANCODE_5; break;
	// 	case ImUiInputKey_6:				scanCode = SDL_SCANCODE_6; break;
	// 	case ImUiInputKey_7:				scanCode = SDL_SCANCODE_7; break;
	// 	case ImUiInputKey_8:				scanCode = SDL_SCANCODE_8; break;
	// 	case ImUiInputKey_9:				scanCode = SDL_SCANCODE_9; break;
	// 	case ImUiInputKey_0:				scanCode = SDL_SCANCODE_0; break;
	// 	case ImUiInputKey_Enter:			scanCode = SDL_SCANCODE_RETURN; break;
	// 	case ImUiInputKey_Escape:			scanCode = SDL_SCANCODE_ESCAPE; break;
	// 	case ImUiInputKey_Backspace:		scanCode = SDL_SCANCODE_BACKSPACE; break;
	// 	case ImUiInputKey_Tab:				scanCode = SDL_SCANCODE_TAB; break;
	// 	case ImUiInputKey_Space:			scanCode = SDL_SCANCODE_SPACE; break;
	// 	case ImUiInputKey_LeftShift:		scanCode = SDL_SCANCODE_LSHIFT; break;
	// 	case ImUiInputKey_RightShift:		scanCode = SDL_SCANCODE_RSHIFT; break;
	// 	case ImUiInputKey_LeftControl:		scanCode = SDL_SCANCODE_LCTRL; break;
	// 	case ImUiInputKey_RightControl:		scanCode = SDL_SCANCODE_RCTRL; break;
	// 	case ImUiInputKey_LeftAlt:			scanCode = SDL_SCANCODE_LALT; break;
	// 	case ImUiInputKey_RightAlt:			scanCode = SDL_SCANCODE_RALT; break;
	// 	case ImUiInputKey_Minus:			scanCode = SDL_SCANCODE_MINUS; break;
	// 	case ImUiInputKey_Equals:			scanCode = SDL_SCANCODE_EQUALS; break;
	// 	case ImUiInputKey_LeftBracket:		scanCode = SDL_SCANCODE_LEFTBRACKET; break;
	// 	case ImUiInputKey_RightBracket:		scanCode = SDL_SCANCODE_RIGHTBRACKET; break;
	// 	case ImUiInputKey_Backslash:		scanCode = SDL_SCANCODE_BACKSLASH; break;
	// 	case ImUiInputKey_Semicolon:		scanCode = SDL_SCANCODE_SEMICOLON; break;
	// 	case ImUiInputKey_Apostrophe:		scanCode = SDL_SCANCODE_APOSTROPHE; break;
	// 	case ImUiInputKey_Grave:			scanCode = SDL_SCANCODE_GRAVE; break;
	// 	case ImUiInputKey_Comma:			scanCode = SDL_SCANCODE_COMMA; break;
	// 	case ImUiInputKey_Period:			scanCode = SDL_SCANCODE_PERIOD; break;
	// 	case ImUiInputKey_Slash:			scanCode = SDL_SCANCODE_SLASH; break;
	// 	case ImUiInputKey_F1:				scanCode = SDL_SCANCODE_F1; break;
	// 	case ImUiInputKey_F2:				scanCode = SDL_SCANCODE_F2; break;
	// 	case ImUiInputKey_F3:				scanCode = SDL_SCANCODE_F3; break;
	// 	case ImUiInputKey_F4:				scanCode = SDL_SCANCODE_F4; break;
	// 	case ImUiInputKey_F5:				scanCode = SDL_SCANCODE_F5; break;
	// 	case ImUiInputKey_F6:				scanCode = SDL_SCANCODE_F6; break;
	// 	case ImUiInputKey_F7:				scanCode = SDL_SCANCODE_F7; break;
	// 	case ImUiInputKey_F8:				scanCode = SDL_SCANCODE_F8; break;
	// 	case ImUiInputKey_F9:				scanCode = SDL_SCANCODE_F9; break;
	// 	case ImUiInputKey_F10:				scanCode = SDL_SCANCODE_F10; break;
	// 	case ImUiInputKey_F11:				scanCode = SDL_SCANCODE_F11; break;
	// 	case ImUiInputKey_F12:				scanCode = SDL_SCANCODE_F12; break;
	// 	case ImUiInputKey_Print:			scanCode = SDL_SCANCODE_PRINTSCREEN; break;
	// 	case ImUiInputKey_Pause:			scanCode = SDL_SCANCODE_PAUSE; break;
	// 	case ImUiInputKey_Insert:			scanCode = SDL_SCANCODE_INSERT; break;
	// 	case ImUiInputKey_Delete:			scanCode = SDL_SCANCODE_DELETE; break;
	// 	case ImUiInputKey_Home:				scanCode = SDL_SCANCODE_HOME; break;
	// 	case ImUiInputKey_End:				scanCode = SDL_SCANCODE_END; break;
	// 	case ImUiInputKey_PageUp:			scanCode = SDL_SCANCODE_PAGEUP; break;
	// 	case ImUiInputKey_PageDown:			scanCode = SDL_SCANCODE_PAGEDOWN; break;
	// 	case ImUiInputKey_Up:				scanCode = SDL_SCANCODE_UP; break;
	// 	case ImUiInputKey_Left:				scanCode = SDL_SCANCODE_LEFT; break;
	// 	case ImUiInputKey_Down:				scanCode = SDL_SCANCODE_DOWN; break;
	// 	case ImUiInputKey_Right:			scanCode = SDL_SCANCODE_RIGHT; break;
	// 	case ImUiInputKey_Numpad_Divide:	scanCode = SDL_SCANCODE_KP_DIVIDE; break;
	// 	case ImUiInputKey_Numpad_Multiply:	scanCode = SDL_SCANCODE_KP_MULTIPLY; break;
	// 	case ImUiInputKey_Numpad_Minus:		scanCode = SDL_SCANCODE_KP_MINUS; break;
	// 	case ImUiInputKey_Numpad_Plus:		scanCode = SDL_SCANCODE_KP_PLUS; break;
	// 	case ImUiInputKey_Numpad_Enter:		scanCode = SDL_SCANCODE_KP_ENTER; break;
	// 	case ImUiInputKey_Numpad_1:			scanCode = SDL_SCANCODE_KP_1; break;
	// 	case ImUiInputKey_Numpad_2:			scanCode = SDL_SCANCODE_KP_2; break;
	// 	case ImUiInputKey_Numpad_3:			scanCode = SDL_SCANCODE_KP_3; break;
	// 	case ImUiInputKey_Numpad_4:			scanCode = SDL_SCANCODE_KP_4; break;
	// 	case ImUiInputKey_Numpad_5:			scanCode = SDL_SCANCODE_KP_5; break;
	// 	case ImUiInputKey_Numpad_6:			scanCode = SDL_SCANCODE_KP_6; break;
	// 	case ImUiInputKey_Numpad_7:			scanCode = SDL_SCANCODE_KP_7; break;
	// 	case ImUiInputKey_Numpad_8:			scanCode = SDL_SCANCODE_KP_8; break;
	// 	case ImUiInputKey_Numpad_9:			scanCode = SDL_SCANCODE_KP_9; break;
	// 	case ImUiInputKey_Numpad_0:			scanCode = SDL_SCANCODE_KP_0; break;
	// 	case ImUiInputKey_Numpad_Period:	scanCode = SDL_SCANCODE_KP_PERIOD; break;
	// 	case ImUiInputKey_MAX:				break;
	// 	}

	// 	platform.inputKeyMapping[ scanCode ] = keyValue;
	// }

	// Linux sucks and there one million ways to store fonts, so hard coded one path for my distribution.
	const char fontsPath[] = "/run/current-system/sw/share/X11/fonts/";

	platform.fontBasePathLength = IMAPP_ARRAY_COUNT( fontsPath ) - 1;
	platform.fontBasePath = malloc( platform.fontBasePathLength + 1 );
	if( !fontsPath )
	{
		return 1;
	}

	strncpy( platform.fontBasePath, fontsPath, platform.fontBasePathLength + 1 );

	const int result = ImAppMain( &platform, argc, argv );

	SDL_Quit();

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	// for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->systemCursors ); ++i )
	// {
	// 	platform->systemCursors[ i ] = SDL_CreateSystemCursor( s_sdlSystemCursorMapping[ i ] );
	// }

	platform->resourceBasePathLength = strlen( resourcePath );

	const char* sourcePath			= resourcePath;
	const bool isRelativePath		= (sourcePath && strstr( sourcePath, "./" ) == sourcePath);
	const bool needsSeperatorEnd	= (resourcePath[ platform->resourceBasePathLength - 1 ] != '/');

	if( needsSeperatorEnd )
	{
		platform->resourceBasePathLength++;
	}
	if( isRelativePath )
	{
		const char* basePath = SDL_GetBasePath();
		platform->resourceBasePathLength += strlen( basePath );
		platform->resourceBasePathLength -= 2; // for "./"
	}

	platform->resourceBasePath = malloc( platform->resourceBasePathLength + 1 );
	if( !resourcePath )
	{
		return false;
	}

	if( isRelativePath )
	{
		const char* basePath = SDL_GetBasePath();
		strncpy( platform->resourceBasePath, basePath, platform->resourceBasePathLength + 1 );
		sourcePath += 2;
	}

	{
		char* targetPath = strrchr( platform->resourceBasePath, IMAPP_PLATFORM_SDL_PATH_SEPERATOR );
		if( targetPath == NULL )
		{
			targetPath = platform->resourceBasePath;
		}
		else
		{
			targetPath++;
		}

		strncpy( targetPath, sourcePath, platform->resourceBasePathLength - (targetPath - platform->resourceBasePath) + 1 );

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
		for( ; *targetPath != '\0'; ++targetPath )
		{
			if( *targetPath == L'/' )
			{
				*targetPath = L'\\';
			}
		}

		if( needsSeperatorEnd )
		{
			platform->resourceBasePath[ platform->resourceBasePathLength - 1u ] = IMAPP_PLATFORM_SDL_PATH_SEPERATOR;
			platform->resourceBasePath[ platform->resourceBasePathLength ] = '\0';
		}
#endif
	}

	return true;
}

void ImAppPlatformShutdown( ImAppPlatform* platform )
{
	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->systemCursors ); ++i )
	{
		SDL_FreeCursor( platform->systemCursors[ i ] );
		platform->systemCursors[ i ] = NULL;
	}

	platform->allocator = NULL;
}

int64_t ImAppPlatformTick( ImAppPlatform* platform, int64_t lastTickValue, int64_t tickInterval )
{
	int64_t currentTick			= (int64_t)SDL_GetTicks64();
	const int64_t deltaTicks	= currentTick - lastTickValue;
	int64_t timeToWait = IMUI_MAX( tickInterval, deltaTicks ) - deltaTicks;

	if( tickInterval == 0 )
	{
		timeToWait = INT_MAX;
	}

	if( timeToWait > 1 )
	{
		SDL_WaitEventTimeout( NULL, (int)timeToWait - 1 );

		currentTick = (int64_t)SDL_GetTicks64();
	}

	return (int64_t)currentTick;
}

void ImAppPlatformShowError( ImAppPlatform* pPlatform, const char* pMessage )
{
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", pMessage, NULL );
}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
	SDL_SetCursor( platform->systemCursors[ cursor ] );
}



void ImAppPlatformSetClipboardText( ImAppPlatform* platform, const char* text )
{
	SDL_SetClipboardText( text );
}

void ImAppPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui )
{
	const char* clipboardText = SDL_GetClipboardText();
	ImUiInputSetPasteText( imui, clipboardText );
}

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowStyle style, ImAppWindowState state )
{
	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->allocator		= platform->allocator;
	window->platform		= platform;

	Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	switch( style )
	{
	case ImAppWindowState_Resizable:
		flags |= SDL_WINDOW_RESIZABLE;
		break;

	case ImAppWindowState_Borderless:
		flags |= SDL_WINDOW_BORDERLESS;
		if( state == ImAppWindowState_Maximized )
		{
			flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
		}
		break;
	}

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

	window->sdlWindow = SDL_CreateWindow( windowTitle, SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), width, height, flags );
	if( window->sdlWindow == NULL )
	{
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to create Window.", NULL );

		ImUiMemoryFree( platform->allocator, window );
		return NULL;
	}

	ImAppEventQueueConstruct( &window->eventQueue, platform->allocator );

	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{
	IMAPP_ASSERT( window->glContext == NULL );

	while( window->firstNewDrop )
	{
		ImAppWindowDrop* drop = window->firstNewDrop;
		window->firstNewDrop = drop->nextDrop;

		ImUiMemoryFree( window->platform->allocator, drop );
	}

	while( window->firstPoppedDrop )
	{
		ImAppWindowDrop* drop = window->firstPoppedDrop;
		window->firstPoppedDrop = drop->nextDrop;

		ImUiMemoryFree( window->platform->allocator, drop );
	}

	ImAppEventQueueDestruct( &window->eventQueue );

	if( window->sdlWindow != NULL )
	{
		SDL_DestroyWindow( window->sdlWindow );
		window->sdlWindow = NULL;
	}

	ImUiMemoryFree( window->allocator, window );
}

bool ImAppPlatformWindowCreateGlContext( ImAppWindow* pWindow )
{
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID ) || IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
#else
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY );
#endif

	pWindow->glContext = SDL_GL_CreateContext( pWindow->sdlWindow );
	if( pWindow->glContext == NULL )
	{
		return false;
	}

	if( SDL_GL_SetSwapInterval( 1 ) < 0 )
	{
		ImAppTrace( "[renderer] Unable to set VSync! SDL Error: %s\n", SDL_GetError() );
	}

	return true;
}

void ImAppPlatformWindowDestroyGlContext( ImAppWindow* window )
{
	if( window->glContext != NULL )
	{
		SDL_GL_DeleteContext( window->glContext );
		window->glContext = NULL;
	}
}

void ImAppPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg )
{
#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	SDL_GL_SetSwapInterval( 1 );
#endif

	while( window->firstPoppedDrop )
	{
		ImAppWindowDrop* drop = window->firstPoppedDrop;
		window->firstPoppedDrop = drop->nextDrop;

		ImUiMemoryFree( window->platform->allocator, drop );
	}

	SDL_Event sdlEvent;
	while( SDL_PollEvent( &sdlEvent ) )
	{
		switch( sdlEvent.type )
		{
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			{
				const SDL_KeyboardEvent* sdlKeyEvent = &sdlEvent.key;

				const ImUiInputKey mappedKey = window->platform->inputKeyMapping[ sdlKeyEvent->keysym.scancode ];
				if( mappedKey != ImUiInputKey_None )
				{
					const ImAppEventType eventType	= sdlKeyEvent->type == SDL_KEYDOWN ? ImAppEventType_KeyDown : ImAppEventType_KeyUp;
					const bool repeate				= sdlKeyEvent->repeat != 0;
					const ImAppEvent keyEvent		= { .key = { .type = eventType, .key = mappedKey, .repeat = repeate } };
					ImAppEventQueuePush( &window->eventQueue, &keyEvent );
				}
			}
			break;

		case SDL_TEXTINPUT:
			{
				const SDL_TextInputEvent* textInputEvent = &sdlEvent.text;

				for( const char* pText = textInputEvent->text; *pText != '\0'; ++pText )
				{
					const ImAppEvent charEvent = { .character = { .type = ImAppEventType_Character, .character = *pText } };
					ImAppEventQueuePush( &window->eventQueue, &charEvent );
				}
			}
			break;

		case SDL_MOUSEMOTION:
			{
				const SDL_MouseMotionEvent* sdlMotionEvent = &sdlEvent.motion;

				const ImAppEvent motionEvent = { .motion = { .type = ImAppEventType_Motion, .x = sdlMotionEvent->x, .y = sdlMotionEvent->y } };
				ImAppEventQueuePush( &window->eventQueue, &motionEvent );
			}
			break;

		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			{
				const SDL_MouseButtonEvent* sdlButtonEvent = &sdlEvent.button;

				ImUiInputMouseButton button;
				switch( sdlButtonEvent->button )
				{
				case SDL_BUTTON_LEFT:	button = ImUiInputMouseButton_Left; break;
				case SDL_BUTTON_MIDDLE:	button = ImUiInputMouseButton_Middle; break;
				case SDL_BUTTON_RIGHT:	button = ImUiInputMouseButton_Right; break;
				case SDL_BUTTON_X1:		button = ImUiInputMouseButton_X1; break;
				case SDL_BUTTON_X2:		button = ImUiInputMouseButton_X2; break;

				default:
					continue;
				}

				const ImAppEventType eventType	= sdlButtonEvent->type == SDL_MOUSEBUTTONDOWN ? ImAppEventType_ButtonDown : ImAppEventType_ButtonUp;
				const ImAppEvent buttonEvent	= { .button = { .type = eventType, .x = sdlButtonEvent->x, .y = sdlButtonEvent->y, .button = button, .repeateCount = sdlButtonEvent->clicks } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			break;

		case SDL_MOUSEWHEEL:
			{
				const SDL_MouseWheelEvent* sdlWheelEvent = &sdlEvent.wheel;

				const ImAppEvent scrollEvent = { .scroll = { .type = ImAppEventType_Scroll, .x = sdlWheelEvent->x, .y = sdlWheelEvent->y } };
				ImAppEventQueuePush( &window->eventQueue, &scrollEvent );
			}
			break;

		case SDL_WINDOWEVENT:
			{
				const SDL_WindowEvent* sdlWindowEvent = &sdlEvent.window;
				switch( sdlWindowEvent->event )
				{
				case SDL_WINDOWEVENT_CLOSE:
					{
						const ImAppEvent closeEvent = { .type = ImAppEventType_WindowClose };
						ImAppEventQueuePush( &window->eventQueue, &closeEvent );
					}
					break;

				default:
					break;
				}
			}
			break;

		case SDL_DROPFILE:
		case SDL_DROPTEXT:
			{
				const SDL_DropEvent* sdlDropEvent = &sdlEvent.drop;

				const uintsize textLength = strlen( sdlDropEvent->file ) + 1u;
				ImAppWindowDrop* drop = (ImAppWindowDrop*)ImUiMemoryAlloc( window->platform->allocator, sizeof( ImAppWindowDrop ) + textLength );
				drop->type = sdlEvent.type == SDL_DROPFILE ? ImAppDropType_File : ImAppDropType_Text;

				memcpy( drop->pathOrText, sdlDropEvent->file, textLength );
				drop->pathOrText[ textLength ] = '\0';

				drop->nextDrop = window->firstNewDrop;
				window->firstNewDrop = drop;
			}
			break;

		case SDL_QUIT:
#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
			emscripten_cancel_main_loop();
#else
			// TODO
#endif
			break;
		}
	}

	if( callback )
	{
		callback( window, arg );
	}
}

bool ImAppPlatformWindowPresent( ImAppWindow* window )
{
	SDL_GL_SwapWindow( window->sdlWindow );
	return true;
}

ImAppEventQueue* ImAppPlatformWindowGetEventQueue( ImAppWindow* window )
{
	return &window->eventQueue;
}

bool ImAppPlatformWindowPopDropData( ImAppWindow* window, ImAppDropData* outData )
{
	if( !window->firstNewDrop )
	{
		return false;
	}

	ImAppWindowDrop* drop = window->firstNewDrop;
	outData->type		= drop->type;
	outData->pathOrText	= drop->pathOrText;

	window->firstNewDrop = drop->nextDrop;

	drop->nextDrop = window->firstPoppedDrop;
	window->firstPoppedDrop = drop;

	return true;
}

void ImAppPlatformWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	*outX = 0;
	*outY = 0;

	ImAppPlatformWindowGetSize( window, outWidth, outHeight );
}

void ImAppPlatformWindowGetSize( const ImAppWindow* window, int* outWidth, int* outHeight )
{
	SDL_GetWindowSize( window->sdlWindow, outWidth, outHeight );
}

void ImAppPlatformWindowGetPosition( const ImAppWindow* window, int* outX, int* outY )
{
	SDL_GetWindowPosition( window->sdlWindow, outX, outY );
}

ImAppWindowState ImAppPlatformWindowGetState( const ImAppWindow* pWindow )
{
	const Uint32 flags = SDL_GetWindowFlags( pWindow->sdlWindow );
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

float ImAppPlatformWindowGetDpiScale( const ImAppWindow* window )
{
	float dpi;
	if( SDL_GetDisplayDPI( SDL_GetWindowDisplayIndex( window->sdlWindow ), &dpi, NULL, NULL ) != 0 )
	{
		return 1.0f;
	}

	return dpi / 96.0f;
}

void ImAppPlatformResourceGetPath( ImAppPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName )
{
	const size_t resourceNameLength = strlen( resourceName );
	if( platform->resourceBasePathLength + resourceNameLength >= pathCapacity )
	{
		ImAppTrace( "Error: Resource path buffer too small!\n" );
		return;
	}
	memcpy( outPath, platform->resourceBasePath, platform->resourceBasePathLength );
	memcpy( outPath + platform->resourceBasePathLength, resourceName, resourceNameLength );
	outPath[ platform->resourceBasePathLength + resourceNameLength ] = '\0';
}

ImAppBlob ImAppPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName )
{
	return ImAppPlatformResourceLoadRange( platform, resourceName, 0u, (uintsize)-1 );
}

ImAppBlob ImAppPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	ImAppBlob result ={ NULL, 0u };

	ImAppFile* file = ImAppPlatformResourceOpen( platform, resourceName );
	if( !file )
	{
		return result;
	}

	if( length == (uintsize)-1 )
	{
		SDL_RWops* rwops = (SDL_RWops*)file;
		length = rwops->size( rwops ) - offset;
	}

	void* memory = ImUiMemoryAlloc( platform->allocator, length );
	if( !memory )
	{
		ImAppPlatformResourceClose( platform, file );
		return result;
	}

	const uintsize readResult = ImAppPlatformResourceRead( file, memory, length, offset );
	ImAppPlatformResourceClose( platform, file );

	if( readResult != length )
	{
		ImUiMemoryFree( platform->allocator, memory );
		return result;
	}

	result.data	= memory;
	result.size	= length;
	return result;
}

ImAppFile* ImAppPlatformResourceOpen( ImAppPlatform* platform, const char* resourceName )
{
	const size_t resourceNameLength = strlen( resourceName );
	const size_t resourcePathLength = platform->resourceBasePathLength + resourceNameLength + 1;

	char* resourcePath = ImUiMemoryAlloc( platform->allocator, resourcePathLength );
	memcpy( resourcePath, platform->resourceBasePath, platform->resourceBasePathLength );
	memcpy( resourcePath + platform->resourceBasePathLength, resourceName, resourceNameLength + 1 );

	SDL_RWops* file = SDL_RWFromFile( platform->resourceBasePath, "rb" );
	if( !file )
	{
		ImAppTrace( "Error: Failed to open '%s'\n", platform->resourceBasePath );
		return NULL;
	}

	return (ImAppFile*)file;
}
uintsize ImAppPlatformResourceRead( ImAppFile* file, void* outData, uintsize length, uintsize offset )
{
	SDL_RWops* rwopts = (SDL_RWops*)file;

	if( SDL_RWseek( rwopts, (Sint64)offset, RW_SEEK_SET ) == -1 )
	{
		return 0u;
	}

	const size_t readResult = SDL_RWread( rwopts, outData, 1u, length );

	if( readResult == 0u )
	{
		return 0u;
	}

	return readResult;
}

void ImAppPlatformResourceClose( ImAppPlatform* platform, ImAppFile* file )
{
	SDL_RWops* rwops = (SDL_RWops*)file;
	SDL_RWclose( rwops );
}

ImAppBlob ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName )
{
	const size_t fontNameLength = strlen( fontName );
	const size_t fontPathLength = platform->fontBasePathLength + fontNameLength;
	char* fontPath = ImUiMemoryAlloc( platform->allocator, fontPathLength + 1 );
	if( !fontPath )
	{
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	memcpy( fontPath, platform->fontBasePath, platform->fontBasePathLength );
	memcpy( fontPath + platform->fontBasePathLength, fontName, fontNameLength + 1 );

	SDL_RWops* file = SDL_RWFromFile( fontPath, "rb" );
	if( !file )
	{
		ImAppTrace( "Error: Failed to open '%s'\n", fontPath );
		ImUiMemoryFree( platform->allocator, fontPath );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	const Sint64 fileSize = SDL_RWsize( file );
	if( fileSize == -1 )
	{
		ImUiMemoryFree( platform->allocator, fontPath );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	void* memory = ImUiMemoryAlloc( platform->allocator, (size_t)fileSize );

	const size_t readResult = SDL_RWread( file, memory, 1u, (size_t)fileSize );
	SDL_RWclose( file );

	if( readResult != fileSize )
	{
		ImUiMemoryFree( platform->allocator, memory );
		ImUiMemoryFree( platform->allocator, fontPath );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	ImUiMemoryFree( platform->allocator, fontPath );
	const ImAppBlob result = { memory, (size_t)fileSize };
	return result;
}

void ImAppPlatformResourceFree( ImAppPlatform* platform, ImAppBlob blob )
{
	ImUiMemoryFree( platform->allocator, blob.data );
}

ImAppFileWatcher* ImAppPlatformFileWatcherCreate( ImAppPlatform* platform )
{
	// not implemented
	return NULL;
}

void ImAppPlatformFileWatcherDestroy( ImAppPlatform* platform, ImAppFileWatcher* watcher )
{
}

void ImAppPlatformFileWatcherAddPath( ImAppFileWatcher* watcher, const char* path )
{
}

void ImAppPlatformFileWatcherRemovePath( ImAppFileWatcher* watcher, const char* path )
{
}

bool ImAppPlatformFileWatcherPopEvent( ImAppFileWatcher* watcher, ImAppFileWatchEvent* outEvent )
{
	return false;
}

ImAppThread* ImAppPlatformThreadCreate( ImAppPlatform* platform, const char* name, ImAppThreadFunc func, void* arg )
{
	ImAppThread* thread = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppThread );
	if( !thread )
	{
		return NULL;
	}

	SDL_AtomicSet( &thread->isRunning, 1 );

	thread->platform	= platform;
	thread->func		= func;
	thread->arg			= arg;
	thread->sdlThread	= SDL_CreateThread( ImAppPlatformThreadEntry, name, thread );

	//SetThreadDescription( thread->handle, name );

	return thread;
}

void ImAppPlatformThreadDestroy( ImAppThread* thread )
{
	if( SDL_AtomicGet( (SDL_atomic_t*)&thread->isRunning ) )
	{
		SDL_WaitThread( thread->sdlThread, NULL );
	}

	ImUiMemoryFree( thread->platform->allocator, thread );
}

bool ImAppPlatformThreadIsRunning( const ImAppThread* thread )
{
	return SDL_AtomicGet( (SDL_atomic_t*)&thread->isRunning);
}

static int SDLCALL ImAppPlatformThreadEntry( void* voidThread )
{
	ImAppThread* thread = (ImAppThread*)voidThread;

	thread->func( thread->arg );

	SDL_AtomicSet( &thread->isRunning, 0 );

	return 0u;
}

ImAppMutex* ImAppPlatformMutexCreate( ImAppPlatform* platform )
{
	return (ImAppMutex*)SDL_CreateMutex();
}

void ImAppPlatformMutexDestroy( ImAppPlatform* platform, ImAppMutex* mutex )
{
	SDL_mutex* sdlMutex = (SDL_mutex*)mutex;
	SDL_DestroyMutex( sdlMutex );
}

void ImAppPlatformMutexLock( ImAppMutex* mutex )
{
	SDL_mutex* sdlMutex = (SDL_mutex*)mutex;
	SDL_LockMutex( sdlMutex );
}

void ImAppPlatformMutexUnlock( ImAppMutex* mutex )
{
	SDL_mutex* sdlMutex = (SDL_mutex*)mutex;
	SDL_UnlockMutex( sdlMutex );
}

ImAppSemaphore* ImAppPlatformSemaphoreCreate( ImAppPlatform* platform )
{
	return (ImAppSemaphore*)SDL_CreateSemaphore( 0u );
}

void ImAppPlatformSemaphoreDestroy( ImAppPlatform* platform, ImAppSemaphore* semaphore )
{
	SDL_sem* sdlSem = (SDL_sem*)semaphore;
	SDL_DestroySemaphore( sdlSem );
}

void ImAppPlatformSemaphoreInc( ImAppSemaphore* semaphore )
{
	SDL_sem* sdlSem = (SDL_sem*)semaphore;
	SDL_SemPost( sdlSem );
}

bool ImAppPlatformSemaphoreDec( ImAppSemaphore* semaphore, bool wait )
{
	SDL_sem* sdlSem = (SDL_sem*)semaphore;
	if( wait )
	{
		return SDL_SemWait( sdlSem ) == 0;
	}
	else
	{
		return SDL_SemTryWait( sdlSem ) == 0;
	}
}

#endif
