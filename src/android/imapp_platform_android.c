#include "../imapp_platform.h"

#include "imapp_android_activity.h"

#include <dlfcn.h>

//////////////////////////////////////////////////////////////////////////
// Main

void ImAppShowError( const char* pMessage )
{

}

//////////////////////////////////////////////////////////////////////////
// Shared Libraries

ImAppSharedLibHandle ImAppSharedLibOpen( const char* pSharedLibName )
{
	const ImAppSharedLibHandle handle = (ImAppSharedLibHandle)dlopen( pSharedLibName, RTLD_NOW | RTLD_LOCAL );
	if( handle == NULL )
	{
		const char* pErrorText = dlerror();
		ImAppTrace( "Failed loading shared lib '%s'. Error: %s", pSharedLibName, pErrorText );
		return NULL;
	}

	return handle;
}

void ImAppSharedLibClose( ImAppSharedLibHandle libHandle )
{
	dlclose( libHandle );
}

void* ImAppSharedLibGetFunction( ImAppSharedLibHandle libHandle, const char* pFunctionName )
{
	return dlsym( libHandle, pFunctionName );
}

//////////////////////////////////////////////////////////////////////////
// Window


ImAppWindow* ImAppWindowCreate( const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state )
{
	ImAppWindow* pWindow = AcquireNativeWindow();
	if( pWindow == NULL )
	{
		return NULL;
	}

	return pWindow;
}

void ImAppWindowDestroy( ImAppWindow* pWindow )
{
	ReleaseNativeWindow( pWindow );
}

int64_t ImAppWindowWaitForEvent( ImAppWindow* pWindow, int64_t lastTickValue, int64_t tickInterval )
{
	if( pWindow->pNativeWindow == NULL )
	{
		return 0;
	}

	return 1;
}

bool ImAppWindowIsOpen( ImAppWindow* pWindow )
{
	return pWindow->isRunning;
}

void ImAppWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* pWindow )
{
	if( pWindow->pNativeWindow == NULL )
	{
		*pWidth		= 0u;
		*pHeight	= 0u;
		return;
	}

	*pWidth		= ANativeWindow_getWidth( pWindow->pNativeWindow );
	*pHeight	= ANativeWindow_getHeight( pWindow->pNativeWindow );
}

void ImAppWindowGetPosition( int* pX, int* pY, ImAppWindow* pWindow )
{
	*pX	= 0u;
	*pY	= 0u;
}

ImAppWindowState ImAppWindowGetState( ImAppWindow* pWindow )
{
	return ImAppWindowState_Maximized;
}

//////////////////////////////////////////////////////////////////////////
// SwapChain

struct ImAppSwapChain
{

};

ImAppSwapChain* ImAppCreateDeviceAndSwapChain( ImAppWindow* pWindow )
{
	return NULL;
}

void ImAppDestroyDeviceAndSwapChain( ImAppSwapChain* pSwapChain )
{

}

bool ImAppSwapChainResize( ImAppSwapChain* pSwapChain, int width, int height )
{
	return false;
}

void ImAppSwapChainPresent( ImAppSwapChain* pSwapChain )
{

}