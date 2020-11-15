#include "../imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_WINDOWS ) && IMAPP_DISABLED( IMAPP_PLATFORM_SDL )

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

//////////////////////////////////////////////////////////////////////////
// Main

int WinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd )
{
	int argc;
	char** pArgs = NULL;
	{
		const wchar_t* pWideCommandLine = GetCommandLineW();
		wchar_t** ppWideArgs = CommandLineToArgvW( pWideCommandLine, &argc );

		pArgs = (char**)ImAppMallocZero( sizeof( char* ) * argc );

		for( size_t i = 0u; i < argc; ++i )
		{
			const int argLength = WideCharToMultiByte( CP_UTF8, 0u, ppWideArgs[ i ], -1, NULL, 0, NULL, NULL );

			pArgs[ i ] = ImAppMallocZero( argLength + 1 );
			WideCharToMultiByte( CP_UTF8, 0u, ppWideArgs[ i ], -1, pArgs[ i ], argLength, NULL, NULL );
		}
		LocalFree( ppWideArgs );
	}

	const int result = ImAppMain( argc, pArgs );

	for( size_t i = 0u; i < argc; ++i )
	{
		ImAppFree( pArgs[ i ] );
	}
	ImAppFree( pArgs );

	return result;
}

//////////////////////////////////////////////////////////////////////////
// Shared Libraries

ImAppSharedLibHandle ImAppSharedLibOpen( const char* pSharedLibName )
{
	return (ImAppSharedLibHandle)LoadLibraryA( pSharedLibName );
}

void ImAppSharedLibClose( ImAppSharedLibHandle libHandle )
{
	FreeLibrary( (HMODULE)libHandle );
}

void* ImAppSharedLibGetFunction( ImAppSharedLibHandle libHandle, const char* pFunctionName )
{
	return GetProcAddress( (HMODULE)libHandle, pFunctionName );
}

//////////////////////////////////////////////////////////////////////////
// Window

static const wchar_t* s_pWindowClass = L"ImAppWindowClass";

struct ImAppWindow
{
	HWND				windowHandle;

	wchar_t				windowTitle[ 256u ];

	bool				isOpen;
	int					x;
	int					y;
	int					width;
	int					height;
	ImAppWindowState	state;

	ImAppInputPlatform*	pInput;
};

static LRESULT CALLBACK		ImAppWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam );
static bool					ImAppWindowHandleMessage( ImAppWindow* pWindow, UINT message, WPARAM wParam, LPARAM lParam );
static bool					ImAppInputHandleMessage( ImAppInputPlatform* pInput, UINT message, WPARAM wParam, LPARAM lParam );

ImAppWindow* ImAppWindowCreate( const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* pWindow = IMAPP_NEW_ZERO( ImAppWindow );
	if( pWindow == NULL )
	{
		return NULL;
	}

	MultiByteToWideChar( CP_UTF8, 0u, pWindowTitle, -1, pWindow->windowTitle, IMAPP_ARRAY_COUNT( pWindow->windowTitle ) - 1 );

	const HINSTANCE	hInstance = GetModuleHandle( NULL );

	WNDCLASSEXW windowClass = { 0 };
	windowClass.cbSize			= sizeof( WNDCLASSEXW );
	windowClass.hInstance		= hInstance;
	windowClass.lpfnWndProc		= &ImAppWindowProc;
	windowClass.lpszClassName	= s_pWindowClass;
	windowClass.hbrBackground	= (HBRUSH)COLOR_WINDOW;
	windowClass.hCursor			= LoadCursor( NULL, IDC_ARROW );

	const HRESULT result = RegisterClassExW( &windowClass );
	if( FAILED( result ) )
	{
		MessageBoxW( NULL, L"Can't register Class.", pWindow->windowTitle, MB_ICONSTOP );
		ImAppWindowDestroy( pWindow );
		return NULL;
	}

	pWindow->windowHandle = CreateWindowW(
		s_pWindowClass,
		pWindow->windowTitle,
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

	if( pWindow->windowHandle == NULL )
	{
		MessageBoxW( NULL, L"Can't create Window.", pWindow->windowTitle, MB_ICONSTOP );
		ImAppWindowDestroy( pWindow );
		return NULL;
	}

	SetWindowLongPtr( pWindow->windowHandle, GWLP_USERDATA, (LONG_PTR)pWindow );

	int showState = SW_SHOWNORMAL;
	switch( state )
	{
	case ImAppWindowState_Default: break;
	case ImAppWindowState_Maximized: showState = SW_SHOWMAXIMIZED; break;
	case ImAppWindowState_Minimized: showState = SW_SHOWMINIMIZED; break;
	}
	ShowWindow( pWindow->windowHandle, showState );

	RECT clientRect;
	GetClientRect( pWindow->windowHandle, &clientRect );

	pWindow->width	= (clientRect.right - clientRect.left);
	pWindow->height	= (clientRect.bottom - clientRect.top);

	return pWindow;
}

void ImAppWindowDestroy( ImAppWindow* pWindow )
{
	if( pWindow->windowHandle != NULL )
	{
		DestroyWindow( pWindow->windowHandle );
		pWindow->windowHandle = NULL;
	}

	ImAppFree( pWindow );
}

void ImAppWindowUpdate( ImAppWindow* pWindow )
{
	MSG message;
	while( PeekMessage( &message, NULL, 0u, 0u, PM_REMOVE ) )
	{
		TranslateMessage( &message );
		DispatchMessage( &message );
	}
}

bool ImAppWindowIsOpen( ImAppWindow* pWindow )
{
	return pWindow->isOpen;
}

void ImAppGetWindowSize( int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	*pWidth		= pWindow->width;
	*pHeight	= pWindow->height;
}

void ImAppGetWindowPosition( int* pX, int* pY, ImAppWindow* pWindow )
{
	*pX = pWindow->x;
	*pY = pWindow->y;
}

ImAppWindowState ImAppGetWindowState( ImAppWindow* pWindow )
{
	return pWindow->state;
}

static LRESULT CALLBACK ImAppWindowProc( HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam )
{
	ImAppWindow* pWindow = (ImAppWindow*)GetWindowLongPtr( hWnd, GWLP_USERDATA );
	if( pWindow != NULL )
	{
		switch( message )
		{
		case WM_CLOSE:
			{
				pWindow->isOpen = false;
			}
			return 0;

		case WM_MOVE:
			{
				pWindow->x = (int)(short)LOWORD( lParam );
				pWindow->y = (int)(short)HIWORD( lParam );
			}
			return 0;

		case WM_SIZE:
			{
				switch( wParam )
				{
				case SIZE_RESTORED:
					pWindow->state = ImAppWindowState_Default;
					break;

				case SIZE_MAXIMIZED:
					pWindow->state = ImAppWindowState_Maximized;
					break;

				case SIZE_MINIMIZED:
					pWindow->state = ImAppWindowState_Minimized;
					break;
				}

				RECT clientRect;
				GetClientRect( hWnd, &clientRect );

				pWindow->width	= (clientRect.right - clientRect.left);
				pWindow->height	= (clientRect.bottom - clientRect.top);
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

//////////////////////////////////////////////////////////////////////////
// SwapChain

struct ImAppSwapChain
{
	ImAppWindow*	pWindow;
};

ImAppSwapChain* ImAppCreateDeviceAndSwapChain( ImAppWindow* pWindow )
{
	ImAppSwapChain* pSwapChain = IMAPP_NEW_ZERO( ImAppSwapChain );
	if( pSwapChain == NULL )
	{
		return NULL;
	}

	return pSwapChain;
}

void ImAppDestroyDeviceAndSwapChain( ImAppSwapChain* pSwapChain )
{

}

bool ImAppResizeSwapChain( ImAppSwapChain* pSwapChain, int width, int height )
{
	return false;
}

//////////////////////////////////////////////////////////////////////////
// Input

struct ImAppInputPlatform
{
	ImAppWindow*	pWindow;

	WNDPROC			pOriginalWinProc;
};

//ImAppInputPlatform* ImAppInputPlatformCreate();
//void ImAppInputPlatformDestroy( ImAppInputPlatform* pPlatformState );
//void ImAppInputPlatformApply( ImAppInputPlatform* pPlatformState, struct nk_context* pNkContext );

#endif
