#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS ) && IMAPP_DISABLED( IMAPP_PLATFORM_SDL )

#include "imapp_internal.h"
#include "imapp_debug.h"
#include "imapp_event_queue.h"
#include "imapp_platform_windows_resources.h"

#include <math.h>
#include <windows.h>
#include <shellapi.h>
#include <windowsx.h>

#define CINTERFACE
#include <oleidl.h>

#if IMAPP_ENABLED( IMAPP_DEBUG )
#	include <crtdbg.h>
#endif

typedef struct ImAppFileWatcherPath ImAppFileWatcherPath;

static LRESULT CALLBACK		ImAppPlatformWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
static ImUiInputKey			ImAppPlatformWindowMapKey( ImAppWindow* window, WPARAM wParam, LPARAM lParam );
HRESULT STDMETHODCALLTYPE	ImAppPlatformWindowDropTargetQueryInterface( __RPC__in IDropTarget* This, __RPC__in REFIID riid, _COM_Outptr_ void **ppvObject );
ULONG STDMETHODCALLTYPE		ImAppPlatformWindowDropTargetAddRef( __RPC__in IDropTarget* This );
ULONG STDMETHODCALLTYPE		ImAppPlatformWindowDropTargetRelease( __RPC__in IDropTarget* This );
HRESULT STDMETHODCALLTYPE	ImAppPlatformWindowDropTargetDragEnter( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD *pdwEffect );
HRESULT STDMETHODCALLTYPE	ImAppPlatformWindowDropTargetDragOver( __RPC__in IDropTarget* This, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect );
HRESULT STDMETHODCALLTYPE	ImAppPlatformWindowDropTargetDragLeave( __RPC__in IDropTarget* This );
HRESULT STDMETHODCALLTYPE	ImAppPlatformWindowDropTargetDrop( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD *pdwEffect );

static void					ImAppPlatformFileWatcherPathStart( ImAppFileWatcherPath* path );

static DWORD WINAPI			ImAppPlatformThreadEntry( void* voidThread );

static const wchar_t* s_pWindowClass	= L"ImAppWindowClass";
static IDropTargetVtbl s_dropTargetVtbl	= { ImAppPlatformWindowDropTargetQueryInterface, ImAppPlatformWindowDropTargetAddRef, ImAppPlatformWindowDropTargetRelease, ImAppPlatformWindowDropTargetDragEnter, ImAppPlatformWindowDropTargetDragOver, ImAppPlatformWindowDropTargetDragLeave, ImAppPlatformWindowDropTargetDrop };

struct ImAppPlatform
{
	ImUiAllocator*		allocator;

	uint8				inputKeyMapping[ 223u ];

	wchar_t				resourceBasePath[ MAX_PATH ];
	uintsize			resourceBasePathLength;

	wchar_t				fontBasePath[ MAX_PATH ];
	uintsize			fontBasePathLength;

	HCURSOR				cursors[ ImUiInputMouseCursor_MAX ];
	HCURSOR				currentCursor;

	int64_t				tickFrequency;
};

typedef struct ImAppWindowDrop ImAppWindowDrop;
typedef struct ImAppWindowDrop
{
	ImAppWindowDrop*	nextDrop;

	ImAppDropType		type;
	char				pathOrText[ 1u ];
} ImAppWindowDrop;

struct ImAppWindow
{
	// must be first for cast
	IDropTarget			dropTarget;
	FORMATETC			dropFormat;
	ImAppWindowDrop*	firstNewDrop;
	ImAppWindowDrop*	firstPoppedDrop;

	ImAppPlatform*		platform;

	HWND				hwnd;
	HDC					hdc;
	HGLRC				hglrc;

	bool				hasFocus;
	bool				hasTracking;
	bool				hasSizeChanged;
	bool				isResize;
	int					x;
	int					y;
	int					width;
	int					height;
	ImAppWindowState	state;
	ImAppWindowStyle	style;
	char*				title;
	uintsize			titleCapacity;
	float				dpiScale;

	int					titleHeight;
	int					titleButtonsX;
	LRESULT				windowHitResult;

	ImAppEventQueue		eventQueue;

	ImAppWindowDoUiFunc					uiFunc;
	ImAppPlatformWindowUpdateCallback	updateCallback;
	void*								updateCallbackArg;
};

typedef struct ImAppFileWatcherPath
{
	OVERLAPPED				overlapped;

	ImAppFileWatcherPath*	prevPath;
	ImAppFileWatcherPath*	nextPath;

	bool					isRunning;
	HANDLE					dirHandle;
	byte					buffer[ sizeof( FILE_NOTIFY_INFORMATION ) + (sizeof( wchar_t ) * MAX_PATH) ];

	char					path[ 1u ];
} ImAppFileWatcherPath;

typedef struct ImAppFileWatcher
{
	ImAppPlatform*			platform;

	ImAppFileWatcherPath*	firstPath;

	HANDLE					ioPort;

	char					eventPath[ MAX_PATH * 4u ];
} ImAppFileWatcher;

struct ImAppThread
{
	ImAppPlatform*		platform;

	HANDLE				handle;

	ImAppThreadFunc		func;
	void*				arg;

	volatile LONG		isRunning;
};

static const LPTSTR s_windowsSystemCursorMapping[] =
{
	IDC_ARROW,
	IDC_WAIT,
	IDC_APPSTARTING,
	IDC_IBEAM,
	IDC_CROSS,
	IDC_HAND,
	IDC_SIZENWSE,
	IDC_SIZENESW,
	IDC_SIZEWE,
	IDC_SIZENS,
	IDC_SIZEALL
};
static_assert(IMAPP_ARRAY_COUNT( s_windowsSystemCursorMapping ) == ImUiInputMouseCursor_MAX, "more cursors");

int WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	IMAPP_USE( hInstance );
	IMAPP_USE( hPrevInstance );
	IMAPP_USE( lpCmdLine );
	IMAPP_USE( nShowCmd );

#if IMAPP_ENABLED( IMAPP_DEBUG )
	int tmpFlag = _CrtSetDbgFlag( _CRTDBG_REPORT_FLAG );

	// Turn on leak-checking bit.
	tmpFlag |= _CRTDBG_LEAK_CHECK_DF;

	// Turn off CRT block checking bit.
	tmpFlag |= _CRTDBG_CHECK_ALWAYS_DF;

	// Set flag to the new value.
	_CrtSetDbgFlag( tmpFlag );
	//_CrtSetBreakAlloc( 100 );

	_clearfp();
	unsigned newControl = _EM_OVERFLOW | _EM_ZERODIVIDE | _EM_INVALID;
	_controlfp_s( 0, ~newControl, newControl );
#endif

#if WINVER >= 0x0605
	SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
#endif

	ImAppPlatform platform = { 0 };

#if IMAPP_ENABLED( IMAPP_DEBUG )
	uintsize maxScanCode = 0u;
#endif
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

#if IMAPP_ENABLED( IMAPP_DEBUG )
		maxScanCode = scanCode > maxScanCode ? scanCode : maxScanCode;
#endif

		IMAPP_ASSERT( scanCode < IMAPP_ARRAY_COUNT( platform.inputKeyMapping ) );
		platform.inputKeyMapping[ scanCode ] = keyValue;
	}
	IMAPP_ASSERT( maxScanCode + 1u == IMAPP_ARRAY_COUNT( platform.inputKeyMapping ) );

	int argc;
	char** argv = NULL;
	{
		const wchar_t* pWideCommandLine = GetCommandLineW();
		wchar_t** ppWideArgs = CommandLineToArgvW( pWideCommandLine, &argc );

		argv = (char**)malloc( sizeof( char* ) * (uintsize)argc );

		for( int i = 0; i < argc; ++i )
		{
			const int argLength = WideCharToMultiByte( CP_UTF8, 0u, ppWideArgs[ i ], -1, NULL, 0, NULL, NULL );

			argv[ i ] = (char*)malloc( argLength + 1u );
			WideCharToMultiByte( CP_UTF8, 0u, ppWideArgs[ i ], -1, argv[ i ], argLength, NULL, NULL );
		}
		LocalFree( ppWideArgs );
	}

	platform.fontBasePathLength = GetWindowsDirectoryW( platform.fontBasePath, IMAPP_ARRAY_COUNT( platform.fontBasePath ) );
	{
		const wchar_t fontsPath[] = L"\\Fonts\\";
		wcscpy_s( platform.fontBasePath + platform.fontBasePathLength, IMAPP_ARRAY_COUNT( platform.fontBasePath ) - platform.fontBasePathLength, fontsPath );
		platform.fontBasePathLength += IMAPP_ARRAY_COUNT( fontsPath ) - 1u;
	}

	const int result = ImAppMain( &platform, argc, argv );

	for( int i = 0; i < argc; ++i )
	{
		free( argv[ i ] );
	}
	free( argv );

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	const HRESULT oleResult = OleInitialize( NULL );
	if( FAILED( oleResult ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to initialize OLE." );
	}

	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->cursors ); ++i )
	{
		platform->cursors[ i ] = LoadCursor( NULL, s_windowsSystemCursorMapping[ i ] );
	}
	platform->currentCursor = platform->cursors[ ImUiInputMouseCursor_Arrow ];

	const char* sourcePath = resourcePath;
	if( sourcePath && strstr( sourcePath, "./" ) == sourcePath )
	{
		GetModuleFileNameW( NULL, platform->resourceBasePath, IMAPP_ARRAY_COUNT( platform->resourceBasePath ) );
		sourcePath += 2u;
	}
	{
		wchar_t* pTargetPath = wcsrchr( platform->resourceBasePath, L'\\' );
		if( pTargetPath == NULL )
		{
			pTargetPath = platform->resourceBasePath;
		}
		else
		{
			pTargetPath++;
		}
		platform->resourceBasePathLength = (pTargetPath - platform->resourceBasePath);

		const int targetLengthInCharacters = IMAPP_ARRAY_COUNT( platform->resourceBasePath ) - (int)platform->resourceBasePathLength;
		const int convertResult = MultiByteToWideChar( CP_UTF8, 0u, sourcePath, -1, pTargetPath, targetLengthInCharacters );
		if( !convertResult )
		{
			IMAPP_DEBUG_LOGE( "Failed to convert resource path!" );
			return false;
		}

		for( int i = 0; i < convertResult - 1; ++i )
		{
			if( pTargetPath[ i ] == L'/' )
			{
				pTargetPath[ i ] = L'\\';
			}
		}

		platform->resourceBasePathLength += (uintsize)convertResult - 1;

		if( platform->resourceBasePath[ platform->resourceBasePathLength - 1u ] != L'/' )
		{
			if( platform->resourceBasePathLength == IMAPP_ARRAY_COUNT( platform->resourceBasePath ) )
			{
				ImAppTrace( "Error: Resource path exceeds limit!\n" );
				return false;
			}

			platform->resourceBasePath[ platform->resourceBasePathLength ] = L'\\';
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

	OleUninitialize();

	platform->allocator = NULL;
}

sint64 ImAppPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickInterval )
{
	IMAPP_USE( platform );

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
	IMAPP_USE( platform );

	MessageBoxA( NULL, message, "I'm App", MB_ICONSTOP );
}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
	platform->currentCursor = platform->cursors[ cursor ];
	SetCursor( platform->currentCursor );
}

void ImAppPlatformSetClipboardText( ImAppPlatform* platform, const char* text )
{
	IMAPP_USE( platform );

	if( !OpenClipboard( NULL ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to open Clipboard." );
		return;
	}

	if( !EmptyClipboard() )
	{
		IMAPP_DEBUG_LOGW( "Failed to clear Clipboard." );
		CloseClipboard();
		return;
	}

	const uintsize textLength = strlen( text );
	HGLOBAL clipboardMemory = GlobalAlloc( GMEM_MOVEABLE, (textLength + 1u) * sizeof( WCHAR ) );
	if( !clipboardMemory )
	{
		IMAPP_DEBUG_LOGW( "Failed to allocate Clipboard memory." );
		CloseClipboard();
		return;
	}

	{
		WCHAR* wideText = (WCHAR*)GlobalLock( clipboardMemory );
		MultiByteToWideChar( CP_UTF8, 0u, text, (int)textLength, wideText, (int)textLength + 1 );
		wideText[ textLength ] = L'\0';
		GlobalUnlock( clipboardMemory );
	}

	if( !SetClipboardData( CF_UNICODETEXT, clipboardMemory ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to set Clipboard." );
	}

	CloseClipboard();
}

void ImAppPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui )
{
	IMAPP_USE( platform );

	if( !OpenClipboard( NULL ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to open Clipboard." );
		return;
	}

	HGLOBAL clipboardMemory = GetClipboardData( CF_UNICODETEXT );
	if( !clipboardMemory )
	{
		CloseClipboard();
		return;
	}

	{
		const WCHAR* wideText = (WCHAR*)GlobalLock( clipboardMemory );
		const size_t textLength = wcslen( wideText );

		char* text = ImUiInputBeginWritePasteText( imui, textLength );
		if( !text )
		{
			GlobalUnlock( clipboardMemory );
			CloseClipboard();
			return;
		}

		WideCharToMultiByte( CP_UTF8, 0u, wideText, (int)textLength, text, (int)textLength + 1, NULL, NULL );
		GlobalUnlock( clipboardMemory );

		ImUiInputEndWritePasteText( imui, textLength );
	}

	CloseClipboard();
}

static void
set_menu_item_state(
	HMENU menu,
	MENUITEMINFO* menuItemInfo,
	UINT item,
	bool enabled
) {
	menuItemInfo->fState = enabled ? MF_ENABLED : MF_DISABLED;
	SetMenuItemInfo( menu, item, false, menuItemInfo );
}

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowStyle style, ImAppWindowState state, ImAppWindowDoUiFunc uiFunc )
{
	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->platform	= platform;
	window->uiFunc		= uiFunc;
	window->hasFocus	= true;
	window->state		= state;
	window->style		= style;

	window->titleHeight = 32;

	const uintsize windowTitleLength = strlen( windowTitle ) + 1u;
	if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( platform->allocator, window->title, window->titleCapacity, windowTitleLength ) )
	{
		IMAPP_DEBUG_LOGE( "Can't allocate title." );
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	strncpy_s( window->title, window->titleCapacity, windowTitle, windowTitleLength );

	wchar_t wideWindowTitle[ 256u ];
	MultiByteToWideChar( CP_UTF8, 0u, windowTitle, -1, wideWindowTitle, IMAPP_ARRAY_COUNT( wideWindowTitle ) - 1 );

	const HINSTANCE	hInstance = GetModuleHandle( NULL );

	WNDCLASSEXW windowClass = { 0 };
	windowClass.cbSize			= sizeof( WNDCLASSEXW );
	windowClass.style			= CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
	windowClass.hInstance		= hInstance;
	windowClass.hIcon			= LoadIcon( hInstance, MAKEINTRESOURCE( IDI_APPICON ) );
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

	DWORD winStyle = WS_OVERLAPPEDWINDOW;
	switch( style )
	{
	case ImAppWindowStyle_Resizable:	break;
	case ImAppWindowStyle_Borderless:	winStyle = WS_POPUP; break;
	case ImAppWindowStyle_Custom:		winStyle = WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME; break;
	}

	IMAPP_USE( x );
	IMAPP_USE( y );

	window->hwnd = CreateWindowExW(
		WS_EX_APPWINDOW,
		s_pWindowClass,
		wideWindowTitle,
		winStyle,
		2200, //CW_USEDEFAULT,
		200,
		0,
		0,
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

#if WINVER >= 0x0605
	window->dpiScale = GetDpiForWindow( window->hwnd ) / (float)USER_DEFAULT_SCREEN_DPI;
#else
	window->dpiScale = 1.0f;
#endif
	SetWindowPos( window->hwnd, HWND_TOP, 0, 0, (int)ceil( width * window->dpiScale ), (int)ceil( height * window->dpiScale ), SWP_NOMOVE );

	{
		RECT windowRect;
		GetWindowRect( window->hwnd, &windowRect );

		window->x = windowRect.left;
		window->y = windowRect.top;

		HANDLE monitorHandle = MonitorFromRect( &windowRect, MONITOR_DEFAULTTONULL );
		if( monitorHandle == NULL )
		{
			monitorHandle = MonitorFromRect( &windowRect, MONITOR_DEFAULTTONEAREST );

			MONITORINFO monitorInfo;
			monitorInfo.cbSize = sizeof( monitorInfo );

			if( GetMonitorInfo( monitorHandle, &monitorInfo ) )
			{
				const int windowWidth	= windowRect.right - windowRect.left;
				const int windowHeight	= windowRect.bottom - windowRect.top;
				const int monitorWidth	= monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left;
				const int monitorHeight	= monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top;
				const int windowX		= monitorInfo.rcMonitor.left + (monitorWidth / 2) - (windowWidth / 2);
				const int windowY		= monitorInfo.rcMonitor.top + (monitorHeight / 2) - (windowHeight / 2);

				SetWindowPos( window->hwnd, NULL, windowX, windowY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE );
			}
		}
	}

	int showState = SW_SHOWNORMAL;
	switch( state )
	{
	case ImAppWindowState_Default: break;
	case ImAppWindowState_Maximized: showState = SW_SHOWMAXIMIZED; break;
	case ImAppWindowState_Minimized: showState = SW_SHOWMINIMIZED; break;
	}
	ShowWindow( window->hwnd, showState );

	{
		RECT clientRect;
		GetClientRect( window->hwnd, &clientRect );

		window->hdc			= GetDC( window->hwnd );
		window->width		= (clientRect.right - clientRect.left);
		window->height		= (clientRect.bottom - clientRect.top);
	}

	ImAppEventQueueConstruct( &window->eventQueue, platform->allocator );

	window->dropTarget.lpVtbl = &s_dropTargetVtbl;
	const HRESULT dropRegisterResult = RegisterDragDrop( window->hwnd, &window->dropTarget );
	if( FAILED( dropRegisterResult ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to register drop target. Result: 0x%08x", dropRegisterResult );
	}

	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{
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

	if( window->hwnd )
	{
		RevokeDragDrop( window->hwnd );

		DestroyWindow( window->hwnd );
		window->hwnd = NULL;
	}

	if( window->title )
	{
		ImUiMemoryFree( window->platform->allocator, window->title );
		window->title = NULL;
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

void ImAppPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg )
{
	window->updateCallback		= callback;
	window->updateCallbackArg	= arg;

	while( window->firstPoppedDrop )
	{
		ImAppWindowDrop* drop = window->firstPoppedDrop;
		window->firstPoppedDrop = drop->nextDrop;

		ImUiMemoryFree( window->platform->allocator, drop );
	}

	MSG message;
	while( PeekMessage( &message, NULL, 0u, 0u, PM_REMOVE ) )
	{
		TranslateMessage( &message );
		DispatchMessage( &message );
	}

	// prevent recursive calls
	window->updateCallback		= NULL;
	window->updateCallbackArg	= NULL;

	callback( window, arg );
}

bool ImAppPlatformWindowPresent( ImAppWindow* window )
{
	return wglSwapLayerBuffers( window->hdc, WGL_SWAP_MAIN_PLANE );
}

ImAppEventQueue* ImAppPlatformWindowGetEventQueue( ImAppWindow* window )
{
	return &window->eventQueue;
}

ImAppWindowDoUiFunc ImAppPlatformWindowGetUiFunc( ImAppWindow* window )
{
	return window->uiFunc;
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

bool ImAppPlatformWindowHasFocus( const ImAppWindow* window )
{
	return window->hasFocus;
}

void ImAppPlatformWindowGetSize( const ImAppWindow* window, int* outWidth, int* outHeight )
{
	*outWidth	= window->width;
	*outHeight	= window->height;
}

void ImAppPlatformWindowSetSize( const ImAppWindow* window, int width, int height )
{
	SetWindowPos( window->hwnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER );
}

void ImAppPlatformWindowGetPosition( const ImAppWindow* window, int* outX, int* outY )
{
	*outX = window->x;
	*outY = window->y;
}

void ImAppPlatformWindowSetPosition( const ImAppWindow* window, int x, int y )
{
	SetWindowPos( window->hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

ImAppWindowState ImAppPlatformWindowGetState( const ImAppWindow* window )
{
	return window->state;
}

void ImAppPlatformWindowSetState( ImAppWindow* window, ImAppWindowState state )
{
	if( window->state == state )
	{
		return;
	}

	int cmdShow = -1;
	switch( state )
	{
	case ImAppWindowState_Default:		cmdShow = SW_RESTORE; break;
	case ImAppWindowState_Maximized:	cmdShow = SW_MAXIMIZE; break;
	case ImAppWindowState_Minimized:	cmdShow = SW_MINIMIZE; break;
	}

	if( cmdShow < 0 )
	{
		return;
	}

	ShowWindow( window->hwnd, cmdShow );
	window->state = state;
}

const char* ImAppPlatformWindowGetTitle( const ImAppWindow* window )
{
	return window->title;
}

void ImAppPlatformWindowSetTitle( ImAppWindow* window, const char* title )
{
	const uintsize windowTitleLength = strlen( title ) + 1u;
	if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( window->platform->allocator, window->title, window->titleCapacity, windowTitleLength ) )
	{
		IMAPP_DEBUG_LOGE( "Can't allocate title." );
		return;
	}

	strncpy_s( window->title, window->titleCapacity, title, windowTitleLength );

	wchar_t wideWindowTitle[ 256u ];
	MultiByteToWideChar( CP_UTF8, 0u, title, -1, wideWindowTitle, IMAPP_ARRAY_COUNT( wideWindowTitle ) - 1 );

	SetWindowTextW( window->hwnd, wideWindowTitle );
}

void ImAppPlatformWindowSetTitleBounds( ImAppWindow* window, int height, int buttonsX )
{
	window->titleHeight		= height;
	window->titleButtonsX	= buttonsX;
}

float ImAppPlatformWindowGetDpiScale( const ImAppWindow* window )
{
	return window->dpiScale;
}

void ImAppPlatformWindowClose( ImAppWindow* window )
{
	SendMessage( window->hwnd, WM_CLOSE, 0, 0 );
}

static LRESULT CALLBACK ImAppPlatformWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	ImAppWindow* window = (ImAppWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	if( window != NULL )
	{
		switch( message )
		{
		case WM_CREATE:
			if( window->style == ImAppWindowStyle_Custom )
			{
				SetWindowPos( window->hwnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER );
			}
			break;

		case WM_CLOSE:
			{
				const ImAppEvent closeEvent = { .window = {.type = ImAppEventType_WindowClose } };
				ImAppEventQueuePush( &window->eventQueue, &closeEvent );
			}
			return 0;

		case WM_SETFOCUS:
			window->hasFocus = true;
			break;

		case WM_KILLFOCUS:
			window->hasFocus = false;
			break;

		case WM_MOVE:
			{
				window->x = (int)GET_X_LPARAM( lParam );
				window->y = (int)GET_Y_LPARAM( lParam );
			}
			return 0;

		case WM_ENTERSIZEMOVE:
			window->isResize = true;
			return 0;

		case WM_EXITSIZEMOVE:
			window->isResize = false;
			window->hasSizeChanged = false;

			window->updateCallback( window, window->updateCallbackArg );
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

				window->width			= (clientRect.right - clientRect.left);
				window->height			= (clientRect.bottom - clientRect.top);
				window->hasSizeChanged	= true;
			}
			return 0;

		case WM_PAINT:
			if( window->isResize )
			{
				window->updateCallback( window, window->updateCallbackArg );
			}
			break;

		case WM_SETCURSOR:
			{
				if( LOWORD( lParam ) == 1 )
				{
					SetCursor( window->platform->currentCursor );
					return TRUE;
				}
			}
			break;

		case WM_DPICHANGED:
			{
				window->dpiScale = HIWORD( wParam ) / (float)USER_DEFAULT_SCREEN_DPI;
				window->hasSizeChanged = true;

				const RECT* targetRect = (const RECT*)lParam;
				SetWindowPos( hWnd, HWND_TOP, targetRect->left, targetRect->top, targetRect->right - targetRect->left, targetRect->bottom - targetRect->top, SWP_NOZORDER );

				if( window->updateCallback )
				{
					window->updateCallback( window, window->updateCallbackArg );
				}
			}
			return 0;

		case WM_MOUSEMOVE:
			{
				if( !window->hasTracking )
				{
					TRACKMOUSEEVENT tme = { 0 };
					tme.cbSize    = sizeof( TRACKMOUSEEVENT );
					tme.dwFlags   = TME_LEAVE;
					tme.hwndTrack = hWnd;

					window->hasTracking = TrackMouseEvent( &tme );
				}

				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent motionEvent = { .motion = {.type = ImAppEventType_Motion, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &motionEvent );
			}
			return 0;

		case WM_MOUSELEAVE:
			{
				const ImAppEvent motionEvent = { .motion = {.type = ImAppEventType_Motion, .x = 0xffffffff, .y = 0xffffffff } };
				ImAppEventQueuePush( &window->eventQueue, &motionEvent );

				window->hasTracking = false;
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
					const wchar_t sourceString[] = { (wchar_t)wParam, L'\0' };
					char targetString[ 8u ];
					const int length = WideCharToMultiByte( CP_UTF8, 0u, sourceString, 1, targetString, sizeof( targetString ), NULL, NULL );
					targetString[ length ] = '\0';

					for( int i = 0u; i < length; ++i )
					{
						const ImAppEvent characterEvent = { .character = {.type = ImAppEventType_Character, .character = targetString[ i ] } };
						ImAppEventQueuePush( &window->eventQueue, &characterEvent );
					}
				}
				else
				{
					const ImAppEvent characterEvent = { .character = {.type = ImAppEventType_Character, .character = (char)wParam } };
					ImAppEventQueuePush( &window->eventQueue, &characterEvent );
				}
			}
			return 0;

		case WM_KEYDOWN:
			{
				const ImUiInputKey key = ImAppPlatformWindowMapKey( window, wParam, lParam );

				const bool repeat = lParam & 0x40000000;
				const ImAppEvent keyEvent = { .key = { .type = ImAppEventType_KeyDown, .key = key, .repeat = repeat } };
				ImAppEventQueuePush( &window->eventQueue, &keyEvent );
			}
			return 0;

		case WM_KEYUP:
			{
				const ImUiInputKey key = ImAppPlatformWindowMapKey( window, wParam, lParam );

				const ImAppEvent keyEvent = { .key = { .type = ImAppEventType_KeyUp, .key = key } };
				ImAppEventQueuePush( &window->eventQueue, &keyEvent );
			}
			return 0;

		case WM_LBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_Left, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				SetCapture( window->hwnd );
			}
			return 0;

		case WM_LBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_Left, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				ReleaseCapture();
			}
			return 0;

		case WM_RBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_Right, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				SetCapture( window->hwnd );
			}
			return 0;

		case WM_RBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_Right, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				ReleaseCapture();
			}
			return 0;

		case WM_MBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_Middle, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				SetCapture( window->hwnd );
			}
			return 0;

		case WM_MBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_Middle, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				ReleaseCapture();
			}
			return 0;

		case WM_XBUTTONDOWN:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_X1, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				SetCapture( window->hwnd );
			}
			return 0;

		case WM_XBUTTONUP:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = { .type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_X1, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

				ReleaseCapture();
			}
			return 0;

		case WM_LBUTTONDBLCLK:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = {.type = ImAppEventType_DoubleClick, .button = ImUiInputMouseButton_Left, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_RBUTTONDBLCLK:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = {.type = ImAppEventType_DoubleClick, .button = ImUiInputMouseButton_Right, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_MBUTTONDBLCLK:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = {.type = ImAppEventType_DoubleClick, .button = ImUiInputMouseButton_Middle, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_XBUTTONDBLCLK:
			{
				const int x = GET_X_LPARAM( lParam );
				const int y = GET_Y_LPARAM( lParam );
				const ImAppEvent buttonEvent = { .button = {.type = ImAppEventType_DoubleClick, .button = ImUiInputMouseButton_X1, .x = x, .y = y } };
				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
			}
			return 0;

		case WM_MOUSEWHEEL:
			{
				const int yDelta = GET_WHEEL_DELTA_WPARAM( wParam ) / WHEEL_DELTA;
				const ImAppEvent scrollEvent = { .scroll = { .type = ImAppEventType_Scroll, .x = 0, .y = yDelta } };
				ImAppEventQueuePush( &window->eventQueue, &scrollEvent );
			}
			return 0;

		case WM_MOUSEHWHEEL:
			{
				const int xDelta = GET_WHEEL_DELTA_WPARAM( wParam ) / WHEEL_DELTA;
				const ImAppEvent scrollEvent = { .scroll = { .type = ImAppEventType_Scroll, .x = xDelta, .y = 0 } };
				ImAppEventQueuePush( &window->eventQueue, &scrollEvent );
			}
			return 0;


		case WM_NCCALCSIZE:
			if( window->style == ImAppWindowStyle_Custom )
			{
				if( !wParam )
				{
					break;
				}

#if WINVER >= 0x0605
				const UINT dpi		= GetDpiForWindow( window->hwnd );
				const int frame_x	= GetSystemMetricsForDpi( SM_CXFRAME, dpi );
				const int frame_y	= GetSystemMetricsForDpi( SM_CYFRAME, dpi );
				const int padding	= GetSystemMetricsForDpi( SM_CXPADDEDBORDER, dpi );
#else
				const int frame_x	= GetSystemMetrics( SM_CXFRAME );
				const int frame_y	= GetSystemMetrics( SM_CYFRAME );
				const int padding	= GetSystemMetrics( SM_CXPADDEDBORDER );
#endif

				NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lParam;
				RECT* requestedClientRect = params->rgrc;

				requestedClientRect->right	-= frame_x + padding;
				requestedClientRect->left	+= frame_x + padding;
				requestedClientRect->bottom	-= frame_y + padding;

				WINDOWPLACEMENT placement = { 0 };
				placement.length = sizeof( WINDOWPLACEMENT );
				if( GetWindowPlacement( window->hwnd, &placement ) &&
					placement.showCmd == SW_SHOWMAXIMIZED )
				{
					requestedClientRect->top += padding;
				}

				return 0;
			}
			break;

		case WM_NCHITTEST:
			if( window->style == ImAppWindowStyle_Custom )
			{
				window->windowHitResult = DefWindowProc( window->hwnd, message, wParam, lParam );
				switch( window->windowHitResult )
				{
				case HTNOWHERE:
				case HTRIGHT:
				case HTLEFT:
				case HTTOPLEFT:
				case HTTOP:
				case HTTOPRIGHT:
				case HTBOTTOMRIGHT:
				case HTBOTTOM:
				case HTBOTTOMLEFT:
					return window->windowHitResult;
				}

#if WINVER >= 0x0605
				const UINT dpi = GetDpiForWindow( window->hwnd );
				const int frame_y = GetSystemMetricsForDpi( SM_CYFRAME, dpi );
				const int padding = GetSystemMetricsForDpi( SM_CXPADDEDBORDER, dpi );
#else
				const int frame_y = GetSystemMetrics( SM_CYFRAME );
				const int padding = GetSystemMetrics( SM_CXPADDEDBORDER );
#endif

				POINT mousePos = { 0 };
				mousePos.x = GET_X_LPARAM( lParam );
				mousePos.y = GET_Y_LPARAM( lParam );
				ScreenToClient( window->hwnd, &mousePos );

				if( mousePos.y > 0 && mousePos.y < frame_y + padding )
				{
					window->windowHitResult = HTTOP;
					return HTTOP;
				}

				if( mousePos.y < window->titleHeight &&
					mousePos.x < window->titleButtonsX )
				{
					window->windowHitResult = HTCAPTION;
					return HTCAPTION;
				}

				window->windowHitResult = HTCLIENT;
				return HTCLIENT;
			}
			break;

		case WM_NCMOUSEMOVE:
			if( window->style == ImAppWindowStyle_Custom )
			{
				POINT mousePos;
				mousePos.x = GET_X_LPARAM( lParam );
				mousePos.y = GET_Y_LPARAM( lParam );
				ScreenToClient( window->hwnd, &mousePos );

				const ImAppEvent motionEvent = { .motion = {.type = ImAppEventType_Motion, .x = mousePos.x, .y = mousePos.y } };
				ImAppEventQueuePush( &window->eventQueue, &motionEvent );

				return 0;
			}
			break;

		case WM_NCLBUTTONDOWN:
			if( window->style == ImAppWindowStyle_Custom )
			{
				POINT mousePos;
				mousePos.x = GET_X_LPARAM( lParam );
				mousePos.y = GET_Y_LPARAM( lParam );
				ScreenToClient( window->hwnd, &mousePos );

				if( window->windowHitResult == HTCLIENT )
				{
					const ImAppEvent buttonEvent = { .button = {.type = ImAppEventType_ButtonDown, .button = ImUiInputMouseButton_Left, .x = mousePos.x, .y = mousePos.y } };
					ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

					return 0;
				}
			}
			break;

		case WM_NCLBUTTONUP:
			if( window->style == ImAppWindowStyle_Custom )
			{
				POINT mousePos;
				mousePos.x = GET_X_LPARAM( lParam );
				mousePos.y = GET_Y_LPARAM( lParam );
				ScreenToClient( window->hwnd, &mousePos );

				if( window->windowHitResult == HTCLIENT )
				{
					const ImAppEvent buttonEvent = { .button = {.type = ImAppEventType_ButtonUp, .button = ImUiInputMouseButton_Left, .x = mousePos.x, .y = mousePos.y } };
					ImAppEventQueuePush( &window->eventQueue, &buttonEvent );

					return 0;
				}
			}
			break;

		case WM_NCRBUTTONUP:
			if( window->style == ImAppWindowStyle_Custom &&
				wParam == HTCAPTION )
			{
				const BOOL isMaximized = window->state == ImAppWindowState_Maximized;
				MENUITEMINFO menu_item_info = {
					.cbSize = sizeof( menu_item_info ),
					.fMask = MIIM_STATE
				};

				const HMENU sysMenu = GetSystemMenu( window->hwnd, false );
				set_menu_item_state( sysMenu, &menu_item_info, SC_RESTORE, isMaximized );
				set_menu_item_state( sysMenu, &menu_item_info, SC_MOVE, !isMaximized );
				set_menu_item_state( sysMenu, &menu_item_info, SC_SIZE, !isMaximized );
				set_menu_item_state( sysMenu, &menu_item_info, SC_MINIMIZE, true );
				set_menu_item_state( sysMenu, &menu_item_info, SC_MAXIMIZE, !isMaximized );
				set_menu_item_state( sysMenu, &menu_item_info, SC_CLOSE, true );

				const BOOL result = TrackPopupMenu( sysMenu, TPM_RETURNCMD, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ), 0, window->hwnd, NULL );
				if( result != 0 )
				{
					PostMessage( window->hwnd, WM_SYSCOMMAND, result, 0 );
				}
			}
			break;

		default:
			break;
		}
	}

	if( message == WM_DESTROY )
	{
		PostQuitMessage( 0 );
		return 0;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

static ImUiInputKey ImAppPlatformWindowMapKey( ImAppWindow* window, WPARAM wParam, LPARAM lParam )
{
	if( wParam > IMAPP_ARRAY_COUNT( window->platform->inputKeyMapping ) )
	{
		return ImUiInputKey_None;
	}

	ImUiInputKey key = (ImUiInputKey)window->platform->inputKeyMapping[ (uint8)wParam ];
	if( wParam == VK_RETURN &&
		lParam & 0x1000000 )
	{
		key = ImUiInputKey_Numpad_Enter;
	}
	else if( wParam == VK_SHIFT )
	{
		key = lParam & 0x1000000 ? ImUiInputKey_RightShift : ImUiInputKey_LeftShift;
	}
	else if( wParam == VK_CONTROL )
	{
		key = lParam & 0x1000000 ? ImUiInputKey_RightControl : ImUiInputKey_LeftControl;
	}
	else if( wParam == VK_MENU )
	{
		key = lParam & 0x1000000 ? ImUiInputKey_RightAlt : ImUiInputKey_LeftAlt;
	}

	return key;
}

HRESULT STDMETHODCALLTYPE ImAppPlatformWindowDropTargetQueryInterface( __RPC__in IDropTarget* This, __RPC__in REFIID riid, _COM_Outptr_ void **ppvObject )
{
	if( riid == &IID_IDropTarget )
	{
		*ppvObject = This;
		return S_OK;
	}

	return S_FALSE;
}

ULONG STDMETHODCALLTYPE ImAppPlatformWindowDropTargetAddRef( __RPC__in IDropTarget* This )
{
	IMAPP_USE( This );

	return 1u;
}

ULONG STDMETHODCALLTYPE ImAppPlatformWindowDropTargetRelease( __RPC__in IDropTarget* This )
{
	IMAPP_USE( This );

	return 1u;
}

HRESULT STDMETHODCALLTYPE ImAppPlatformWindowDropTargetDragEnter( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect )
{
	IMAPP_USE( grfKeyState );
	IMAPP_USE( pt );
	IMAPP_USE( pdwEffect );

	ImAppWindow* window = (ImAppWindow*)This;

	IEnumFORMATETC* formatEnum = NULL;
	pDataObj->lpVtbl->EnumFormatEtc( pDataObj, DATADIR_GET, &formatEnum );

	ULONG count = 0u;
	FORMATETC format;
	bool supported = false;
	while( SUCCEEDED( formatEnum->lpVtbl->Next( formatEnum, 1u, &format, &count ) ) && count > 0u )
	{
		IMAPP_DEBUG_LOGI( "Drop format: %d", format.cfFormat );

		if( format.cfFormat != CF_TEXT &&
			format.cfFormat != CF_HDROP )
		{
			//formatEnum->lpVtbl->Skip( formatEnum, 1u );
			continue;
		}

		supported = true;
		window->dropFormat = format;
		break;
	}

	formatEnum->lpVtbl->Release( formatEnum );

	return supported ? S_OK : E_UNEXPECTED;
}

HRESULT STDMETHODCALLTYPE ImAppPlatformWindowDropTargetDragOver( __RPC__in IDropTarget* This, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect )
{
	IMAPP_USE( This );
	IMAPP_USE( grfKeyState );
	IMAPP_USE( pt );
	IMAPP_USE( pdwEffect );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ImAppPlatformWindowDropTargetDragLeave( __RPC__in IDropTarget* This )
{
	IMAPP_USE( This );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE ImAppPlatformWindowDropTargetDrop( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect )
{
	IMAPP_USE( grfKeyState );
	IMAPP_USE( pt );
	IMAPP_USE( pdwEffect );

	ImAppWindow* window = (ImAppWindow*)This;

	STGMEDIUM medium = { 0u };
	const HRESULT dataResult = pDataObj->lpVtbl->GetData( pDataObj, &window->dropFormat, &medium );
	if( FAILED( dataResult ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to der drop data. Result: 0x%08x", dataResult );
		return dataResult;
	}

	bool handled = false;
	if( window->dropFormat.cfFormat == CF_TEXT &&
		medium.tymed == TYMED_HGLOBAL )
	{
		const size_t dataSize = GlobalSize( medium.hGlobal );

		ImAppWindowDrop* drop = (ImAppWindowDrop*)ImUiMemoryAlloc( window->platform->allocator, sizeof( ImAppWindowDrop ) + dataSize );
		drop->type = ImAppDropType_Text;

		const void* data = (const void*)GlobalLock( medium.hGlobal );
		memcpy( drop->pathOrText, data, dataSize );
		GlobalUnlock( medium.hGlobal );
		drop->pathOrText[ dataSize ] = '\0';

		drop->nextDrop = window->firstNewDrop;
		window->firstNewDrop = drop;

		handled = true;
	}
	else if( window->dropFormat.cfFormat == CF_HDROP &&
			 medium.tymed == TYMED_HGLOBAL )
	{
		HDROP hDrop = (HDROP)medium.hGlobal;

		wchar_t fileName[ MAX_PATH ];
		const UINT fileCount = DragQueryFileW( hDrop, 0xffffffffu, NULL, 0u );
		for( UINT i = 0; i < fileCount; ++i )
		{
			const UINT nameLength = DragQueryFileW( hDrop, i, fileName, IMAPP_ARRAY_COUNT( fileName ) );
			if( nameLength == 0u )
			{
				IMAPP_DEBUG_LOGW( "Failed to get drop file name %d", i );
				continue;
			}

			const int utfNameLength = WideCharToMultiByte( CP_UTF8, 0u, fileName, nameLength, NULL, 0, NULL, NULL );
			ImAppWindowDrop* drop = (ImAppWindowDrop*)ImUiMemoryAlloc( window->platform->allocator, sizeof( ImAppWindowDrop ) + utfNameLength );
			drop->type = ImAppDropType_File;

			WideCharToMultiByte( CP_UTF8, 0u, fileName, nameLength, drop->pathOrText, utfNameLength, NULL, NULL );
			drop->pathOrText[ utfNameLength ] = '\0';

			drop->nextDrop = window->firstNewDrop;
			window->firstNewDrop = drop;
		}

		handled = true;
	}

	ReleaseStgMedium( &medium );

	return handled ? S_OK : S_FALSE;
}

void ImAppPlatformResourceGetPath( ImAppPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName )
{
	const uintsize nameLength = strlen( resourceName );

	const int length = WideCharToMultiByte( CP_UTF8, 0, platform->resourceBasePath, (int)platform->resourceBasePathLength, outPath, (int)pathCapacity, NULL, NULL );
	pathCapacity -= length;
	const uintsize nameCapacity = IMUI_MIN( nameLength, pathCapacity - 1u );
	memcpy( outPath + length, resourceName, nameCapacity );
	const uintsize fullLength = (uintsize)length + nameCapacity;
	outPath[ fullLength ] = '\0';

	for( uintsize i = 0u; i < fullLength; ++i )
	{
		if( outPath[ i ] == '\\' )
		{
			outPath[ i ] = '/';
		}
	}
}

ImAppBlob ImAppPlatformResourceLoad( ImAppPlatform* platform, const char* resourceName )
{
	return ImAppPlatformResourceLoadRange( platform, resourceName, 0u, (uintsize)-1 );
}

ImAppBlob ImAppPlatformResourceLoadRange( ImAppPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	ImAppBlob result = { NULL, 0u };

	ImAppFile* file = ImAppPlatformResourceOpen( platform, resourceName );
	if( !file )
	{
		return result;
	}

	if( length == (uintsize)-1 )
	{
		LARGE_INTEGER fileSize;
		GetFileSizeEx( (HANDLE)file, &fileSize );

		length = fileSize.QuadPart - offset;
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
	wchar_t pathBuffer[ MAX_PATH ];
	wcsncpy_s( pathBuffer, MAX_PATH, platform->resourceBasePath, platform->resourceBasePathLength );
	{
		wchar_t* pathNamePart = pathBuffer + platform->resourceBasePathLength;
		const uintsize remainingLengthInCharacters = IMAPP_ARRAY_COUNT( pathBuffer ) - platform->resourceBasePathLength;
		MultiByteToWideChar( CP_UTF8, 0, resourceName, -1, pathNamePart, (int)remainingLengthInCharacters );

		for( ; *pathNamePart != L'\0'; ++pathNamePart )
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
		return NULL;
	}

	return (ImAppFile*)fileHandle;
}
uintsize ImAppPlatformResourceRead( ImAppFile* file, void* outData, uintsize length, uintsize offset )
{
	const HANDLE fileHandle = (HANDLE)file;

	if( SetFilePointer( fileHandle, (LONG)offset, NULL, FILE_BEGIN ) == INVALID_SET_FILE_POINTER )
	{
		return 0u;
	}

	DWORD bytesRead = 0u;
	const BOOL readResult = ReadFile( fileHandle, outData, (DWORD)length, &bytesRead, NULL );

	if( !readResult || bytesRead != (DWORD)length )
	{
		return 0u;
	}

	return bytesRead;
}

void ImAppPlatformResourceClose( ImAppPlatform* platform, ImAppFile* file )
{
	IMAPP_USE( platform );

	const HANDLE fileHandle = (HANDLE)file;
	CloseHandle( fileHandle );
}

ImAppBlob ImAppPlatformResourceLoadSystemFont( ImAppPlatform* platform, const char* fontName )
{
	wchar_t* pTargetBuffer = platform->fontBasePath + platform->fontBasePathLength;
	const uintsize targetLengthInCharacters = IMAPP_ARRAY_COUNT( platform->fontBasePath ) - platform->fontBasePathLength;
	MultiByteToWideChar( CP_UTF8, 0, fontName, -1, pTargetBuffer, (int)targetLengthInCharacters );

	while( *pTargetBuffer != L'\0' )
	{
		if( *pTargetBuffer == L'/' )
		{
			*pTargetBuffer = L'\\';
		}
		pTargetBuffer++;
	}

	const HANDLE fileHandle = CreateFileW( platform->fontBasePath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
	if( fileHandle == INVALID_HANDLE_VALUE )
	{
		const ImAppBlob result = { NULL, 0u };
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
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	const ImAppBlob result = { memory, (uintsize)fileSize.QuadPart };
	return result;
}

void ImAppPlatformResourceFree( ImAppPlatform* platform, ImAppBlob blob )
{
	ImUiMemoryFree( platform->allocator, blob.data );
}

ImAppFileWatcher* ImAppPlatformFileWatcherCreate( ImAppPlatform* platform )
{
	ImAppFileWatcher* watcher = IMUI_MEMORY_NEW( platform->allocator, ImAppFileWatcher );
	if( !watcher )
	{
		return NULL;
	}

	watcher->platform	= platform;
	watcher->firstPath	= NULL;
	watcher->ioPort		= CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 1u );

	if( watcher->ioPort == NULL )
	{
		ImAppPlatformFileWatcherDestroy( platform, watcher );
		return NULL;
	}

	return watcher;
}

void ImAppPlatformFileWatcherDestroy( ImAppPlatform* platform, ImAppFileWatcher* watcher )
{
	ImAppFileWatcherPath* path = watcher->firstPath;
	ImAppFileWatcherPath* nextPath = NULL;
	while( path )
	{
		nextPath = path->nextPath;
		CancelIo( path->dirHandle );
		CloseHandle( path->dirHandle );
		ImUiMemoryFree( platform->allocator, path );
		path = nextPath;
	}

	if( watcher->ioPort != NULL )
	{
		CloseHandle( watcher->ioPort );
	}

	ImUiMemoryFree( platform->allocator, watcher );
}

void ImAppPlatformFileWatcherAddPath( ImAppFileWatcher* watcher, const char* path )
{
	const char* pathEnd = strrchr( path, '/' );
	const uintsize pathLength = pathEnd ? pathEnd - path : strlen( path );

	for( ImAppFileWatcherPath* watcherPath = watcher->firstPath; watcherPath; watcherPath = watcherPath->nextPath )
	{
		if( strncmp( path, watcherPath->path, pathLength ) == 0 )
		{
			// already exists
			return;
		}
	}

	ImAppFileWatcherPath* watcherPath = (ImAppFileWatcherPath*)ImUiMemoryAllocZero( watcher->platform->allocator, sizeof( ImAppFileWatcherPath ) + pathLength );
	if( !watcherPath )
	{
		return;
	}

	memcpy( watcherPath->path, path, pathLength );

	wchar_t widePathBuffer[ MAX_PATH ];
	const int widePathLength = MultiByteToWideChar( CP_UTF8, 0u, watcherPath->path, (int)pathLength, widePathBuffer, IMAPP_ARRAY_COUNT( widePathBuffer ) );
	widePathBuffer[ widePathLength ] = L'\0';

	for( int i = 0; i < widePathLength; ++i )
	{
		if( widePathBuffer[ i ] == L'/' )
		{
			widePathBuffer[ i ] = L'\\';
		}
	}

	watcherPath->dirHandle = CreateFileW(
		widePathBuffer,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL
	);

	if( watcherPath->dirHandle == INVALID_HANDLE_VALUE )
	{
		IMAPP_DEBUG_LOGE( "Failed to open '%s'. Error: 0x%08x", GetLastError() );
		ImUiMemoryFree( watcher->platform->allocator, watcherPath );
		return;
	}

	const HANDLE ioPort = CreateIoCompletionPort( watcherPath->dirHandle, watcher->ioPort, 0u, 0u );
	if( ioPort == NULL )
	{
		IMAPP_DEBUG_LOGE( "Failed to attach '%s' to I/O completion port. Error: 0x%08x", GetLastError() );
		CloseHandle( watcherPath->dirHandle );
		ImUiMemoryFree( watcher->platform->allocator, watcherPath );
		return;
	}

	ImAppPlatformFileWatcherPathStart( watcherPath );

	watcherPath->nextPath = watcher->firstPath;
	if( watcherPath->nextPath )
	{
		watcherPath->nextPath->prevPath = watcherPath;
	}
	watcher->firstPath = watcherPath;
}

void ImAppPlatformFileWatcherRemovePath( ImAppFileWatcher* watcher, const char* path )
{
	const char* pathEnd = strrchr( path, '/' );
	const uintsize pathLength = pathEnd ? pathEnd - path : strlen( path );

	ImAppFileWatcherPath* watcherPath = NULL;
	for( watcherPath = watcher->firstPath; watcherPath; watcherPath = watcherPath->nextPath )
	{
		if( strncmp( path, watcherPath->path, pathLength ) == 0 )
		{
			break;
		}
	}

	if( !watcherPath )
	{
		return;
	}

	CancelIo( watcherPath->dirHandle );
	CloseHandle( watcherPath->dirHandle );

	if( watcherPath->prevPath )
	{
		watcherPath->prevPath->nextPath = watcherPath->nextPath;
	}

	if( watcherPath->nextPath )
	{
		watcherPath->nextPath->prevPath = watcherPath->prevPath;
	}

	if( watcherPath == watcher->firstPath )
	{
		watcher->firstPath = watcherPath->nextPath;
	}

	ImUiMemoryFree( watcher->platform->allocator, watcherPath );
}

bool ImAppPlatformFileWatcherPopEvent( ImAppFileWatcher* watcher, ImAppFileWatchEvent* outEvent )
{
	ULONG_PTR key;
	DWORD readSize;
	OVERLAPPED* overlapped;
	const BOOL success = GetQueuedCompletionStatus( watcher->ioPort, &readSize, &key, &overlapped, 0 );
	if( !success )
	{
		const DWORD lastError = GetLastError();
		if( lastError != WAIT_TIMEOUT )
		{
			IMAPP_DEBUG_LOGE( "Failed to get completion status. Error: 0x%08x", lastError );
		}
		return false;
	}

	ImAppFileWatcherPath* path = (ImAppFileWatcherPath*)overlapped;
	const FILE_NOTIFY_INFORMATION* fileInfo = (const FILE_NOTIFY_INFORMATION*)path->buffer;

	const int pathLength = WideCharToMultiByte( CP_UTF8, 0, fileInfo->FileName, (int)fileInfo->FileNameLength, watcher->eventPath, IMAPP_ARRAY_COUNT( watcher->eventPath ), NULL, NULL );
	watcher->eventPath[ pathLength ] = '\0';

	outEvent->path = watcher->eventPath;

	ImAppPlatformFileWatcherPathStart( path );

	return true;
}

static void ImAppPlatformFileWatcherPathStart( ImAppFileWatcherPath* path )
{
	const BOOL result = ReadDirectoryChangesW(
		path->dirHandle,
		path->buffer,
		sizeof( path->buffer ),
		true,
		FILE_NOTIFY_CHANGE_LAST_WRITE,
		NULL,
		&path->overlapped,
		NULL
	);

	if( result )
	{
		path->isRunning = true;
	}
	else
	{
		IMAPP_DEBUG_LOGE( "Failed to start watch on '%s'. Error: 0x%08x\n", path->path, GetLastError() );
	}
}

ImAppThread* ImAppPlatformThreadCreate( ImAppPlatform* platform, const char* name, ImAppThreadFunc func, void* arg )
{
	ImAppThread* thread = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppThread );
	if( !thread )
	{
		return NULL;
	}

	InterlockedExchangeNoFence( &thread->isRunning, 1 );

	thread->platform	= platform;
	thread->func		= func;
	thread->arg			= arg;
	thread->handle		= CreateThread( NULL, 0u, ImAppPlatformThreadEntry, thread, 0u, NULL );

	IMAPP_USE( name );
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
	return InterlockedOrNoFence( (volatile LONG*)&thread->isRunning, 0);
}

static DWORD WINAPI ImAppPlatformThreadEntry( void* voidThread )
{
	ImAppThread* thread = (ImAppThread*)voidThread;

	thread->func( thread->arg );

	InterlockedExchangeNoFence( &thread->isRunning, 0 );

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
	IMAPP_USE( platform );

	return (ImAppSemaphore*)CreateSemaphore( NULL, 0, LONG_MAX, NULL );
}

void ImAppPlatformSemaphoreDestroy( ImAppPlatform* platform, ImAppSemaphore* semaphore )
{
	IMAPP_USE( platform );

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

#endif
