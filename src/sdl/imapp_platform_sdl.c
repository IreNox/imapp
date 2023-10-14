#include "../imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_SDL )

#include "../imapp_debug.h"
#include "../imapp_event_queue.h"

#include <SDL.h>

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
#	include <windows.h>
#endif

//////////////////////////////////////////////////////////////////////////
// Main

struct ImAppPlatform
{
	ImUiAllocator*	allocator;

	ImUiInputKey	inputKeyMapping[ SDL_NUM_SCANCODES ];

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	wchar_t			resourcePath[ MAX_PATH ];
	size_t			resourceBasePathLength;

	wchar_t			fontPath[ MAX_PATH ];
	size_t			fontBasePathLength;
#endif

	SDL_Cursor*		systemCursors[ ImUiInputMouseCursor_MAX ];
};

static const SDL_SystemCursor s_systemCursorMapping[] =
{
	SDL_SYSTEM_CURSOR_ARROW,
	SDL_SYSTEM_CURSOR_WAIT,
	SDL_SYSTEM_CURSOR_WAITARROW,
	SDL_SYSTEM_CURSOR_IBEAM,
	SDL_SYSTEM_CURSOR_CROSSHAIR,
	SDL_SYSTEM_CURSOR_HAND,
	SDL_SYSTEM_CURSOR_SIZENWSE,
	SDL_SYSTEM_CURSOR_SIZENESW,
	SDL_SYSTEM_CURSOR_SIZEWE,
	SDL_SYSTEM_CURSOR_SIZENS,
	SDL_SYSTEM_CURSOR_SIZEALL
};
IMAPP_STATIC_ASSERT( IMAPP_ARRAY_COUNT( s_systemCursorMapping ) == ImUiInputMouseCursor_MAX );

int SDL_main( int argc, char* argv[] )
{
	ImAppPlatform platform = { 0 };

	if( SDL_Init( SDL_INIT_EVENTS | SDL_INIT_VIDEO | SDL_INIT_TIMER ) < 0 )
	{
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to initialize SDL.", NULL );
		return 1;
	}

	for( size_t i = 0u; i < ImUiInputKey_MAX; ++i )
	{
		const ImUiInputKey keyValue = (ImUiInputKey)i;

		SDL_Scancode scanCode = SDL_SCANCODE_UNKNOWN;
		switch( keyValue )
		{
		case ImUiInputKey_None:				scanCode = SDL_SCANCODE_UNKNOWN; break;
		case ImUiInputKey_A:				scanCode = SDL_SCANCODE_A; break;
		case ImUiInputKey_B:				scanCode = SDL_SCANCODE_B; break;
		case ImUiInputKey_C:				scanCode = SDL_SCANCODE_C; break;
		case ImUiInputKey_D:				scanCode = SDL_SCANCODE_D; break;
		case ImUiInputKey_E:				scanCode = SDL_SCANCODE_E; break;
		case ImUiInputKey_F:				scanCode = SDL_SCANCODE_F; break;
		case ImUiInputKey_G:				scanCode = SDL_SCANCODE_G; break;
		case ImUiInputKey_H:				scanCode = SDL_SCANCODE_H; break;
		case ImUiInputKey_I:				scanCode = SDL_SCANCODE_I; break;
		case ImUiInputKey_J:				scanCode = SDL_SCANCODE_J; break;
		case ImUiInputKey_K:				scanCode = SDL_SCANCODE_K; break;
		case ImUiInputKey_L:				scanCode = SDL_SCANCODE_L; break;
		case ImUiInputKey_M:				scanCode = SDL_SCANCODE_M; break;
		case ImUiInputKey_N:				scanCode = SDL_SCANCODE_N; break;
		case ImUiInputKey_O:				scanCode = SDL_SCANCODE_O; break;
		case ImUiInputKey_P:				scanCode = SDL_SCANCODE_P; break;
		case ImUiInputKey_Q:				scanCode = SDL_SCANCODE_Q; break;
		case ImUiInputKey_R:				scanCode = SDL_SCANCODE_R; break;
		case ImUiInputKey_S:				scanCode = SDL_SCANCODE_S; break;
		case ImUiInputKey_T:				scanCode = SDL_SCANCODE_T; break;
		case ImUiInputKey_U:				scanCode = SDL_SCANCODE_U; break;
		case ImUiInputKey_V:				scanCode = SDL_SCANCODE_V; break;
		case ImUiInputKey_W:				scanCode = SDL_SCANCODE_W; break;
		case ImUiInputKey_X:				scanCode = SDL_SCANCODE_X; break;
		case ImUiInputKey_Y:				scanCode = SDL_SCANCODE_Y; break;
		case ImUiInputKey_Z:				scanCode = SDL_SCANCODE_Z; break;
		case ImUiInputKey_1:				scanCode = SDL_SCANCODE_1; break;
		case ImUiInputKey_2:				scanCode = SDL_SCANCODE_2; break;
		case ImUiInputKey_3:				scanCode = SDL_SCANCODE_3; break;
		case ImUiInputKey_4:				scanCode = SDL_SCANCODE_4; break;
		case ImUiInputKey_5:				scanCode = SDL_SCANCODE_5; break;
		case ImUiInputKey_6:				scanCode = SDL_SCANCODE_6; break;
		case ImUiInputKey_7:				scanCode = SDL_SCANCODE_7; break;
		case ImUiInputKey_8:				scanCode = SDL_SCANCODE_8; break;
		case ImUiInputKey_9:				scanCode = SDL_SCANCODE_9; break;
		case ImUiInputKey_0:				scanCode = SDL_SCANCODE_0; break;
		case ImUiInputKey_Enter:			scanCode = SDL_SCANCODE_RETURN; break;
		case ImUiInputKey_Escape:			scanCode = SDL_SCANCODE_ESCAPE; break;
		case ImUiInputKey_Backspace:		scanCode = SDL_SCANCODE_BACKSPACE; break;
		case ImUiInputKey_Tab:				scanCode = SDL_SCANCODE_TAB; break;
		case ImUiInputKey_Space:			scanCode = SDL_SCANCODE_SPACE; break;
		case ImUiInputKey_LeftShift:		scanCode = SDL_SCANCODE_LSHIFT; break;
		case ImUiInputKey_RightShift:		scanCode = SDL_SCANCODE_RSHIFT; break;
		case ImUiInputKey_LeftControl:		scanCode = SDL_SCANCODE_LCTRL; break;
		case ImUiInputKey_RightControl:		scanCode = SDL_SCANCODE_RCTRL; break;
		case ImUiInputKey_LeftAlt:			scanCode = SDL_SCANCODE_LALT; break;
		case ImUiInputKey_RightAlt:			scanCode = SDL_SCANCODE_RALT; break;
		case ImUiInputKey_Minus:			scanCode = SDL_SCANCODE_MINUS; break;
		case ImUiInputKey_Equals:			scanCode = SDL_SCANCODE_EQUALS; break;
		case ImUiInputKey_LeftBracket:		scanCode = SDL_SCANCODE_LEFTBRACKET; break;
		case ImUiInputKey_RightBracket:		scanCode = SDL_SCANCODE_RIGHTBRACKET; break;
		case ImUiInputKey_Backslash:		scanCode = SDL_SCANCODE_BACKSLASH; break;
		case ImUiInputKey_Semicolon:		scanCode = SDL_SCANCODE_SEMICOLON; break;
		case ImUiInputKey_Apostrophe:		scanCode = SDL_SCANCODE_APOSTROPHE; break;
		case ImUiInputKey_Grave:			scanCode = SDL_SCANCODE_GRAVE; break;
		case ImUiInputKey_Comma:			scanCode = SDL_SCANCODE_COMMA; break;
		case ImUiInputKey_Period:			scanCode = SDL_SCANCODE_PERIOD; break;
		case ImUiInputKey_Slash:			scanCode = SDL_SCANCODE_SLASH; break;
		case ImUiInputKey_F1:				scanCode = SDL_SCANCODE_F1; break;
		case ImUiInputKey_F2:				scanCode = SDL_SCANCODE_F2; break;
		case ImUiInputKey_F3:				scanCode = SDL_SCANCODE_F3; break;
		case ImUiInputKey_F4:				scanCode = SDL_SCANCODE_F4; break;
		case ImUiInputKey_F5:				scanCode = SDL_SCANCODE_F5; break;
		case ImUiInputKey_F6:				scanCode = SDL_SCANCODE_F6; break;
		case ImUiInputKey_F7:				scanCode = SDL_SCANCODE_F7; break;
		case ImUiInputKey_F8:				scanCode = SDL_SCANCODE_F8; break;
		case ImUiInputKey_F9:				scanCode = SDL_SCANCODE_F9; break;
		case ImUiInputKey_F10:				scanCode = SDL_SCANCODE_F10; break;
		case ImUiInputKey_F11:				scanCode = SDL_SCANCODE_F11; break;
		case ImUiInputKey_F12:				scanCode = SDL_SCANCODE_F12; break;
		case ImUiInputKey_Print:			scanCode = SDL_SCANCODE_PRINTSCREEN; break;
		case ImUiInputKey_Pause:			scanCode = SDL_SCANCODE_PAUSE; break;
		case ImUiInputKey_Insert:			scanCode = SDL_SCANCODE_INSERT; break;
		case ImUiInputKey_Delete:			scanCode = SDL_SCANCODE_DELETE; break;
		case ImUiInputKey_Home:				scanCode = SDL_SCANCODE_HOME; break;
		case ImUiInputKey_End:				scanCode = SDL_SCANCODE_END; break;
		case ImUiInputKey_PageUp:			scanCode = SDL_SCANCODE_PAGEUP; break;
		case ImUiInputKey_PageDown:			scanCode = SDL_SCANCODE_PAGEDOWN; break;
		case ImUiInputKey_Up:				scanCode = SDL_SCANCODE_UP; break;
		case ImUiInputKey_Left:				scanCode = SDL_SCANCODE_LEFT; break;
		case ImUiInputKey_Down:				scanCode = SDL_SCANCODE_DOWN; break;
		case ImUiInputKey_Right:			scanCode = SDL_SCANCODE_RIGHT; break;
		case ImUiInputKey_Numpad_Divide:	scanCode = SDL_SCANCODE_KP_DIVIDE; break;
		case ImUiInputKey_Numpad_Multiply:	scanCode = SDL_SCANCODE_KP_MULTIPLY; break;
		case ImUiInputKey_Numpad_Minus:		scanCode = SDL_SCANCODE_KP_MINUS; break;
		case ImUiInputKey_Numpad_Plus:		scanCode = SDL_SCANCODE_KP_PLUS; break;
		case ImUiInputKey_Numpad_Enter:		scanCode = SDL_SCANCODE_KP_ENTER; break;
		case ImUiInputKey_Numpad_1:			scanCode = SDL_SCANCODE_KP_1; break;
		case ImUiInputKey_Numpad_2:			scanCode = SDL_SCANCODE_KP_2; break;
		case ImUiInputKey_Numpad_3:			scanCode = SDL_SCANCODE_KP_3; break;
		case ImUiInputKey_Numpad_4:			scanCode = SDL_SCANCODE_KP_4; break;
		case ImUiInputKey_Numpad_5:			scanCode = SDL_SCANCODE_KP_5; break;
		case ImUiInputKey_Numpad_6:			scanCode = SDL_SCANCODE_KP_6; break;
		case ImUiInputKey_Numpad_7:			scanCode = SDL_SCANCODE_KP_7; break;
		case ImUiInputKey_Numpad_8:			scanCode = SDL_SCANCODE_KP_8; break;
		case ImUiInputKey_Numpad_9:			scanCode = SDL_SCANCODE_KP_9; break;
		case ImUiInputKey_Numpad_0:			scanCode = SDL_SCANCODE_KP_0; break;
		case ImUiInputKey_Numpad_Period:	scanCode = SDL_SCANCODE_KP_PERIOD; break;
		}

		platform.inputKeyMapping[ scanCode ] = keyValue;
	}

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	platform.fontBasePathLength = GetWindowsDirectoryW( platform.fontPath, IMAPP_ARRAY_COUNT( platform.fontPath ) );
	{
		const wchar_t fontsPath[] = L"\\Fonts\\";
		wcscpy_s( platform.fontPath + platform.fontBasePathLength, IMAPP_ARRAY_COUNT( platform.fontPath ) - platform.fontBasePathLength, fontsPath );
		platform.fontBasePathLength += IMAPP_ARRAY_COUNT( fontsPath ) - 1u;
	}
#endif

	const int result = ImAppMain( &platform, argc, argv );

	SDL_Quit();

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->systemCursors ); ++i )
	{
		platform->systemCursors[ i ] = SDL_CreateSystemCursor( s_systemCursorMapping[ i ] );
	}

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	const char* sourcePath = resourcePath;
	if( sourcePath && strstr( sourcePath, "./" ) == sourcePath )
	{
		GetModuleFileNameW( NULL, platform->resourcePath, IMAPP_ARRAY_COUNT( platform->resourcePath ) );
		sourcePath += 2u;
	}
	{
		wchar_t* pTargetPath = wcsrchr( platform->resourcePath, L'\\' );
		if( pTargetPath == NULL )
		{
			pTargetPath = platform->resourcePath;
		}
		else
		{
			pTargetPath++;
		}
		platform->resourceBasePathLength = (pTargetPath - platform->resourcePath);

		const int targetLengthInCharacters = IMAPP_ARRAY_COUNT( platform->resourcePath ) - (int)platform->resourceBasePathLength;
		const int convertResult = MultiByteToWideChar( CP_UTF8, 0u, sourcePath, -1, pTargetPath, targetLengthInCharacters );
		if( !convertResult )
		{
			ImAppTrace( "Error: Failed to convert resource path!\n" );
			return false;
		}

		for( size_t i = 0; i < convertResult - 1; ++i )
		{
			if( pTargetPath[ i ] == L'/' )
			{
				pTargetPath[ i ] = L'\\';
			}
		}

		platform->resourceBasePathLength += (size_t)convertResult - 1;

		if( platform->resourcePath[ platform->resourceBasePathLength - 1u ] != L'/' )
		{
			if( platform->resourceBasePathLength == IMAPP_ARRAY_COUNT( platform->resourcePath ) )
			{
				ImAppTrace( "Error: Resource path exceeds limit!\n" );
				return false;
			}

			platform->resourcePath[ platform->resourceBasePathLength ] = L'\\';
			platform->resourceBasePathLength++;
		}
	}
#endif

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

void ImAppPlatformShowError( ImAppPlatform* pPlatform, const char* pMessage )
{
	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", pMessage, NULL );
}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
	SDL_SetCursor( platform->systemCursors[ cursor ] );
}

//////////////////////////////////////////////////////////////////////////
// Window

struct ImAppWindow
{
	ImUiAllocator*		allocator;
	ImAppPlatform*		platform;
	ImAppEventQueue*	eventQueue;

	SDL_Window*			sdlWindow;
	SDL_GLContext		glContext;

	char*				dropBuffer;
	uintsize			dropBufferCapacity;
};

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->allocator		= platform->allocator;
	window->platform		= platform;
	window->eventQueue		= ImAppEventQueueCreate( platform->allocator );

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

	window->sdlWindow = SDL_CreateWindow( windowTitle, SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), width, height, flags );
	if( window->sdlWindow == NULL )
	{
		SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to create Window.", NULL );

		ImUiMemoryFree( platform->allocator, window );
		return NULL;
	}

	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{
	IMAPP_ASSERT( window->glContext == NULL );

	ImUiMemoryFree( window->allocator, window->dropBuffer );

	if( window->sdlWindow != NULL )
	{
		SDL_DestroyWindow( window->sdlWindow );
		window->sdlWindow = NULL;
	}

	if( window->eventQueue != NULL )
	{
		ImAppEventQueueDestroy( window->eventQueue );
		window->eventQueue = NULL;
	}

	ImUiMemoryFree( window->allocator, window );
}

bool ImAppPlatformWindowCreateGlContext( ImAppWindow* pWindow )
{
#if IMAPP_ENABLED( IMAPP_PLATFORM_ANDROID )
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES );
#else
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MAJOR_VERSION, 3 );
	SDL_GL_SetAttribute( SDL_GL_CONTEXT_MINOR_VERSION, 0 );
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

int64_t ImAppPlatformWindowTick( ImAppWindow* window, int64_t lastTickValue, int64_t tickInterval )
{
	const int64_t nextTick = (int64_t)SDL_GetTicks64();

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
				const SDL_KeyboardEvent* sdlKeyEvent = &sdlEvent.key;

				const ImUiInputKey mappedKey = window->platform->inputKeyMapping[ sdlKeyEvent->keysym.scancode ];
				if( mappedKey != ImUiInputKey_None )
				{
					const ImAppEventType eventType	= sdlKeyEvent->type == SDL_KEYDOWN ? ImAppEventType_KeyDown : ImAppEventType_KeyUp;
					const bool repeate				= sdlKeyEvent->repeat != 0;
					const ImAppEvent keyEvent		= { .key = { .type = eventType, .key = mappedKey, .repeat = repeate } };
					ImAppEventQueuePush( window->eventQueue, &keyEvent );
				}
			}
			break;

		case SDL_TEXTINPUT:
			{
				const SDL_TextInputEvent* textInputEvent = &sdlEvent.text;

				for( const char* pText = textInputEvent->text; *pText != '\0'; ++pText )
				{
					const ImAppEvent charEvent = { .character = { .type = ImAppEventType_Character, .character = *pText } };
					ImAppEventQueuePush( window->eventQueue, &charEvent );
				}
			}
			break;

		case SDL_MOUSEMOTION:
			{
				const SDL_MouseMotionEvent* sdlMotionEvent = &sdlEvent.motion;

				const ImAppEvent motionEvent = { .motion = { .type = ImAppEventType_Motion, .x = sdlMotionEvent->x, .y = sdlMotionEvent->y } };
				ImAppEventQueuePush( window->eventQueue, &motionEvent );
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
				ImAppEventQueuePush( window->eventQueue, &buttonEvent );
			}
			break;

		case SDL_MOUSEWHEEL:
			{
				const SDL_MouseWheelEvent* wheelEvent = &sdlEvent.wheel;

				const ImAppEvent scrollEvent = { .scroll = { .type = ImAppEventType_Scroll, .x = wheelEvent->x, .y = wheelEvent->y } };
				ImAppEventQueuePush( window->eventQueue, &scrollEvent );
			}
			break;

		case SDL_WINDOWEVENT:
			{
				const SDL_WindowEvent* windowEvent = &sdlEvent.window;
				switch( windowEvent->event )
				{
				case SDL_WINDOWEVENT_CLOSE:
					{
						const ImAppEvent closeEvent = { .type = ImAppEventType_WindowClose };
						ImAppEventQueuePush( window->eventQueue, &closeEvent );
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
				if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( window->allocator, window->dropBuffer, window->dropBufferCapacity, textLength ) )
				{
					continue;
				}

				memcpy( window->dropBuffer, sdlDropEvent->file, textLength );

				const ImAppEventType type = sdlEvent.type == SDL_DROPFILE ? ImAppEventType_DropFile : ImAppEventType_DropText;
				const ImAppEvent dropEvent = { .drop = {.type = type, .pathOrText = window->dropBuffer } };
				ImAppEventQueuePush( window->eventQueue, &dropEvent );
			}
			break;

		default:
			break;
		}
	}

	return (int64_t)nextTick;
}

bool ImAppPlatformWindowPresent( ImAppWindow* pWindow )
{
	SDL_GL_SwapWindow( pWindow->sdlWindow );
	return true;
}

ImAppEventQueue* ImAppPlatformWindowGetEventQueue( ImAppWindow* pWindow )
{
	return pWindow->eventQueue;
}

void ImAppPlatformWindowGetViewRect( int* pX, int* pY, int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	*pX = 0;
	*pY = 0;

	ImAppPlatformWindowGetSize( pWidth, pHeight, pWindow );
}

void ImAppPlatformWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	SDL_GetWindowSize( pWindow->sdlWindow, pWidth, pHeight );
}

void ImAppPlatformWindowGetPosition( int* pX, int* pY, ImAppWindow* pWindow )
{
	SDL_GetWindowPosition( pWindow->sdlWindow, pX, pY );
}

ImAppWindowState ImAppPlatformWindowGetState( ImAppWindow* pWindow )
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

//////////////////////////////////////////////////////////////////////////
// Resources

ImAppBlob ImAppPlatformResourceLoad( ImAppPlatform* platform, ImUiStringView resourceName )
{
#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	wchar_t* pTargetBuffer = platform->resourcePath + platform->resourceBasePathLength;
	const size_t targetLengthInCharacters = IMAPP_ARRAY_COUNT( platform->resourcePath ) - platform->resourceBasePathLength;
	MultiByteToWideChar( CP_UTF8, 0, resourceName.data, (int)resourceName.length, pTargetBuffer, (int)targetLengthInCharacters );
	pTargetBuffer[ resourceName.length ] = L'\0';

	while( *pTargetBuffer != L'\0' )
	{
		if( *pTargetBuffer == L'/' )
		{
			*pTargetBuffer = L'\\';
		}
		pTargetBuffer++;
	}

	const HANDLE fileHandle = CreateFileW( platform->resourcePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( fileHandle == INVALID_HANDLE_VALUE )
	{
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	LARGE_INTEGER fileSize;
	GetFileSizeEx( fileHandle, &fileSize );

	void* memory = ImUiMemoryAlloc( platform->allocator, (size_t)fileSize.QuadPart );

	DWORD bytesRead = 0u;
	const BOOL readResult = ReadFile( fileHandle, memory, (DWORD)fileSize.QuadPart, &bytesRead, NULL );
	CloseHandle( fileHandle );

	if( !readResult || bytesRead != (DWORD)fileSize.QuadPart )
	{
		ImUiMemoryFree( platform->allocator, memory );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	const ImAppBlob result = { memory, (size_t)fileSize.QuadPart };
	return result;
#else
#	error Not imeplemented
#endif
}

ImAppBlob ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, ImUiStringView resourceName )
{
#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS )
	wchar_t* pTargetBuffer = platform->fontPath + platform->fontBasePathLength;
	const size_t targetLengthInCharacters = IMAPP_ARRAY_COUNT( platform->fontPath ) - platform->fontBasePathLength;
	MultiByteToWideChar( CP_UTF8, 0, resourceName.data, (int)resourceName.length, pTargetBuffer, (int)targetLengthInCharacters );
	pTargetBuffer[ resourceName.length ] = L'\0';

	while( *pTargetBuffer != L'\0' )
	{
		if( *pTargetBuffer == L'/' )
		{
			*pTargetBuffer = L'\\';
		}
		pTargetBuffer++;
	}

	const HANDLE fileHandle = CreateFileW( platform->fontPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( fileHandle == INVALID_HANDLE_VALUE )
	{
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	LARGE_INTEGER fileSize;
	GetFileSizeEx( fileHandle, &fileSize );

	void* memory = ImUiMemoryAlloc( platform->allocator, (size_t)fileSize.QuadPart );

	DWORD bytesRead = 0u;
	const BOOL readResult = ReadFile( fileHandle, memory, (DWORD)fileSize.QuadPart, &bytesRead, NULL );
	CloseHandle( fileHandle );

	if( !readResult || bytesRead != (DWORD)fileSize.QuadPart )
	{
		ImUiMemoryFree( platform->allocator, memory );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	const ImAppBlob result = { memory, (size_t)fileSize.QuadPart };
	return result;
#else
#	error Not imeplemented
#endif
}

#endif
