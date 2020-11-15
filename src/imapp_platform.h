#pragma once

//////////////////////////////////////////////////////////////////////////
// Main

int						ImAppMain( int argc, char* argv[] );

void					ImAppShowError( const char* pMessage );

//////////////////////////////////////////////////////////////////////////
// Shared Libraries

struct ImAppSharedLib;
typedef struct ImAppSharedLib* ImAppSharedLibHandle;

ImAppSharedLibHandle	ImAppSharedLibOpen( const char* pSharedLibName );
void					ImAppSharedLibClose( ImAppSharedLibHandle libHandle );
void*					ImAppSharedLibGetFunction( ImAppSharedLibHandle libHandle, const char* pFunctionName );

//////////////////////////////////////////////////////////////////////////
// Window
struct ImAppWindow;
typedef struct ImAppWindow ImAppWindow;

typedef enum ImAppWindowState
{
	ImAppWindowState_Default,
	ImAppWindowState_Maximized,
	ImAppWindowState_Minimized
} ImAppWindowState;

ImAppWindow*			ImAppWindowCreate( const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state );
void					ImAppWindowDestroy( ImAppWindow* pWindow );

int64_t					ImAppWindowWaitForEvent( ImAppWindow* pWindow, int64_t lastTickValue, int64_t tickInterval );

bool					ImAppWindowIsOpen( ImAppWindow* pWindow );
void					ImAppWindowGetSize( int* pWidth, int* pHeight, ImAppWindow* pWindow );
void					ImAppWindowGetPosition( int* pX, int* pY, ImAppWindow* pWindow );
ImAppWindowState		ImAppWindowGetState( ImAppWindow* pWindow );

//////////////////////////////////////////////////////////////////////////
// SwapChain

struct ImAppSwapChain;
typedef struct ImAppSwapChain ImAppSwapChain;

ImAppSwapChain*			ImAppCreateDeviceAndSwapChain( ImAppWindow* pWindow );
void					ImAppDestroyDeviceAndSwapChain( ImAppSwapChain* pSwapChain );

bool					ImAppSwapChainResize( ImAppSwapChain* pSwapChain, int width, int height );
void					ImAppSwapChainPresent( ImAppSwapChain* pSwapChain );

//////////////////////////////////////////////////////////////////////////
// Input

typedef enum ImAppEventType
{
	// Window
	ImAppEventType_WindowClose,

	// Input
	ImAppEventType_KeyDown,
	ImAppEventType_KeyUp,
	ImAppEventType_ButtonDown,
	ImAppEventType_ButtonUp,
	ImAppEventType_Motion

} ImAppEventType;

typedef struct ImAppWindowCloseEvent
{
	ImAppEventType type;
} ImAppWindowCloseEvent;

typedef union ImAppEvent
{
	ImAppEventType			type;

	ImAppWindowCloseEvent	windowClose;
} ImAppEvent;

struct ImAppInput;
typedef struct ImAppInput ImAppInput;

ImAppInput*				ImAppInputCreate( ImAppWindow* pWindow );
void					ImAppInputDestroy( ImAppInput* pInput );

void					ImAppInputApply( ImAppInput* pInput, struct nk_context* pNkContext );
