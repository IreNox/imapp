#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS ) && IMAPP_DISABLED( IMAPP_PLATFORM_SDL )

#include "imapp_debug.h"
#include "imapp_event_queue.h"
#include "imapp_internal.h"
#include "imapp_livepp.h"
#include "imapp_platform_windows_resources.h"

#include <math.h>
#include <windows.h>
#include <dwmapi.h>
#include <windowsx.h>
#include <shellapi.h>
#include <Xinput.h>
#include <sdkddkver.h>

#define CINTERFACE
#include <oleidl.h>

#if IMAPP_ENABLED( IMAPP_DEBUG )
#	include <crtdbg.h>
#endif

typedef struct ImappFileWatcherPath ImappFileWatcherPath;

struct ImappPlatform
{
	ImuiAllocator*		allocator;

	HINSTANCE			hInstance;
	HWND				contextHwnd;
	HDC					contextDc;
	HGLRC				contextGlrc;

	uint8				inputKeyMapping[ 223u ];

	wchar_t				resourceBasePath[ MAX_PATH ];
	uintsize			resourceBasePathLength;

	wchar_t				fontBasePath[ MAX_PATH ];
	uintsize			fontBasePathLength;

	HCURSOR				cursors[ ImuiInputMouseCursor_MAX ];
	HCURSOR				currentCursor;

	HWND*				windows;
	uintsize			windowCapacity;
	uintsize			windowSize;

	int64_t				tickFrequency;
};

typedef struct ImappWindowDrop ImappWindowDrop;
typedef struct ImappWindowDrop
{
	ImappWindowDrop*	nextDrop;

	ImappDropType		type;
	char				pathOrText[ 1u ];
} ImappWindowDrop;

struct ImappWindow
{
	// must be first for cast
	IDropTarget			dropTarget;
	FORMATETC			dropFormat;
	ImappWindowDrop*	firstNewDrop;
	ImappWindowDrop*	firstPoppedDrop;

	ImappPlatform*		platform;

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
	ImappWindowState	state;
	ImappWindowStyle	style;
	char*				title;
	uintsize			titleCapacity;
	float				dpiScale;

	int					titleHeight;
	int					titleButtonsX;
	LRESULT				windowHitResult;

	XINPUT_STATE		lastControllerState;

	ImappEventQueue		eventQueue;

	imappPlatformWindowUpdateCallback	updateCallback;
	void*								updateCallbackArg;
};

typedef struct ImappFileWatcherPath
{
	OVERLAPPED				overlapped;

	ImappFileWatcherPath*	prevPath;
	ImappFileWatcherPath*	nextPath;

	bool					isRunning;
	HANDLE					dirHandle;
	byte					buffer[ sizeof( FILE_NOTIFY_INFORMATION ) + (sizeof( wchar_t ) * MAX_PATH) ];

	char					path[ 1u ];
} ImappFileWatcherPath;

typedef struct ImappFileWatcher
{
	ImappPlatform*			platform;

	ImappFileWatcherPath*	firstPath;

	HANDLE					ioPort;

	char					eventPath[ MAX_PATH * 4u ];
} ImappFileWatcher;

struct ImappThread
{
	ImappPlatform*		platform;

	HANDLE				handle;

	ImappThreadFunc		func;
	void*				arg;

	volatile LONG		isRunning;
};

static void					imappPlatformSetupKeyboard( ImappPlatform* platform );

static HGLRC				imappPlatformWindowCreateGlContext( HDC windowDc );
static void					imappPlatformWindowUpdateController( ImappWindow* window );
static LRESULT CALLBACK		imappPlatformWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
static ImuiInputKey			imappPlatformWindowMapKey( ImappWindow* window, WPARAM wParam, LPARAM lParam );
HRESULT STDMETHODCALLTYPE	imappPlatformWindowDropTargetQueryInterface( __RPC__in IDropTarget* This, __RPC__in REFIID riid, _COM_Outptr_ void **ppvObject );
ULONG STDMETHODCALLTYPE		imappPlatformWindowDropTargetAddRef( __RPC__in IDropTarget* This );
ULONG STDMETHODCALLTYPE		imappPlatformWindowDropTargetRelease( __RPC__in IDropTarget* This );
HRESULT STDMETHODCALLTYPE	imappPlatformWindowDropTargetDragEnter( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD *pdwEffect );
HRESULT STDMETHODCALLTYPE	imappPlatformWindowDropTargetDragOver( __RPC__in IDropTarget* This, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect );
HRESULT STDMETHODCALLTYPE	imappPlatformWindowDropTargetDragLeave( __RPC__in IDropTarget* This );
HRESULT STDMETHODCALLTYPE	imappPlatformWindowDropTargetDrop( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD *pdwEffect );

static void					imappPlatformFileWatcherPathStart( ImappFileWatcherPath* path );

static DWORD WINAPI			imappPlatformThreadEntry( void* voidThread );

static const wchar_t* s_pWindowDummyGlClass	= L"imappWindowDummyGlClass";
static const wchar_t* s_pWindowClass		= L"imappWindowClass";
static IDropTargetVtbl s_dropTargetVtbl		= { imappPlatformWindowDropTargetQueryInterface, imappPlatformWindowDropTargetAddRef, imappPlatformWindowDropTargetRelease, imappPlatformWindowDropTargetDragEnter, imappPlatformWindowDropTargetDragOver, imappPlatformWindowDropTargetDragLeave, imappPlatformWindowDropTargetDrop };

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
static_assert(IMAPP_ARRAY_COUNT( s_windowsSystemCursorMapping ) == ImuiInputMouseCursor_MAX, "more cursors");

int WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
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

#if IMAPP_ENABLED( IMAPP_LIVEPP )
	imappLivePlusPlusEnable( L"../../externals/https/liveplusplus.tech/LPP_2_11_1/LivePP" );
#endif

#if WINVER >= 0x0605
#	if defined( NTDDI_WIN10_RS2 ) && NTDDI_VERSION >= NTDDI_WIN10_RS2
	SetProcessDpiAwarenessContext( DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 );
#	endif
#endif

	ImappPlatform platform = { 0 };
	platform.hInstance = hInstance;

	imappPlatformSetupKeyboard( &platform );

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

	const int result = imappMain( &platform, argc, argv );

	for( int i = 0; i < argc; ++i )
	{
		free( argv[ i ] );
	}
	free( argv );

	return result;
}

bool imappPlatformInitialize( ImappPlatform* platform, ImuiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	const HRESULT oleResult = OleInitialize( NULL );
	if( FAILED( oleResult ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to initialize OLE." );
	}

	platform->fontBasePathLength = GetWindowsDirectoryW( platform->fontBasePath, IMAPP_ARRAY_COUNT( platform->fontBasePath ) );
	{
		const wchar_t fontsPath[] = L"\\Fonts\\";
		wcscpy_s( platform->fontBasePath + platform->fontBasePathLength, IMAPP_ARRAY_COUNT( platform->fontBasePath ) - platform->fontBasePathLength, fontsPath );
		platform->fontBasePathLength += IMAPP_ARRAY_COUNT( fontsPath ) - 1u;
	}

	// dummy GL context
	{
		WNDCLASSEXW windowClass = { 0 };
		windowClass.cbSize			= sizeof( WNDCLASSEXW );
		windowClass.style			= CS_OWNDC;
		windowClass.hInstance		= platform->hInstance;
		windowClass.lpfnWndProc		= DefWindowProc;
		windowClass.lpszClassName	= s_pWindowDummyGlClass;

		const ATOM result = RegisterClassExW( &windowClass );
		if( result == 0 )
		{
			IMAPP_DEBUG_LOGE( "Can't register Dummy GL Window Class." );
			return false;
		}

		platform->contextHwnd = CreateWindowW( s_pWindowDummyGlClass, L"", WS_POPUP, 0, 0, 1, 1, NULL, NULL, platform->hInstance, NULL );
		if( !platform->contextHwnd )
		{
			IMAPP_DEBUG_LOGE( "Failed to create GL dummy window." );
			return false;
		}

		platform->contextDc = GetDC( platform->contextHwnd );

		platform->contextGlrc = imappPlatformWindowCreateGlContext( platform->contextDc );
		if( !platform->contextGlrc )
		{
			IMAPP_DEBUG_LOGE( "Failed to create GL dummy GL context." );
			return false;
		}

		if( !wglMakeCurrent( platform->contextDc, platform->contextGlrc ) )
		{
			IMAPP_DEBUG_LOGE( "Failed to activate dummy GL context." );
			return false;
		}
	}

	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->cursors ); ++i )
	{
		platform->cursors[ i ] = LoadCursor( NULL, s_windowsSystemCursorMapping[ i ] );
	}
	platform->currentCursor = platform->cursors[ ImuiInputMouseCursor_Arrow ];

	{
		WNDCLASSEXW windowClass = { 0 };
		windowClass.cbSize			= sizeof( WNDCLASSEXW );
		windowClass.style			= CS_HREDRAW | CS_VREDRAW | CS_OWNDC | CS_DBLCLKS;
		windowClass.hInstance		= platform->hInstance;
		windowClass.hIcon			= LoadIcon( platform->hInstance, MAKEINTRESOURCE( IDI_APPICON ) );
		windowClass.lpfnWndProc		= &imappPlatformWindowProc;
		windowClass.lpszClassName	= s_pWindowClass;
		windowClass.hbrBackground	= (HBRUSH)COLOR_WINDOW;
		windowClass.hCursor			= platform->cursors[ ImuiInputMouseCursor_Arrow ];

		const ATOM result = RegisterClassExW( &windowClass );
		if( result == 0 )
		{
			IMAPP_DEBUG_LOGE( "Can't register Window Class." );
			return false;
		}
	}

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
				imappTrace( "Error: Resource path exceeds limit!\n" );
				return false;
			}

			platform->resourceBasePath[ platform->resourceBasePathLength ] = L'\\';
			platform->resourceBasePathLength++;
		}
	}

	LARGE_INTEGER performanceCounterFrequency;
	QueryPerformanceFrequency( &performanceCounterFrequency );
	platform->tickFrequency = performanceCounterFrequency.QuadPart;

	return true;
}

void imappPlatformShutdown( ImappPlatform* platform )
{
	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->cursors ); ++i )
	{
		DestroyCursor( platform->cursors[ i ] );
		platform->cursors[ i ] = NULL;
	}

	if( platform->contextGlrc )
	{
		wglMakeCurrent( NULL, NULL );

		wglDeleteContext( platform->contextGlrc );
		platform->contextGlrc = NULL;
	}

	if( platform->contextHwnd )
	{
		platform->contextDc = NULL;

		DestroyWindow( platform->contextHwnd );
		platform->contextHwnd = NULL;
	}

	OleUninitialize();

	platform->hInstance	= NULL;
	platform->allocator	= NULL;
}

sint64 imappPlatformTick( ImappPlatform* platform, sint64 lastTickValue, sint64 tickIntervalMs )
{
	IMAPP_USE( platform );

#if IMAPP_ENABLED( IMAPP_LIVEPP )
	imappLivePlusPlusUpdate();
#endif

	LARGE_INTEGER currentPerformanceCounterValue;
	QueryPerformanceCounter( &currentPerformanceCounterValue );

	const sint64 deltaTicksMs	= (currentPerformanceCounterValue.QuadPart - lastTickValue) / (platform->tickFrequency / 1000);
	sint64 waitTicks			= IMUI_MAX( tickIntervalMs, deltaTicksMs ) - deltaTicksMs;

	if( tickIntervalMs == 0u )
	{
		waitTicks = INFINITE;
	}

	if( waitTicks > 1u )
	{
		MsgWaitForMultipleObjects( 0, NULL, FALSE, (DWORD)waitTicks - 1u, QS_ALLEVENTS );
		QueryPerformanceCounter( &currentPerformanceCounterValue );
	}

	return currentPerformanceCounterValue.QuadPart;
}

double imappPlatformTicksToSeconds( ImappPlatform* platform, sint64 tickValue )
{
	return tickValue / (double)platform->tickFrequency;
}

void imappPlatformShowError( ImappPlatform* platform, const char* message )
{
	IMAPP_USE( platform );

	MessageBoxA( NULL, message, "I'm App", MB_ICONSTOP );
}

void imappPlatformSetMouseCursor( ImappPlatform* platform, ImuiInputMouseCursor cursor )
{
	platform->currentCursor = platform->cursors[ cursor ];
	SetCursor( platform->currentCursor );
}

void imappPlatformSetClipboardText( ImappPlatform* platform, const char* text )
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

void imappPlatformGetClipboardText( ImappPlatform* platform, ImuiContext* imui )
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

		char* text = imuiInputBeginWritePasteText( imui, textLength );
		if( !text )
		{
			GlobalUnlock( clipboardMemory );
			CloseClipboard();
			return;
		}

		WideCharToMultiByte( CP_UTF8, 0u, wideText, (int)textLength, text, (int)textLength + 1, NULL, NULL );
		GlobalUnlock( clipboardMemory );

		imuiInputEndWritePasteText( imui, textLength );
	}

	CloseClipboard();
}

static void imappPlatformSetupKeyboard( ImappPlatform* platform )
{
#if IMAPP_ENABLED( IMAPP_DEBUG )
	uintsize maxScanCode = 0u;
#endif
	for( uintsize i = 0u; i < ImuiInputKey_MAX; ++i )
	{
		const ImuiInputKey keyValue = (ImuiInputKey)i;

		uint8 scanCode = 0u;
		switch( keyValue )
		{
		case ImuiInputKey_None:				break;
		case ImuiInputKey_A:				scanCode = 0x41; break;
		case ImuiInputKey_B:				scanCode = 0x42; break;
		case ImuiInputKey_C:				scanCode = 0x43; break;
		case ImuiInputKey_D:				scanCode = 0x44; break;
		case ImuiInputKey_E:				scanCode = 0x45; break;
		case ImuiInputKey_F:				scanCode = 0x46; break;
		case ImuiInputKey_G:				scanCode = 0x47; break;
		case ImuiInputKey_H:				scanCode = 0x48; break;
		case ImuiInputKey_I:				scanCode = 0x49; break;
		case ImuiInputKey_J:				scanCode = 0x4a; break;
		case ImuiInputKey_K:				scanCode = 0x4b; break;
		case ImuiInputKey_L:				scanCode = 0x4c; break;
		case ImuiInputKey_M:				scanCode = 0x4d; break;
		case ImuiInputKey_N:				scanCode = 0x4e; break;
		case ImuiInputKey_O:				scanCode = 0x4f; break;
		case ImuiInputKey_P:				scanCode = 0x50; break;
		case ImuiInputKey_Q:				scanCode = 0x51; break;
		case ImuiInputKey_R:				scanCode = 0x52; break;
		case ImuiInputKey_S:				scanCode = 0x53; break;
		case ImuiInputKey_T:				scanCode = 0x54; break;
		case ImuiInputKey_U:				scanCode = 0x55; break;
		case ImuiInputKey_V:				scanCode = 0x56; break;
		case ImuiInputKey_W:				scanCode = 0x57; break;
		case ImuiInputKey_X:				scanCode = 0x58; break;
		case ImuiInputKey_Y:				scanCode = 0x59; break;
		case ImuiInputKey_Z:				scanCode = 0x5a; break;
		case ImuiInputKey_1:				scanCode = 0x31; break;
		case ImuiInputKey_2:				scanCode = 0x32; break;
		case ImuiInputKey_3:				scanCode = 0x33; break;
		case ImuiInputKey_4:				scanCode = 0x34; break;
		case ImuiInputKey_5:				scanCode = 0x35; break;
		case ImuiInputKey_6:				scanCode = 0x36; break;
		case ImuiInputKey_7:				scanCode = 0x37; break;
		case ImuiInputKey_8:				scanCode = 0x38; break;
		case ImuiInputKey_9:				scanCode = 0x39; break;
		case ImuiInputKey_0:				scanCode = 0x30; break;
		case ImuiInputKey_Enter:			scanCode = VK_RETURN; break;
		case ImuiInputKey_Escape:			scanCode = VK_ESCAPE; break;
		case ImuiInputKey_Backspace:		scanCode = VK_BACK; break;
		case ImuiInputKey_Tab:				scanCode = VK_TAB; break;
		case ImuiInputKey_Space:			scanCode = VK_SPACE; break;
		case ImuiInputKey_LeftShift:		scanCode = VK_LSHIFT; break;
		case ImuiInputKey_RightShift:		scanCode = VK_RSHIFT; break;
		case ImuiInputKey_LeftControl:		scanCode = VK_LCONTROL; break;
		case ImuiInputKey_RightControl:		scanCode = VK_RCONTROL; break;
		case ImuiInputKey_LeftAlt:			scanCode = VK_LMENU; break;
		case ImuiInputKey_RightAlt:			scanCode = VK_RMENU; break;
		case ImuiInputKey_Minus:			scanCode = VK_OEM_MINUS; break;
		case ImuiInputKey_Equals:			scanCode = VK_OEM_PLUS; break;
		case ImuiInputKey_LeftBracket:		scanCode = VK_OEM_4; break;
		case ImuiInputKey_RightBracket:		scanCode = VK_OEM_6; break;
		case ImuiInputKey_Backslash:		scanCode = VK_OEM_5; break;
		case ImuiInputKey_Semicolon:		scanCode = VK_OEM_1; break;
		case ImuiInputKey_Apostrophe:		scanCode = VK_OEM_7; break;
		case ImuiInputKey_Grave:			scanCode = VK_OEM_3; break;
		case ImuiInputKey_Comma:			scanCode = VK_OEM_COMMA; break;
		case ImuiInputKey_Period:			scanCode = VK_OEM_PERIOD; break;
		case ImuiInputKey_Slash:			scanCode = VK_OEM_2; break;
		case ImuiInputKey_F1:				scanCode = VK_F1; break;
		case ImuiInputKey_F2:				scanCode = VK_F2; break;
		case ImuiInputKey_F3:				scanCode = VK_F3; break;
		case ImuiInputKey_F4:				scanCode = VK_F4; break;
		case ImuiInputKey_F5:				scanCode = VK_F5; break;
		case ImuiInputKey_F6:				scanCode = VK_F6; break;
		case ImuiInputKey_F7:				scanCode = VK_F7; break;
		case ImuiInputKey_F8:				scanCode = VK_F8; break;
		case ImuiInputKey_F9:				scanCode = VK_F9; break;
		case ImuiInputKey_F10:				scanCode = VK_F10; break;
		case ImuiInputKey_F11:				scanCode = VK_F11; break;
		case ImuiInputKey_F12:				scanCode = VK_F12; break;
		case ImuiInputKey_Print:			scanCode = VK_PRINT; break;
		case ImuiInputKey_Pause:			scanCode = VK_PAUSE; break;
		case ImuiInputKey_Insert:			scanCode = VK_INSERT; break;
		case ImuiInputKey_Delete:			scanCode = VK_DELETE; break;
		case ImuiInputKey_Home:				scanCode = VK_HOME; break;
		case ImuiInputKey_End:				scanCode = VK_END; break;
		case ImuiInputKey_PageUp:			scanCode = VK_PRIOR; break;
		case ImuiInputKey_PageDown:			scanCode = VK_NEXT; break;
		case ImuiInputKey_Up:				scanCode = VK_UP; break;
		case ImuiInputKey_Left:				scanCode = VK_LEFT; break;
		case ImuiInputKey_Down:				scanCode = VK_DOWN; break;
		case ImuiInputKey_Right:			scanCode = VK_RIGHT; break;
		case ImuiInputKey_Numpad_Divide:	scanCode = VK_DIVIDE; break;
		case ImuiInputKey_Numpad_Multiply:	scanCode = VK_MULTIPLY; break;
		case ImuiInputKey_Numpad_Minus:		scanCode = VK_SUBTRACT; break;
		case ImuiInputKey_Numpad_Plus:		scanCode = VK_ADD; break;
		case ImuiInputKey_Numpad_Enter:		break;
		case ImuiInputKey_Numpad_1:			scanCode = VK_NUMPAD1; break;
		case ImuiInputKey_Numpad_2:			scanCode = VK_NUMPAD2; break;
		case ImuiInputKey_Numpad_3:			scanCode = VK_NUMPAD3; break;
		case ImuiInputKey_Numpad_4:			scanCode = VK_NUMPAD4; break;
		case ImuiInputKey_Numpad_5:			scanCode = VK_NUMPAD5; break;
		case ImuiInputKey_Numpad_6:			scanCode = VK_NUMPAD6; break;
		case ImuiInputKey_Numpad_7:			scanCode = VK_NUMPAD7; break;
		case ImuiInputKey_Numpad_8:			scanCode = VK_NUMPAD8; break;
		case ImuiInputKey_Numpad_9:			scanCode = VK_NUMPAD9; break;
		case ImuiInputKey_Numpad_0:			scanCode = VK_NUMPAD0; break;
		case ImuiInputKey_Numpad_Period:	scanCode = VK_SEPARATOR; break;
		case ImuiInputKey_MAX:				break;
		}

#if IMAPP_ENABLED( IMAPP_DEBUG )
		maxScanCode = scanCode > maxScanCode ? scanCode : maxScanCode;
#endif

		IMAPP_ASSERT( scanCode < IMAPP_ARRAY_COUNT( platform->inputKeyMapping ) );
		platform->inputKeyMapping[ scanCode ] = keyValue;
	}
	IMAPP_ASSERT( maxScanCode + 1u == IMAPP_ARRAY_COUNT( platform->inputKeyMapping ) );
}

static void imappPlatformWindowSetMenuItemState( HMENU menu, MENUITEMINFO* menuItemInfo, UINT item, bool enabled )
{
	menuItemInfo->fState = enabled ? MF_ENABLED : MF_DISABLED;
	SetMenuItemInfo( menu, item, false, menuItemInfo );
}

ImappWindow* imappPlatformWindowCreate( ImappPlatform* platform, const ImappWindowParameters* parameters )
{
	ImappWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImappWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->platform	= platform;
	window->hasFocus	= true;
	window->state		= parameters->state;
	window->style		= parameters->style;

	window->titleHeight = 32;

	const uintsize windowTitleLength = strlen( parameters->title ) + 1u;
	if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( platform->allocator, window->title, window->titleCapacity, windowTitleLength ) )
	{
		IMAPP_DEBUG_LOGE( "Can't allocate title." );
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	strncpy_s( window->title, window->titleCapacity, parameters->title, windowTitleLength );

	wchar_t wideWindowTitle[ 256u ];
	MultiByteToWideChar( CP_UTF8, 0u, parameters->title, -1, wideWindowTitle, IMAPP_ARRAY_COUNT( wideWindowTitle ) - 1 );

	DWORD winStyle = WS_OVERLAPPEDWINDOW;
	switch( parameters->style )
	{
	case ImappWindowStyle_Resizable:	break;
	case ImappWindowStyle_Borderless:	winStyle = WS_POPUP; break;
	case ImappWindowStyle_Custom:		winStyle = WS_OVERLAPPED | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_SYSMENU | WS_THICKFRAME; break;
	}

	int x = parameters->x;
	int y = parameters->y;
	if( x == IMAPP_WINDOW_DEFAULT_POSITION )
	{
		x = CW_USEDEFAULT;
	}
	if( y == IMAPP_WINDOW_DEFAULT_POSITION )
	{
		y = CW_USEDEFAULT;
	}

	window->hwnd = CreateWindowExW(
		WS_EX_APPWINDOW,
		s_pWindowClass,
		wideWindowTitle,
		winStyle,
		x,
		y,
		0,
		0,
		NULL,
		NULL,
		platform->hInstance,
		window
	);

	if( window->hwnd == NULL )
	{
		IMAPP_DEBUG_LOGE( "Can't create Window." );
		imappPlatformWindowDestroy( window );
		return NULL;
	}

	window->dpiScale = GetDpiForWindow( window->hwnd ) / (float)USER_DEFAULT_SCREEN_DPI;
	SetWindowPos( window->hwnd, HWND_TOP, 0, 0, (int)ceil( parameters->width * window->dpiScale ), (int)ceil( parameters->height * window->dpiScale ), SWP_NOMOVE );

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
	switch( parameters->state )
	{
	case ImappWindowState_Default: break;
	case ImappWindowState_Maximized: showState = SW_SHOWMAXIMIZED; break;
	case ImappWindowState_Minimized: showState = SW_SHOWMINIMIZED; break;
	}
	ShowWindow( window->hwnd, showState );

	{
		RECT clientRect;
		GetClientRect( window->hwnd, &clientRect );

		window->hdc			= GetDC( window->hwnd );
		window->width		= (clientRect.right - clientRect.left);
		window->height		= (clientRect.bottom - clientRect.top);
	}

	imappEventQueueConstruct( &window->eventQueue, platform->allocator );

	window->dropTarget.lpVtbl = &s_dropTargetVtbl;
	const HRESULT dropRegisterResult = RegisterDragDrop( window->hwnd, &window->dropTarget );
	if( FAILED( dropRegisterResult ) )
	{
		IMAPP_DEBUG_LOGW( "Failed to register drop target. Result: 0x%08x", dropRegisterResult );
	}

	// create GL context
	{
		window->hglrc = imappPlatformWindowCreateGlContext( window->hdc );
		if( !window->hglrc )
		{
			IMAPP_DEBUG_LOGE( "Failed to create GL context." );
			return false;
		}

		if( !wglShareLists( window->platform->contextGlrc, window->hglrc ) )
		{
			IMAPP_DEBUG_LOGE( "Failed to share GL context." );
			return false;
		}
	}

	return window;
}

void imappPlatformWindowDestroy( ImappWindow* window )
{
	while( window->firstNewDrop )
	{
		ImappWindowDrop* drop = window->firstNewDrop;
		window->firstNewDrop = drop->nextDrop;

		imuiMemoryFree( window->platform->allocator, drop );
	}

	while( window->firstPoppedDrop )
	{
		ImappWindowDrop* drop = window->firstPoppedDrop;
		window->firstPoppedDrop = drop->nextDrop;

		imuiMemoryFree( window->platform->allocator, drop );
	}

	imappEventQueueDestruct( &window->eventQueue );

	if( window->hglrc )
	{
		wglMakeCurrent( window->platform->contextDc, window->platform->contextGlrc );

		wglDeleteContext( window->hglrc );
		window->hglrc = NULL;
	}

	if( window->hwnd )
	{
		RevokeDragDrop( window->hwnd );

		DestroyWindow( window->hwnd );
		window->hwnd = NULL;
	}

	if( window->title )
	{
		imuiMemoryFree( window->platform->allocator, window->title );
		window->title = NULL;
	}

	imuiMemoryFree( window->platform->allocator, window );
}

static HGLRC imappPlatformWindowCreateGlContext( HDC windowDc )
{
	PIXELFORMATDESCRIPTOR pixelFormat;
	ZeroMemory( &pixelFormat, sizeof( pixelFormat ) );
	pixelFormat.nSize		= sizeof( pixelFormat );
	pixelFormat.nVersion	= 1;
	pixelFormat.dwFlags		= PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pixelFormat.iPixelType	= PFD_TYPE_RGBA;
	pixelFormat.cColorBits	= 24;
	pixelFormat.cAlphaBits	= 8;
	pixelFormat.iLayerType	= PFD_MAIN_PLANE;

	const int format = ChoosePixelFormat( windowDc, &pixelFormat );
	if( !SetPixelFormat( windowDc, format, &pixelFormat ) )
	{
		IMAPP_DEBUG_LOGE( "Failed to set pixel format." );
		return NULL;
	}

	return wglCreateContext( windowDc );
}

ImappWindowDeviceState imappPlatformWindowGetGlContextState( const ImappWindow* window )
{
	IMAPP_USE( window );
	return ImappWindowDeviceState_Ok;
}

void imappPlatformWindowUpdate( ImappWindow* window, imappPlatformWindowUpdateCallback callback, void* arg )
{
	window->updateCallback		= callback;
	window->updateCallbackArg	= arg;

	while( window->firstPoppedDrop )
	{
		ImappWindowDrop* drop = window->firstPoppedDrop;
		window->firstPoppedDrop = drop->nextDrop;

		imuiMemoryFree( window->platform->allocator, drop );
	}

	imappPlatformWindowUpdateController( window );

	MSG message;
	while( PeekMessage( &message, window->hwnd, 0u, 0u, PM_REMOVE ) )
	{
		TranslateMessage( &message );
		DispatchMessage( &message );
	}

	// prevent recursive calls
	window->updateCallback		= NULL;
	window->updateCallbackArg	= NULL;
}

bool imappPlatformWindowBeginRender( ImappWindow* window )
{
	if( !wglMakeCurrent( window->hdc, window->hglrc ) )
	{
		IMAPP_DEBUG_LOGE( "Failed to activate GL context." );
		return false;
	}

	return true;
}

bool imappPlatformWindowEndRender( ImappWindow* window )
{
	if( !wglSwapLayerBuffers( window->hdc, WGL_SWAP_MAIN_PLANE ) )
	{
		IMAPP_DEBUG_LOGE( "Failed to present." );
		return false;
	}

	return true;
}

ImappEventQueue* imappPlatformWindowGetEventQueue( ImappWindow* window )
{
	return &window->eventQueue;
}

bool imappPlatformWindowPopDropData( ImappWindow* window, ImappDropData* outData )
{
	if( !window->firstNewDrop )
	{
		return false;
	}

	ImappWindowDrop* drop = window->firstNewDrop;
	outData->type		= drop->type;
	outData->pathOrText	= drop->pathOrText;

	window->firstNewDrop = drop->nextDrop;

	drop->nextDrop = window->firstPoppedDrop;
	window->firstPoppedDrop = drop;

	return true;
}

void imappPlatformWindowGetViewRect( const ImappWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	*outX = 0;
	*outY = 0;

	imappPlatformWindowGetSize( window, outWidth, outHeight );
}

bool imappPlatformWindowHasFocus( const ImappWindow* window )
{
	return window->hasFocus;
}

void imappPlatformWindowGetSize( const ImappWindow* window, int* outWidth, int* outHeight )
{
	*outWidth	= window->width;
	*outHeight	= window->height;
}

void imappPlatformWindowSetSize( ImappWindow* window, int width, int height )
{
	SetWindowPos( window->hwnd, HWND_TOP, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER );
}

void imappPlatformWindowGetPosition( const ImappWindow* window, int* outX, int* outY )
{
	*outX = window->x;
	*outY = window->y;
}

void imappPlatformWindowSetPosition( const ImappWindow* window, int x, int y )
{
	SetWindowPos( window->hwnd, HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER );
}

ImappWindowStyle imappPlatformWindowGetStyle( const ImappWindow* window )
{
	return window->style;
}

ImappWindowState imappPlatformWindowGetState( const ImappWindow* window )
{
	return window->state;
}

void imappPlatformWindowSetState( ImappWindow* window, ImappWindowState state )
{
	if( window->state == state )
	{
		return;
	}

	int cmdShow = -1;
	switch( state )
	{
	case ImappWindowState_Default:		cmdShow = SW_RESTORE; break;
	case ImappWindowState_Maximized:	cmdShow = SW_MAXIMIZE; break;
	case ImappWindowState_Minimized:	cmdShow = SW_MINIMIZE; break;
	}

	if( cmdShow < 0 )
	{
		return;
	}

	ShowWindow( window->hwnd, cmdShow );
	window->state = state;
}

const char* imappPlatformWindowGetTitle( const ImappWindow* window )
{
	return window->title;
}

void imappPlatformWindowSetTitle( ImappWindow* window, const char* title )
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

void imappPlatformWindowSetTitleBounds( ImappWindow* window, int height, int buttonsX )
{
	window->titleHeight		= height;
	window->titleButtonsX	= buttonsX;
}

float imappPlatformWindowGetDpiScale( const ImappWindow* window )
{
	return window->dpiScale;
}

void imappPlatformWindowClose( ImappWindow* window )
{
	SendMessage( window->hwnd, WM_CLOSE, 0, 0 );
}

static const WORD s_controllerButtonsXInput[] =
{
	XINPUT_GAMEPAD_DPAD_UP,
	XINPUT_GAMEPAD_DPAD_DOWN,
	XINPUT_GAMEPAD_DPAD_LEFT,
	XINPUT_GAMEPAD_DPAD_RIGHT,
	XINPUT_GAMEPAD_START,
	XINPUT_GAMEPAD_BACK,
	XINPUT_GAMEPAD_LEFT_THUMB,
	XINPUT_GAMEPAD_RIGHT_THUMB,
	XINPUT_GAMEPAD_LEFT_SHOULDER,
	XINPUT_GAMEPAD_RIGHT_SHOULDER,
	XINPUT_GAMEPAD_A,
	XINPUT_GAMEPAD_B,
	XINPUT_GAMEPAD_X,
	XINPUT_GAMEPAD_Y
};

static const ImuiInputKey s_controllerButtonsimui[] =
{
	ImuiInputKey_Gamepad_Dpad_Up,
	ImuiInputKey_Gamepad_Dpad_Down,
	ImuiInputKey_Gamepad_Dpad_Left,
	ImuiInputKey_Gamepad_Dpad_Right,
	ImuiInputKey_Gamepad_Start,
	ImuiInputKey_Gamepad_Back,
	ImuiInputKey_Gamepad_LeftThumb,
	ImuiInputKey_Gamepad_RightThumb,
	ImuiInputKey_Gamepad_LeftShoulder,
	ImuiInputKey_Gamepad_RightShoulder,
	ImuiInputKey_Gamepad_A,
	ImuiInputKey_Gamepad_B,
	ImuiInputKey_Gamepad_X,
	ImuiInputKey_Gamepad_Y
};
static_assert( IMAPP_ARRAY_COUNT( s_controllerButtonsXInput ) == IMAPP_ARRAY_COUNT( s_controllerButtonsimui ), "more controller buttons" );

static void imappPlatformWindowUpdateController( ImappWindow* window )
{
	XINPUT_STATE state;
	if( XInputGetState( 0u, &state ) != ERROR_SUCCESS )
	{
		return;
	}
	else if( state.dwPacketNumber == window->lastControllerState.dwPacketNumber )
	{
		return;
	}

	if( state.Gamepad.sThumbLX != window->lastControllerState.Gamepad.sThumbLX ||
		state.Gamepad.sThumbLY != window->lastControllerState.Gamepad.sThumbLY )
	{
		ImappEvent directionEvent = { .type = ImappEventType_Direction };
		if( state.Gamepad.sThumbLX < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
		{
			directionEvent.direction.x = (float)(state.Gamepad.sThumbLX + XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32768.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		}
		else if( state.Gamepad.sThumbLX > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
		{
			directionEvent.direction.x = (float)(state.Gamepad.sThumbLX - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (32767.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		}

		if( state.Gamepad.sThumbLY < -XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
		{
			directionEvent.direction.y = (float)(state.Gamepad.sThumbLY + XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (-32768.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		}
		else if( state.Gamepad.sThumbLY > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE )
		{
			directionEvent.direction.y = (float)(state.Gamepad.sThumbLY - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) / (-32767.0f - XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
		}

		imappEventQueuePush( &window->eventQueue, &directionEvent );
	}

	for( uintsize i = 0; i < IMAPP_ARRAY_COUNT( s_controllerButtonsXInput ); ++i )
	{
		const bool isDown = (state.Gamepad.wButtons & s_controllerButtonsXInput[ i ]) != 0;
		const bool wasDown = (window->lastControllerState.Gamepad.wButtons & s_controllerButtonsXInput[ i ]) != 0;

		if( isDown == wasDown )
		{
			continue;
		}

		ImappEvent keyEvent;
		keyEvent.key.type	= isDown ? ImappEventType_KeyDown : ImappEventType_KeyUp;
		keyEvent.key.key	= s_controllerButtonsimui[ i ];

		imappEventQueuePush( &window->eventQueue, &keyEvent );
	}

	window->lastControllerState = state;
}

static LRESULT CALLBACK imappPlatformWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	if( message == WM_NCCREATE )
	{
		CREATESTRUCTW* createStruct = (CREATESTRUCTW*)lParam;

		SetWindowLongPtrW( hWnd, GWLP_USERDATA, (LONG_PTR)createStruct->lpCreateParams );

		return TRUE; // MUST return TRUE or creation fails
	}
	else if( message == WM_DESTROY )
	{
		PostQuitMessage( 0 );
		return 0;
	}

	ImappWindow* window = (ImappWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	if( window == NULL )
	{
		return DefWindowProc( hWnd, message, wParam, lParam );
	}

	switch( message )
	{
	case WM_CREATE:
		if( window->style == ImappWindowStyle_Custom )
		{
			// extend the frame over the entire window
			const MARGINS margin = { -1, -1, -1, -1 };
			DwmExtendFrameIntoClientArea( hWnd, &margin );

			SetWindowPos( hWnd, NULL, 0, 0, 0, 0, SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER );
		}
		break;

	case WM_CLOSE:
		{
			const ImappEvent closeEvent = { .window = { .type = ImappEventType_WindowClose } };
			imappEventQueuePush( &window->eventQueue, &closeEvent );
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

		if( window->updateCallback )
		{
			window->updateCallback( window, window->updateCallbackArg );
		}
		return 0;

	case WM_SIZE:
		{
			switch( wParam )
			{
			case SIZE_RESTORED:
				window->state = ImappWindowState_Default;
				break;

			case SIZE_MAXIMIZED:
				window->state = ImappWindowState_Maximized;
				break;

			case SIZE_MINIMIZED:
				window->state = ImappWindowState_Minimized;
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
		if( window->isResize &&
			window->updateCallback )
		{
			window->updateCallback( window, window->updateCallbackArg );
		}
		break;

	case WM_NCACTIVATE:
		if( window->style == ImappWindowStyle_Custom )
		{
			return TRUE;
		}
		break;

	//case WM_NCPAINT:
	//	if( window->style == imappWindowStyle_Custom )
	//	{
	//		return 0;
	//	}
	//	break;

	case WM_ERASEBKGND:
		if( window->style == ImappWindowStyle_Custom )
		{
			return 1;
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
			const ImappEvent motionEvent = { .motion = {.type = ImappEventType_Motion, .x = x, .y = y } };
			imappEventQueuePush( &window->eventQueue, &motionEvent );
		}
		return 0;

	case WM_MOUSELEAVE:
		{
			const ImappEvent motionEvent = { .motion = {.type = ImappEventType_Motion, .x = 0xffffffff, .y = 0xffffffff } };
			imappEventQueuePush( &window->eventQueue, &motionEvent );

			window->hasTracking = false;
		}
		return 0;

	case WM_CHAR:
		{
			if( wParam < L' ' )
			{
				break;
			}

			const ImappEvent characterEvent = { .character = {.type = ImappEventType_Character, .character = (uint32_t)wParam } };
			imappEventQueuePush( &window->eventQueue, &characterEvent );
		}
		return 0;

	case WM_KEYDOWN:
		{
			const ImuiInputKey key = imappPlatformWindowMapKey( window, wParam, lParam );

			const bool repeat = lParam & 0x40000000;
			const ImappEvent keyEvent = { .key = { .type = ImappEventType_KeyDown, .key = key, .repeat = repeat } };
			imappEventQueuePush( &window->eventQueue, &keyEvent );
		}
		return 0;

	case WM_KEYUP:
		{
			const ImuiInputKey key = imappPlatformWindowMapKey( window, wParam, lParam );

			const ImappEvent keyEvent = { .key = { .type = ImappEventType_KeyUp, .key = key } };
			imappEventQueuePush( &window->eventQueue, &keyEvent );
		}
		return 0;

	case WM_LBUTTONDBLCLK:
		{
			const ImappEvent buttonEvent = { .button = {.type = ImappEventType_DoubleClick, .button = ImuiInputMouseButton_Left } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );
		}
		// fall through

	case WM_LBUTTONDOWN:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonDown, .button = ImuiInputMouseButton_Left } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			SetCapture( window->hwnd );
		}
		return 0;

	case WM_LBUTTONUP:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonUp, .button = ImuiInputMouseButton_Left } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			ReleaseCapture();
		}
		return 0;

	case WM_RBUTTONDBLCLK:
		{
			const ImappEvent buttonEvent = { .button = {.type = ImappEventType_DoubleClick, .button = ImuiInputMouseButton_Right } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );
		}
		// fall through

	case WM_RBUTTONDOWN:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonDown, .button = ImuiInputMouseButton_Right } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			SetCapture( window->hwnd );
		}
		return 0;

	case WM_RBUTTONUP:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonUp, .button = ImuiInputMouseButton_Right } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			ReleaseCapture();
		}
		return 0;

	case WM_MBUTTONDBLCLK:
		{
			const ImappEvent buttonEvent = { .button = {.type = ImappEventType_DoubleClick, .button = ImuiInputMouseButton_Middle } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );
		}
		// fall through

	case WM_MBUTTONDOWN:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonDown, .button = ImuiInputMouseButton_Middle } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			SetCapture( window->hwnd );
		}
		return 0;

	case WM_MBUTTONUP:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonUp, .button = ImuiInputMouseButton_Middle } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			ReleaseCapture();
		}
		return 0;

	case WM_XBUTTONDBLCLK:
		{
			const ImappEvent buttonEvent = { .button = {.type = ImappEventType_DoubleClick, .button = ImuiInputMouseButton_X1 } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );
		}
		// fall through

	case WM_XBUTTONDOWN:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonDown, .button = ImuiInputMouseButton_X1 } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			SetCapture( window->hwnd );
		}
		return 0;

	case WM_XBUTTONUP:
		{
			const ImappEvent buttonEvent = { .button = { .type = ImappEventType_ButtonUp, .button = ImuiInputMouseButton_X1 } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			ReleaseCapture();
		}
		return 0;

	case WM_MOUSEWHEEL:
		{
			const int yDelta = GET_WHEEL_DELTA_WPARAM( wParam ) / WHEEL_DELTA;
			const ImappEvent scrollEvent = { .scroll = { .type = ImappEventType_Scroll, .x = 0, .y = yDelta } };
			imappEventQueuePush( &window->eventQueue, &scrollEvent );
		}
		return 0;

	case WM_MOUSEHWHEEL:
		{
			const int xDelta = GET_WHEEL_DELTA_WPARAM( wParam ) / WHEEL_DELTA;
			const ImappEvent scrollEvent = { .scroll = { .type = ImappEventType_Scroll, .x = xDelta, .y = 0 } };
			imappEventQueuePush( &window->eventQueue, &scrollEvent );
		}
		return 0;

	case WM_NCCALCSIZE:
		if( window->style == ImappWindowStyle_Custom )
		{
			if( !wParam )
			{
				break;
			}

			return 0;
		}
		break;

	case WM_NCHITTEST:
		if( window->style == ImappWindowStyle_Custom )
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

			const UINT dpi		= GetDpiForWindow( window->hwnd );
			int frame_x			= GetSystemMetricsForDpi( SM_CXFRAME, dpi );
			int frame_y			= GetSystemMetricsForDpi( SM_CYFRAME, dpi );
			const int padding	= GetSystemMetricsForDpi( SM_CXPADDEDBORDER, dpi );

			frame_x += padding;
			frame_y += padding;

			POINT mousePos = { 0 };
			mousePos.x = GET_X_LPARAM( lParam );
			mousePos.y = GET_Y_LPARAM( lParam );
			ScreenToClient( window->hwnd, &mousePos );

			RECT windowRect;
			GetClientRect( hWnd, &windowRect );
			const int windowWidth	= windowRect.right - windowRect.left;
			const int windowHeight	= windowRect.bottom - windowRect.top;

			if( mousePos.x < frame_x &&
				mousePos.y < frame_y )
			{
				window->windowHitResult = HTTOPLEFT;
			}
			else if( mousePos.x >= windowWidth - frame_x &&
				mousePos.y < frame_y )
			{
				window->windowHitResult = HTTOPRIGHT;
			}
			else if( mousePos.y < frame_y )
			{
				window->windowHitResult = HTTOP;
			}
			else if( mousePos.x < frame_x &&
				mousePos.y >= windowHeight - frame_y )
			{
				window->windowHitResult = HTBOTTOMLEFT;
			}
			else if( mousePos.x >= windowWidth - frame_x &&
				mousePos.y >= windowHeight - frame_y )
			{
				window->windowHitResult = HTBOTTOMRIGHT;
			}
			else if( mousePos.x < frame_x )
			{
				window->windowHitResult = HTLEFT;
			}
			else if( mousePos.x >= windowWidth - frame_x )
			{
				window->windowHitResult = HTRIGHT;
			}
			else if( mousePos.y >= windowHeight - frame_y )
			{
				window->windowHitResult = HTBOTTOM;
			}
			else if( mousePos.y < window->titleHeight &&
				mousePos.x < window->titleButtonsX )
			{
				window->windowHitResult = HTCAPTION;
			}
			else
			{
				window->windowHitResult = HTCLIENT;
			}

			return window->windowHitResult;
		}
		break;

	case WM_NCMOUSEMOVE:
		if( window->style == ImappWindowStyle_Custom )
		{
			POINT mousePos;
			mousePos.x = GET_X_LPARAM( lParam );
			mousePos.y = GET_Y_LPARAM( lParam );
			ScreenToClient( window->hwnd, &mousePos );

			const ImappEvent motionEvent = { .motion = {.type = ImappEventType_Motion, .x = mousePos.x, .y = mousePos.y } };
			imappEventQueuePush( &window->eventQueue, &motionEvent );

			return 0;
		}
		break;

	case WM_NCLBUTTONDOWN:
		if( window->style == ImappWindowStyle_Custom &&
			window->windowHitResult == HTCLIENT )
		{
			const ImappEvent buttonEvent = { .button = {.type = ImappEventType_ButtonDown, .button = ImuiInputMouseButton_Left } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			return 0;
		}
		break;

	case WM_NCLBUTTONUP:
		if( window->style == ImappWindowStyle_Custom &&
			window->windowHitResult == HTCLIENT )
		{
			const ImappEvent buttonEvent = { .button = {.type = ImappEventType_ButtonUp, .button = ImuiInputMouseButton_Left } };
			imappEventQueuePush( &window->eventQueue, &buttonEvent );

			return 0;
		}
		break;

	case WM_NCRBUTTONUP:
		if( window->style == ImappWindowStyle_Custom &&
			wParam == HTCAPTION )
		{
			const BOOL isMaximized = window->state == ImappWindowState_Maximized;
			MENUITEMINFO menuItemInfo = {
				.cbSize = sizeof( menuItemInfo ),
				.fMask = MIIM_STATE
			};

			const HMENU sysMenu = GetSystemMenu( window->hwnd, false );
			imappPlatformWindowSetMenuItemState( sysMenu, &menuItemInfo, SC_RESTORE, isMaximized );
			imappPlatformWindowSetMenuItemState( sysMenu, &menuItemInfo, SC_MOVE, !isMaximized );
			imappPlatformWindowSetMenuItemState( sysMenu, &menuItemInfo, SC_SIZE, !isMaximized );
			imappPlatformWindowSetMenuItemState( sysMenu, &menuItemInfo, SC_MINIMIZE, true );
			imappPlatformWindowSetMenuItemState( sysMenu, &menuItemInfo, SC_MAXIMIZE, !isMaximized );
			imappPlatformWindowSetMenuItemState( sysMenu, &menuItemInfo, SC_CLOSE, true );

			const BOOL result = TrackPopupMenu( sysMenu, TPM_RETURNCMD, GET_X_LPARAM( lParam ), GET_Y_LPARAM( lParam ), 0, window->hwnd, NULL );
			if( result != 0 )
			{
				PostMessage( window->hwnd, WM_SYSCOMMAND, result, 0 );
			}
		}
		break;

	case WM_GETMINMAXINFO:
		if( window->style == ImappWindowStyle_Custom )
		{
			MINMAXINFO* mmi = (MINMAXINFO*)lParam;

			HMONITOR mon = MonitorFromWindow( window->hwnd, MONITOR_DEFAULTTONEAREST );
			MONITORINFO mi = { 0 };
			mi.cbSize = sizeof( mi );
			GetMonitorInfo( mon, &mi );

			// Work area excludes taskbar; monitor area includes it.
			RECT rcWork = mi.rcWork;
			RECT rcMon  = mi.rcMonitor;

			mmi->ptMaxPosition.x = rcWork.left - rcMon.left;
			mmi->ptMaxPosition.y = rcWork.top - rcMon.top;
			mmi->ptMaxSize.x = rcWork.right - rcWork.left;
			mmi->ptMaxSize.y = rcWork.bottom - rcWork.top;

			return 0;
		}
		break;

	default:
		break;
	}

	return DefWindowProc( hWnd, message, wParam, lParam );
}

static ImuiInputKey imappPlatformWindowMapKey( ImappWindow* window, WPARAM wParam, LPARAM lParam )
{
	if( wParam > IMAPP_ARRAY_COUNT( window->platform->inputKeyMapping ) )
	{
		return ImuiInputKey_None;
	}

	ImuiInputKey key = (ImuiInputKey)window->platform->inputKeyMapping[ (uint8)wParam ];
	if( wParam == VK_RETURN &&
		lParam & 0x1000000 )
	{
		key = ImuiInputKey_Numpad_Enter;
	}
	else if( wParam == VK_SHIFT )
	{
		key = lParam & 0x1000000 ? ImuiInputKey_RightShift : ImuiInputKey_LeftShift;
	}
	else if( wParam == VK_CONTROL )
	{
		key = lParam & 0x1000000 ? ImuiInputKey_RightControl : ImuiInputKey_LeftControl;
	}
	else if( wParam == VK_MENU )
	{
		key = lParam & 0x1000000 ? ImuiInputKey_RightAlt : ImuiInputKey_LeftAlt;
	}

	return key;
}

HRESULT STDMETHODCALLTYPE imappPlatformWindowDropTargetQueryInterface( __RPC__in IDropTarget* This, __RPC__in REFIID riid, _COM_Outptr_ void **ppvObject )
{
	if( riid == &IID_IDropTarget )
	{
		*ppvObject = This;
		return S_OK;
	}

	return S_FALSE;
}

ULONG STDMETHODCALLTYPE imappPlatformWindowDropTargetAddRef( __RPC__in IDropTarget* This )
{
	IMAPP_USE( This );

	return 1u;
}

ULONG STDMETHODCALLTYPE imappPlatformWindowDropTargetRelease( __RPC__in IDropTarget* This )
{
	IMAPP_USE( This );

	return 1u;
}

HRESULT STDMETHODCALLTYPE imappPlatformWindowDropTargetDragEnter( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect )
{
	IMAPP_USE( grfKeyState );
	IMAPP_USE( pt );
	IMAPP_USE( pdwEffect );

	ImappWindow* window = (ImappWindow*)This;

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

HRESULT STDMETHODCALLTYPE imappPlatformWindowDropTargetDragOver( __RPC__in IDropTarget* This, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect )
{
	IMAPP_USE( This );
	IMAPP_USE( grfKeyState );
	IMAPP_USE( pt );
	IMAPP_USE( pdwEffect );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE imappPlatformWindowDropTargetDragLeave( __RPC__in IDropTarget* This )
{
	IMAPP_USE( This );

	return S_OK;
}

HRESULT STDMETHODCALLTYPE imappPlatformWindowDropTargetDrop( __RPC__in IDropTarget* This, __RPC__in_opt IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, __RPC__inout DWORD* pdwEffect )
{
	IMAPP_USE( grfKeyState );
	IMAPP_USE( pt );
	IMAPP_USE( pdwEffect );

	ImappWindow* window = (ImappWindow*)This;

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

		ImappWindowDrop* drop = (ImappWindowDrop*)imuiMemoryAlloc( window->platform->allocator, sizeof( ImappWindowDrop ) + dataSize );
		drop->type = ImappDropType_Text;

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
			ImappWindowDrop* drop = (ImappWindowDrop*)imuiMemoryAlloc( window->platform->allocator, sizeof( ImappWindowDrop ) + utfNameLength );
			drop->type = ImappDropType_File;

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

void imappPlatformResourceGetPath( ImappPlatform* platform, char* outPath, uintsize pathCapacity, const char* resourceName )
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

ImappBlob imappPlatformResourceLoad( ImappPlatform* platform, const char* resourceName )
{
	return imappPlatformResourceLoadRange( platform, resourceName, 0u, (uintsize)-1 );
}

ImappBlob imappPlatformResourceLoadRange( ImappPlatform* platform, const char* resourceName, uintsize offset, uintsize length )
{
	ImappBlob result = { NULL, 0u };

	ImappFile* file = imappPlatformResourceOpen( platform, resourceName );
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

	void* memory = imuiMemoryAlloc( platform->allocator, length );
	if( !memory )
	{
		imappPlatformResourceClose( platform, file );
		return result;
	}

	const uintsize readResult = imappPlatformResourceRead( file, memory, length, offset );
	imappPlatformResourceClose( platform, file );

	if( readResult != length )
	{
		imuiMemoryFree( platform->allocator, memory );
		return result;
	}

	result.data	= memory;
	result.size	= length;
	return result;
}

ImappFile* imappPlatformResourceOpen( ImappPlatform* platform, const char* resourceName )
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

	return (ImappFile*)fileHandle;
}
uintsize imappPlatformResourceRead( ImappFile* file, void* outData, uintsize length, uintsize offset )
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

void imappPlatformResourceClose( ImappPlatform* platform, ImappFile* file )
{
	IMAPP_USE( platform );

	const HANDLE fileHandle = (HANDLE)file;
	CloseHandle( fileHandle );
}

ImappBlob imappPlatformResourceLoadSystemFont( ImappPlatform* platform, const char* fontName )
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
		const ImappBlob result = { NULL, 0u };
		return result;
	}

	LARGE_INTEGER fileSize;
	GetFileSizeEx( fileHandle, &fileSize );

	void* memory = imuiMemoryAlloc( platform->allocator, (uintsize)fileSize.QuadPart );

	DWORD bytesRead = 0u;
	const BOOL readResult = ReadFile( fileHandle, memory, (DWORD)fileSize.QuadPart, &bytesRead, NULL );
	CloseHandle( fileHandle );

	if( !readResult || bytesRead != (DWORD)fileSize.QuadPart )
	{
		imuiMemoryFree( platform->allocator, memory );
		const ImappBlob result = { NULL, 0u };
		return result;
	}

	const ImappBlob result = { memory, (uintsize)fileSize.QuadPart };
	return result;
}

void imappPlatformResourceFree( ImappPlatform* platform, ImappBlob blob )
{
	imuiMemoryFree( platform->allocator, blob.data );
}

ImappFileWatcher* imappPlatformFileWatcherCreate( ImappPlatform* platform )
{
	ImappFileWatcher* watcher = IMUI_MEMORY_NEW( platform->allocator, ImappFileWatcher );
	if( !watcher )
	{
		return NULL;
	}

	watcher->platform	= platform;
	watcher->firstPath	= NULL;
	watcher->ioPort		= CreateIoCompletionPort( INVALID_HANDLE_VALUE, NULL, 0, 1u );

	if( watcher->ioPort == NULL )
	{
		imappPlatformFileWatcherDestroy( platform, watcher );
		return NULL;
	}

	return watcher;
}

void imappPlatformFileWatcherDestroy( ImappPlatform* platform, ImappFileWatcher* watcher )
{
	ImappFileWatcherPath* path = watcher->firstPath;
	ImappFileWatcherPath* nextPath = NULL;
	while( path )
	{
		nextPath = path->nextPath;
		CancelIo( path->dirHandle );
		CloseHandle( path->dirHandle );
		imuiMemoryFree( platform->allocator, path );
		path = nextPath;
	}

	if( watcher->ioPort != NULL )
	{
		CloseHandle( watcher->ioPort );
	}

	imuiMemoryFree( platform->allocator, watcher );
}

void imappPlatformFileWatcherAddPath( ImappFileWatcher* watcher, const char* path )
{
	const char* pathEnd = strrchr( path, '/' );
	const uintsize pathLength = pathEnd ? pathEnd - path : strlen( path );

	for( ImappFileWatcherPath* watcherPath = watcher->firstPath; watcherPath; watcherPath = watcherPath->nextPath )
	{
		if( strncmp( path, watcherPath->path, pathLength ) == 0 )
		{
			// already exists
			return;
		}
	}

	ImappFileWatcherPath* watcherPath = (ImappFileWatcherPath*)imuiMemoryAllocZero( watcher->platform->allocator, sizeof( ImappFileWatcherPath ) + pathLength );
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
		imuiMemoryFree( watcher->platform->allocator, watcherPath );
		return;
	}

	const HANDLE ioPort = CreateIoCompletionPort( watcherPath->dirHandle, watcher->ioPort, 0u, 0u );
	if( ioPort == NULL )
	{
		IMAPP_DEBUG_LOGE( "Failed to attach '%s' to I/O completion port. Error: 0x%08x", GetLastError() );
		CloseHandle( watcherPath->dirHandle );
		imuiMemoryFree( watcher->platform->allocator, watcherPath );
		return;
	}

	imappPlatformFileWatcherPathStart( watcherPath );

	watcherPath->nextPath = watcher->firstPath;
	if( watcherPath->nextPath )
	{
		watcherPath->nextPath->prevPath = watcherPath;
	}
	watcher->firstPath = watcherPath;
}

void imappPlatformFileWatcherRemovePath( ImappFileWatcher* watcher, const char* path )
{
	const char* pathEnd = strrchr( path, '/' );
	const uintsize pathLength = pathEnd ? pathEnd - path : strlen( path );

	ImappFileWatcherPath* watcherPath = NULL;
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

	imuiMemoryFree( watcher->platform->allocator, watcherPath );
}

bool imappPlatformFileWatcherPopEvent( ImappFileWatcher* watcher, ImappFileWatchEvent* outEvent )
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

	ImappFileWatcherPath* path = (ImappFileWatcherPath*)overlapped;
	const FILE_NOTIFY_INFORMATION* fileInfo = (const FILE_NOTIFY_INFORMATION*)path->buffer;

	const int pathLength = WideCharToMultiByte( CP_UTF8, 0, fileInfo->FileName, (int)fileInfo->FileNameLength, watcher->eventPath, IMAPP_ARRAY_COUNT( watcher->eventPath ), NULL, NULL );
	watcher->eventPath[ pathLength ] = '\0';

	outEvent->path = watcher->eventPath;

	imappPlatformFileWatcherPathStart( path );

	return true;
}

static void imappPlatformFileWatcherPathStart( ImappFileWatcherPath* path )
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

ImappThread* imappPlatformThreadCreate( ImappPlatform* platform, const char* name, ImappThreadFunc func, void* arg )
{
	ImappThread* thread = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImappThread );
	if( !thread )
	{
		return NULL;
	}

	InterlockedExchangeNoFence( &thread->isRunning, 1 );

	thread->platform	= platform;
	thread->func		= func;
	thread->arg			= arg;
	thread->handle		= CreateThread( NULL, 0u, imappPlatformThreadEntry, thread, 0u, NULL );

	IMAPP_USE( name );
	//SetThreadDescription( thread->handle, name );

	return thread;
}

void imappPlatformThreadDestroy( ImappThread* thread )
{
	WaitForSingleObject( thread->handle, INFINITE );
	CloseHandle( thread->handle );

	imuiMemoryFree( thread->platform->allocator, thread );
}

bool imappPlatformThreadIsRunning( const ImappThread* thread )
{
	return InterlockedOrNoFence( (volatile LONG*)&thread->isRunning, 0);
}

static DWORD WINAPI imappPlatformThreadEntry( void* voidThread )
{
	ImappThread* thread = (ImappThread*)voidThread;

	thread->func( thread->arg );

	InterlockedExchangeNoFence( &thread->isRunning, 0 );

	return 0u;
}

ImappMutex* imappPlatformMutexCreate( ImappPlatform* platform )
{
	CRITICAL_SECTION* cs = IMUI_MEMORY_NEW_ZERO( platform->allocator, CRITICAL_SECTION );

	InitializeCriticalSection( cs );

	return (ImappMutex*)cs;
}

void imappPlatformMutexDestroy( ImappPlatform* platform, ImappMutex* mutex )
{
	imuiMemoryFree( platform->allocator, mutex );
}

void imappPlatformMutexLock( ImappMutex* mutex )
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)mutex;
	EnterCriticalSection( cs );
}

void imappPlatformMutexUnlock( ImappMutex* mutex )
{
	CRITICAL_SECTION* cs = (CRITICAL_SECTION*)mutex;
	LeaveCriticalSection( cs );
}

ImappSemaphore* imappPlatformSemaphoreCreate( ImappPlatform* platform )
{
	IMAPP_USE( platform );

	return (ImappSemaphore*)CreateSemaphore( NULL, 0, LONG_MAX, NULL );
}

void imappPlatformSemaphoreDestroy( ImappPlatform* platform, ImappSemaphore* semaphore )
{
	IMAPP_USE( platform );

	CloseHandle( (HANDLE)semaphore );
}

void imappPlatformSemaphoreInc( ImappSemaphore* semaphore )
{
	ReleaseSemaphore( (HANDLE)semaphore, 1, NULL );
}

bool imappPlatformSemaphoreDec( ImappSemaphore* semaphore, bool wait )
{
	return WaitForSingleObject( (HANDLE)semaphore, wait ? INFINITE : 0u ) == WAIT_OBJECT_0;
}

#endif
