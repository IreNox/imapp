#include "imapp_platform.h"

#include "imapp_internal.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS ) && IMAPP_DISABLED( IMAPP_PLATFORM_SDL )

#include "imapp_debug.h"
#include "imapp_event_queue.h"

#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>

static LRESULT CALLBACK		ImAppPlatformWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
static DWORD WINAPI			ImAppPlatformThreadEntry( void* voidThread );
//static bool					ImAppWindowHandleMessage( ImAppWindow* pWindow, UINT message, WPARAM wParam, LPARAM lParam );
//static bool					ImAppInputHandleMessage( ImAppInputPlatform* pInput, UINT message, WPARAM wParam, LPARAM lParam );

struct ImAppPlatform
{
	ImUiAllocator*	allocator;

	uint8			inputKeyMapping[ 255u ];	// ImUiInputKey

	wchar_t			resourcePath[ MAX_PATH ];
	uintsize			resourceBasePathLength;

	wchar_t			fontPath[ MAX_PATH ];
	uintsize			fontBasePathLength;

	HCURSOR			cursors[ ImUiInputMouseCursor_MAX ];
	HCURSOR			currentCursor;

	int64_t			tickFrequency;
};

struct ImAppThread
{
	ImAppPlatform*	platform;

	HANDLE			handle;

	ImAppThreadFunc	func;
	void*			arg;

	ImAppAtomic32	isRunning;
};

static const LPWSTR s_windowsSystemCursorMapping[] =
{
	IDC_ARROW,
	IDC_WAIT,
	IDC_APPSTARTING,
	IDC_IBEAM,
	IDC_CROSS,
	IDC_UPARROW,
	IDC_SIZENWSE,
	IDC_SIZENESW,
	IDC_SIZEWE,
	IDC_SIZENS,
	IDC_SIZEALL
};
static_assert(IMAPP_ARRAY_COUNT( s_windowsSystemCursorMapping ) == ImUiInputMouseCursor_MAX, "more cursors");

int WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	ImAppPlatform platform = { 0 };

	for( uintsize i = 0u; i < ImUiInputKey_MAX; ++i )
	{
		const ImUiInputKey keyValue = (ImUiInputKey)i;

		uint8 scanCode = 0u;
		switch( keyValue )
		{
		case ImUiInputKey_None:				break;
		case ImUiInputKey_A:				scanCode = 0x41; break;
		case ImUiInputKey_B:				scanCode = 0x42; break;
		case ImUiInputKey_C:				scanCode = 0x43; break;
		case ImUiInputKey_D:				scanCode = 0x44; break;
		case ImUiInputKey_E:				scanCode = 0x45; break;
		case ImUiInputKey_F:				scanCode = 0x46; break;
		case ImUiInputKey_G:				scanCode = 0x47; break;
		case ImUiInputKey_H:				scanCode = 0x48; break;
		case ImUiInputKey_I:				scanCode = 0x49; break;
		case ImUiInputKey_J:				scanCode = 0x4a; break;
		case ImUiInputKey_K:				scanCode = 0x4b; break;
		case ImUiInputKey_L:				scanCode = 0x4c; break;
		case ImUiInputKey_M:				scanCode = 0x4d; break;
		case ImUiInputKey_N:				scanCode = 0x4e; break;
		case ImUiInputKey_O:				scanCode = 0x4f; break;
		case ImUiInputKey_P:				scanCode = 0x50; break;
		case ImUiInputKey_Q:				scanCode = 0x51; break;
		case ImUiInputKey_R:				scanCode = 0x52; break;
		case ImUiInputKey_S:				scanCode = 0x53; break;
		case ImUiInputKey_T:				scanCode = 0x54; break;
		case ImUiInputKey_U:				scanCode = 0x55; break;
		case ImUiInputKey_V:				scanCode = 0x56; break;
		case ImUiInputKey_W:				scanCode = 0x57; break;
		case ImUiInputKey_X:				scanCode = 0x58; break;
		case ImUiInputKey_Y:				scanCode = 0x59; break;
		case ImUiInputKey_Z:				scanCode = 0x5a; break;
		case ImUiInputKey_1:				scanCode = 0x31; break;
		case ImUiInputKey_2:				scanCode = 0x32; break;
		case ImUiInputKey_3:				scanCode = 0x33; break;
		case ImUiInputKey_4:				scanCode = 0x34; break;
		case ImUiInputKey_5:				scanCode = 0x35; break;
		case ImUiInputKey_6:				scanCode = 0x36; break;
		case ImUiInputKey_7:				scanCode = 0x37; break;
		case ImUiInputKey_8:				scanCode = 0x38; break;
		case ImUiInputKey_9:				scanCode = 0x39; break;
		case ImUiInputKey_0:				scanCode = 0x30; break;
		case ImUiInputKey_Enter:			scanCode = VK_RETURN; break;
		case ImUiInputKey_Escape:			scanCode = VK_ESCAPE; break;
		case ImUiInputKey_Backspace:		scanCode = VK_BACK; break;
		case ImUiInputKey_Tab:				scanCode = VK_TAB; break;
		case ImUiInputKey_Space:			scanCode = VK_SPACE; break;
		case ImUiInputKey_LeftShift:		scanCode = VK_LSHIFT; break;
		case ImUiInputKey_RightShift:		scanCode = VK_RSHIFT; break;
		case ImUiInputKey_LeftControl:		scanCode = VK_LCONTROL; break;
		case ImUiInputKey_RightControl:		scanCode = VK_RCONTROL; break;
		case ImUiInputKey_LeftAlt:			scanCode = VK_LMENU; break;
		case ImUiInputKey_RightAlt:			scanCode = VK_RMENU; break;
		case ImUiInputKey_Minus:			scanCode = VK_OEM_MINUS; break;
		case ImUiInputKey_Equals:			scanCode = VK_OEM_PLUS; break;
		case ImUiInputKey_LeftBracket:		scanCode = VK_OEM_4; break;
		case ImUiInputKey_RightBracket:		scanCode = VK_OEM_6; break;
		case ImUiInputKey_Backslash:		scanCode = VK_OEM_5; break;
		case ImUiInputKey_Semicolon:		scanCode = VK_OEM_1; break;
		case ImUiInputKey_Apostrophe:		scanCode = VK_OEM_7; break;
		case ImUiInputKey_Grave:			scanCode = VK_OEM_3; break;
		case ImUiInputKey_Comma:			scanCode = VK_OEM_COMMA; break;
		case ImUiInputKey_Period:			scanCode = VK_OEM_PERIOD; break;
		case ImUiInputKey_Slash:			scanCode = VK_OEM_2; break;
		case ImUiInputKey_F1:				scanCode = VK_F1; break;
		case ImUiInputKey_F2:				scanCode = VK_F2; break;
		case ImUiInputKey_F3:				scanCode = VK_F3; break;
		case ImUiInputKey_F4:				scanCode = VK_F4; break;
		case ImUiInputKey_F5:				scanCode = VK_F5; break;
		case ImUiInputKey_F6:				scanCode = VK_F6; break;
		case ImUiInputKey_F7:				scanCode = VK_F7; break;
		case ImUiInputKey_F8:				scanCode = VK_F8; break;
		case ImUiInputKey_F9:				scanCode = VK_F9; break;
		case ImUiInputKey_F10:				scanCode = VK_F10; break;
		case ImUiInputKey_F11:				scanCode = VK_F11; break;
		case ImUiInputKey_F12:				scanCode = VK_F12; break;
		case ImUiInputKey_Print:			scanCode = VK_PRINT; break;
		case ImUiInputKey_Pause:			scanCode = VK_PAUSE; break;
		case ImUiInputKey_Insert:			scanCode = VK_INSERT; break;
		case ImUiInputKey_Delete:			scanCode = VK_DELETE; break;
		case ImUiInputKey_Home:				scanCode = VK_HOME; break;
		case ImUiInputKey_End:				scanCode = VK_END; break;
		case ImUiInputKey_PageUp:			scanCode = VK_PRIOR; break;
		case ImUiInputKey_PageDown:			scanCode = VK_NEXT; break;
		case ImUiInputKey_Up:				scanCode = VK_UP; break;
		case ImUiInputKey_Left:				scanCode = VK_LEFT; break;
		case ImUiInputKey_Down:				scanCode = VK_DOWN; break;
		case ImUiInputKey_Right:			scanCode = VK_RIGHT; break;
		case ImUiInputKey_Numpad_Divide:	scanCode = VK_DIVIDE; break;
		case ImUiInputKey_Numpad_Multiply:	scanCode = VK_MULTIPLY; break;
		case ImUiInputKey_Numpad_Minus:		scanCode = VK_SUBTRACT; break;
		case ImUiInputKey_Numpad_Plus:		scanCode = VK_ADD; break;
		case ImUiInputKey_Numpad_Enter:		break;
		case ImUiInputKey_Numpad_1:			scanCode = VK_NUMPAD1; break;
		case ImUiInputKey_Numpad_2:			scanCode = VK_NUMPAD2; break;
		case ImUiInputKey_Numpad_3:			scanCode = VK_NUMPAD3; break;
		case ImUiInputKey_Numpad_4:			scanCode = VK_NUMPAD4; break;
		case ImUiInputKey_Numpad_5:			scanCode = VK_NUMPAD5; break;
		case ImUiInputKey_Numpad_6:			scanCode = VK_NUMPAD6; break;
		case ImUiInputKey_Numpad_7:			scanCode = VK_NUMPAD7; break;
		case ImUiInputKey_Numpad_8:			scanCode = VK_NUMPAD8; break;
		case ImUiInputKey_Numpad_9:			scanCode = VK_NUMPAD9; break;
		case ImUiInputKey_Numpad_0:			scanCode = VK_NUMPAD0; break;
		case ImUiInputKey_Numpad_Period:	scanCode = VK_SEPARATOR; break;
		case ImUiInputKey_MAX:				break;
		}

		platform.inputKeyMapping[ scanCode ] = keyValue;
	}

	int argc;
	char** argv = NULL;
	{
		const wchar_t* pWideCommandLine = GetCommandLineW();
		wchar_t** ppWideArgs = CommandLineToArgvW( pWideCommandLine, &argc );

		argv = (char**)malloc( sizeof( char* ) * argc );

		for( uintsize i = 0u; i < argc; ++i )
		{
			const int argLength = WideCharToMultiByte( CP_UTF8, 0u, ppWideArgs[ i ], -1, NULL, 0, NULL, NULL );

			argv[ i ] = (char*)malloc( argLength + 1u );
			WideCharToMultiByte( CP_UTF8, 0u, ppWideArgs[ i ], -1, argv[ i ], argLength, NULL, NULL );
		}
		LocalFree( ppWideArgs );
	}

	platform.fontBasePathLength = GetWindowsDirectoryW( platform.fontPath, IMAPP_ARRAY_COUNT( platform.fontPath ) );
	{
		const wchar_t fontsPath[] = L"\\Fonts\\";
		wcscpy_s( platform.fontPath + platform.fontBasePathLength, IMAPP_ARRAY_COUNT( platform.fontPath ) - platform.fontBasePathLength, fontsPath );
		platform.fontBasePathLength += IMAPP_ARRAY_COUNT( fontsPath ) - 1u;
	}

	const int result = ImAppMain( &platform, argc, argv );

	for( uintsize i = 0u; i < argc; ++i )
	{
		free( argv[ i ] );
	}
	free( argv );

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->cursors ); ++i )
	{
		platform->cursors[ i ] = LoadCursor( NULL, s_windowsSystemCursorMapping[ i ] );
	}
	platform->currentCursor = platform->cursors[ ImUiInputMouseCursor_Arrow ];

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
			IMAPP_DEBUG_LOGE( "Failed to convert resource path!" );
			return false;
		}

		for( uintsize i = 0; i < convertResult - 1; ++i )
		{
			if( pTargetPath[ i ] == L'/' )
			{
				pTargetPath[ i ] = L'\\';
			}
		}

		platform->resourceBasePathLength += (uintsize)convertResult - 1;

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

	LARGE_INTEGER performanceCounterFrequency;
	QueryPerformanceFrequency( &performanceCounterFrequency );
	platform->tickFrequency = performanceCounterFrequency.QuadPart / 1000;

	return true;
}

void ImAppPlatformShutdown( ImAppPlatform* platform )
{
	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->cursors ); ++i )
	{
		DestroyCursor( platform->cursors[ i ] );
		platform->cursors[ i ] = NULL;
	}

	platform->allocator = NULL;
}

sint64 ImAppPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickInterval )
{
	LARGE_INTEGER currentPerformanceCounterValue;
	QueryPerformanceCounter( &currentPerformanceCounterValue );

	sint64 currentTickValue	= currentPerformanceCounterValue.QuadPart / platform->tickFrequency;
	const sint64 deltaTicks	= currentTickValue - lastTickValue;
	sint64 waitTicks		= IMUI_MAX( tickInterval, deltaTicks ) - deltaTicks;

	if( tickInterval == 0u )
	{
		waitTicks = INFINITE;
	}

	if( waitTicks > 1u )
	{
		MsgWaitForMultipleObjects( 0, NULL, FALSE, (DWORD)waitTicks - 1u, QS_ALLEVENTS );

		QueryPerformanceCounter( &currentPerformanceCounterValue );
		currentTickValue = currentPerformanceCounterValue.QuadPart / platform->tickFrequency;
	}

	return currentTickValue;
}

void ImAppPlatformShowError( ImAppPlatform* platform, const char* message )
{
	MessageBoxA( NULL, message, "I'm App", MB_ICONSTOP );
}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
	platform->currentCursor = platform->cursors[ cursor ];
	SetCursor( platform->currentCursor );
}

static const wchar_t* s_pWindowClass = L"ImAppWindowClass";

struct ImAppWindow
{
	ImAppPlatform*		platform;

	HWND				hwnd;
	HDC					hdc;
	HGLRC				hglrc;

	bool				isOpen;
	int					x;
	int					y;
	int					width;
	int					height;
	ImAppWindowState	state;

	ImAppEventQueue		eventQueue;
};

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->platform = platform;

	wchar_t wideWindowTitle[ 256u ];
	MultiByteToWideChar( CP_UTF8, 0u, windowTitle, -1, wideWindowTitle, IMAPP_ARRAY_COUNT( wideWindowTitle ) - 1 );

	const HINSTANCE	hInstance = GetModuleHandle( NULL );

	WNDCLASSEXW windowClass = { 0 };
	windowClass.cbSize			= sizeof( WNDCLASSEXW );
	windowClass.hInstance		= hInstance;
	windowClass.lpfnWndProc		= &ImAppPlatformWindowProc;
	windowClass.lpszClassName	= s_pWindowClass;
	windowClass.hbrBackground	= (HBRUSH)COLOR_WINDOW;
	windowClass.hCursor			= platform->cursors[ ImUiInputMouseCursor_Arrow ];

	const HRESULT result = RegisterClassExW( &windowClass );
	if( FAILED( result ) )
	{
		IMAPP_DEBUG_LOGE( "Can't register Window Class." );
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	window->hwnd = CreateWindowW(
		s_pWindowClass,
		wideWindowTitle,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		width,
		height,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if( window->hwnd == NULL )
	{
		IMAPP_DEBUG_LOGE( "Can't create Window." );
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	SetWindowLongPtr( window->hwnd, GWLP_USERDATA, (LONG_PTR)window );

	int showState = SW_SHOWNORMAL;
	switch( state )
	{
	case ImAppWindowState_Default: break;
	case ImAppWindowState_Maximized: showState = SW_SHOWMAXIMIZED; break;
	case ImAppWindowState_Minimized: showState = SW_SHOWMINIMIZED; break;
	}
	ShowWindow( window->hwnd, showState );

	RECT clientRect;
	GetClientRect( window->hwnd, &clientRect );

	window->hdc			= GetDC( window->hwnd );
	window->width		= (clientRect.right - clientRect.left);
	window->height		= (clientRect.bottom - clientRect.top);

	ImAppEventQueueConstruct( &window->eventQueue, platform->allocator );

	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{
	ImAppEventQueueDestruct( &window->eventQueue );

	if( window->hwnd )
	{
		DestroyWindow( window->hwnd );
		window->hwnd = NULL;
	}

	ImUiMemoryFree( window->platform->allocator, window );
}

bool ImAppPlatformWindowCreateGlContext( ImAppWindow* window )
{
	PIXELFORMATDESCRIPTOR pfd;
	ZeroMemory( &pfd, sizeof( pfd ) );
	pfd.nSize		= sizeof( pfd );
	pfd.nVersion	= 1;
	pfd.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType	= PFD_TYPE_RGBA;
	pfd.cColorBits	= 24;
	pfd.cAlphaBits	= 8;
	pfd.iLayerType	= PFD_MAIN_PLANE;

	const int format = ChoosePixelFormat( window->hdc, &pfd );
	if( !SetPixelFormat( window->hdc, format, &pfd ) )
	{
		IMAPP_DEBUG_LOGE( "Failed to set pixel format." );
		return false;
	}

	window->hglrc = wglCreateContext( window->hdc );
	if( window->hglrc == NULL )
	{
		IMAPP_DEBUG_LOGE( "Failed to create GL context." );
		return false;
	}

	if( !wglMakeCurrent( window->hdc, window->hglrc ) )
	{
		IMAPP_DEBUG_LOGE( "Failed to activate GL context." );
		return false;
	}

	return true;
}

void ImAppPlatformWindowDestroyGlContext( ImAppWindow* window )
{
	wglMakeCurrent( NULL, NULL );

	wglDeleteContext( window->hglrc );
	window->hglrc = NULL;
}

void ImAppPlatformWindowUpdate( ImAppWindow* window )
{
	MSG message;
	while( PeekMessage( &message, NULL, 0u, 0u, PM_REMOVE ) )
	{
		TranslateMessage( &message );
		DispatchMessage( &message );
	}
}

bool ImAppPlatformWindowPresent( ImAppWindow* window )
{
	return wglSwapLayerBuffers( window->hdc, WGL_SWAP_MAIN_PLANE );
}

ImAppEventQueue* ImAppPlatformWindowGetEventQueue( ImAppWindow* window )
{
	return &window->eventQueue;
}

void ImAppPlatformWindowGetViewRect( ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	*outX = 0;
	*outY = 0;

	ImAppPlatformWindowGetSize( window, outWidth, outHeight );
}

void ImAppPlatformWindowGetSize( ImAppWindow* window, int* outWidth, int* outHeight )
{
	*outWidth	= window->width;
	*outHeight	= window->height;
}

void ImAppPlatformWindowGetPosition( ImAppWindow* window, int* outX, int* outY )
{
	*outX = window->x;
	*outY = window->y;
}

ImAppWindowState ImAppPlatformWindowGetState( ImAppWindow* window )
{
	return window->state;
}

static LRESULT CALLBACK ImAppPlatformWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	ImAppWindow* window = (ImAppWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	if( window != NULL )
	{
		switch( message )
		{
		case WM_CLOSE:
			{
				const ImAppEvent closeEvent ={ .window = {.type = ImAppEventType_WindowClose } };
				ImAppEventQueuePush( &window->eventQueue, &closeEvent );
			}
			return 0;

		case WM_MOVE:
			{
				window->x = (int)LOWORD( lParam );
				window->y = (int)HIWORD( lParam );
			}
			return 0;

		case WM_SIZE:
			{
				switch( wParam )
				{
				case SIZE_RESTORED:
					window->state = ImAppWindowState_Default;
					break;

				case SIZE_MAXIMIZED:
					window->state = ImAppWindowState_Maximized;
					break;

				case SIZE_MINIMIZED:
					window->state = ImAppWindowState_Minimized;
					break;
				}

				RECT clientRect;
				GetClientRect( hWnd, &clientRect );

				window->width	= (clientRect.right - clientRect.left);
				window->height	= (clientRect.bottom - clientRect.top);
			}
			return 0;

		case WM_SETCURSOR:
			SetCursor( window->platform->currentCursor );
			return TRUE;

		case WM_MOUSEMOVE:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent motionEvent ={ .motion = {.type = ImAppEventType_Motion, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &motionEvent );
			}
			return 0;

		case WM_CHAR:
			{
				if( wParam < L' ' )
				{
					break;
				}

				if( wParam >= 127 )
				{
					const wchar_t sourceString[] ={ (wchar_t)wParam, L'\0' };
					char targetString[ 8u ];
					const int length = WideCharToMultiByte( CP_UTF8, 0u, sourceString, 1, targetString, sizeof( targetString ), NULL, NULL );
					targetString[ length ] = '\0';

					for( int i = 0u; i < length; ++i )
					{
						const ImAppEvent characterEvent ={ .character = {.type = ImAppEventType_Character, .character = targetString[ i ] } };
						ImAppEventQueuePush( &window->eventQueue, &characterEvent );
					}
				}
				else
				{
					const ImAppEvent characterEvent ={ .character = {.type = ImAppEventType_Character, .character = (char)wParam } };
					ImAppEventQueuePush( &window->eventQueue, &characterEvent );
				}
			}
			return 0;

		case WM_KEYDOWN:
			{
				ImUiInputKey key = (ImUiInputKey)window->platform->inputKeyMapping[ (uint8)wParam ];
				if( wParam == VK_RETURN &&
					lParam & 0x1000000 )
				{
					key = ImUiInputKey_Numpad_Enter;
				}

				const ImAppEvent keyEvent ={ .key = {.type = ImAppEventType_KeyDown, .key = key } };
				ImAppEventQueuePush( &window->eventQueue, &keyEvent );
			}
			return 0;

		case WM_KEYUP:
			{
				ImUiInputKey key = (ImUiInputKey)window->platform->inputKeyMapping[ (uint8)wParam ];
				if( wParam == VK_RETURN &&
					lParam & 0x1000000 )
				{
					key = ImUiInputKey_Numpad_Enter;
				}

				const ImAppEvent keyEvent ={ .key = {.type = ImAppEventType_KeyUp, .key = key } };
				ImAppEventQueuePush( &window->eventQueue, &keyEvent );
			}
			return 0;

		case WM_LBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_Left, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_LBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_Left, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_RBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_Right, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_RBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_Right, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_MBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_Middle, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_MBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_Middle, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_XBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_X1, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_XBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent ={ .button = {.type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_X1, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_MOUSEHWHEEL:
			{
				//GET_WHEEL_DELTA_WPARAM
				const int xDelta = GET_X_LPARAM( lParam );
				const int yDelta = GET_Y_LPARAM( lParam );
				const ImAppEvent scrollEvent ={ .scroll = {.type = ImAppEventType_Scroll, .x = xDelta, .y = yDelta } };
				ImAppEventQueuePush( &window->eventQueue, &scrollEvent );
			}
			return 0;

		default:
			break;
		}
	}

	switch( message )
	{
	case WM_DESTROY:
		{
			PostQuitMessage( 0 );
		}
		return 0;

	default:
		break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

ImAppBlob ImAppPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName )
{
	return ImAppPlatformResourceLoadRange( platform, resourceName, 0u, (uintsize)-1 );
}

ImAppBlob ImAppPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	ImAppBlob result = { NULL, 0u };

	wchar_t pathBuffer[ MAX_PATH ];
	wcsncpy_s( pathBuffer, MAX_PATH, platform->resourcePath, platform->resourceBasePathLength );
	{
		wchar_t* pathNamePart = pathBuffer + platform->resourceBasePathLength;
		const uintsize remainingLengthInCharacters = IMAPP_ARRAY_COUNT( pathBuffer ) - platform->resourceBasePathLength;
		MultiByteToWideChar( CP_UTF8, 0, resourceName, -1, pathNamePart, (int)remainingLengthInCharacters );

		for( ; pathNamePart != L'\0'; ++pathNamePart )
		{
			if( *pathNamePart == L'/' )
			{
				*pathNamePart = L'\\';
			}
		}
	}

	const HANDLE fileHandle = CreateFileW( pathBuffer, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( fileHandle == INVALID_HANDLE_VALUE )
	{
		return result;
	}

	if( length == (uintsize)-1 )
	{
		LARGE_INTEGER fileSize;
		GetFileSizeEx( fileHandle, &fileSize );

		length = fileSize.QuadPart - offset;
	}

	if( offset != 0u &&
		SetFilePointer( fileHandle, (LONG)offset, NULL, FILE_BEGIN ) == INVALID_SET_FILE_POINTER )
	{
		CloseHandle( fileHandle );
		return result;
	}

	void* memory = ImUiMemoryAlloc( platform->allocator, length );
	if( !memory )
	{
		CloseHandle( fileHandle );
		return result;
	}

	DWORD bytesRead = 0u;
	const BOOL readResult = ReadFile( fileHandle, memory, (DWORD)length, &bytesRead, NULL );
	CloseHandle( fileHandle );

	if( !readResult || bytesRead != (DWORD)length )
	{
		ImUiMemoryFree( platform->allocator, memory );
		return result;
	}

	result.data	= memory;
	result.size	= length;
	return result;
}

ImAppBlob ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName )
{
	wchar_t* pTargetBuffer = platform->fontPath + platform->fontBasePathLength;
	const uintsize targetLengthInCharacters = IMAPP_ARRAY_COUNT( platform->fontPath ) - platform->fontBasePathLength;
	MultiByteToWideChar( CP_UTF8, 0, fontName, -1, pTargetBuffer, (int)targetLengthInCharacters );

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
		const ImAppBlob result ={ NULL, 0u };
		return result;
	}

	LARGE_INTEGER fileSize;
	GetFileSizeEx( fileHandle, &fileSize );

	void* memory = ImUiMemoryAlloc( platform->allocator, (uintsize)fileSize.QuadPart );

	DWORD bytesRead = 0u;
	const BOOL readResult = ReadFile( fileHandle, memory, (DWORD)fileSize.QuadPart, &bytesRead, NULL );
	CloseHandle( fileHandle );

	if( !readResult || bytesRead != (DWORD)fileSize.QuadPart )
	{
		ImUiMemoryFree( platform->allocator, memory );
		const ImAppBlob result ={ NULL, 0u };
		return result;
	}

	const ImAppBlob result ={ memory, (uintsize)fileSize.QuadPart };
	return result;
}

ImAppThread* ImAppPlatformThreadCreate( ImAppPlatform* platform, const char* name, ImAppThreadFunc func, void* arg )
{
	ImAppThread* thread = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppThread );
	if( !thread )
	{
		return NULL;
	}

	ImAppPlatformAtomicSet( &thread->isRunning, 1 );

	thread->platform	= platform;
	thread->func		= func;
	thread->arg			= arg;
	thread->handle		= CreateThread( NULL, 0u, ImAppPlatformThreadEntry, thread, 0u, NULL );

	//SetThreadDescription( thread->handle, name );

	return thread;
}

void ImAppPlatformThreadDestroy( ImAppThread* thread )
{
	WaitForSingleObject( thread->handle, INFINITE );
	CloseHandle( thread->handle );

	ImUiMemoryFree( thread->platform->allocator, thread );
}

bool ImAppPlatformThreadIsRunning( const ImAppThread* thread )
{
	return ImAppPlatformAtomicGet( &thread->isRunning );
}

static DWORD WINAPI ImAppPlatformThreadEntry( void* voidThread )
{
	ImAppThread* thread = (ImAppThread*)voidThread;

	thread->func( thread->arg );

	ImAppPlatformAtomicSet( &thread->isRunning, 0 );

	return 0u;
}

ImAppMutex* ImAppPlatformMutexCreate( ImAppPlatform* platform )
{
	CRITICAL_SECTION* cs = IMUI_MEMORY_NEW_ZERO( platform->allocator, CRITICAL_SECTION );

	InitializeCriticalSection( cs );

	return (ImAppMutex*)cs;
}

void ImAppPlatformMutexDestroy( ImAppPlatform* platform, ImAppMutex* mutex )
{
	ImUiMemoryFree( platform->allocator, mutex );
}

void ImAppPlatformMutexLock( ImAppMutex* mutex )
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)mutex;
	EnterCriticalSection( cs );
}

void ImAppPlatformMutexUnlock( ImAppMutex* mutex )
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)mutex;
	LeaveCriticalSection( cs );
}

ImAppSemaphore* ImAppPlatformSemaphoreCreate( ImAppPlatform* platform )
{
	return (ImAppSemaphore*)CreateSemaphore( NULL, 0, LONG_MAX, NULL );
}

void ImAppPlatformSemaphoreDestroy( ImAppPlatform* platform, ImAppSemaphore* semaphore )
{
	CloseHandle( (HANDLE)semaphore );
}

void ImAppPlatformSemaphoreInc( ImAppSemaphore* semaphore )
{
	ReleaseSemaphore( (HANDLE)semaphore, 1, NULL );
}

bool ImAppPlatformSemaphoreDec( ImAppSemaphore* semaphore, bool wait )
{
	return WaitForSingleObject( (HANDLE)semaphore, wait ? INFINITE : 0u ) == WAIT_OBJECT_0;
}

uint32 ImAppPlatformAtomicGet( const ImAppAtomic32* atomic )
{
	return InterlockedOrNoFence( (volatile long*)&atomic->value, 0u );
}

uint32 ImAppPlatformAtomicSet( ImAppAtomic32* atomic, uint32 value )
{
	return InterlockedExchangeNoFence( (volatile long*)&atomic->value, value );
}

uint32 ImAppPlatformAtomicInc( ImAppAtomic32* atomic )
{
	return InterlockedIncrementNoFence( (volatile long*)&atomic->value ) - 1u;
}

uint32 ImAppPlatformAtomicDec( ImAppAtomic32* atomic )
{
	return InterlockedDecrementNoFence( (volatile long*)&atomic->value ) + 1u;
}

#endif
