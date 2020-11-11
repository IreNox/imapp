#pragma once

//////////////////////////////////////////////////////////////////////////
// Main

int					ImAppMain( int argc, char* argv[] );

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

ImAppWindow*		ImAppWindowCreate( const char* pWindowTitle, int x, int y, int width, int height, ImAppWindowState state );
void				ImAppWindowDestroy( ImAppWindow* pWindow );

void				ImAppWindowUpdate( ImAppWindow* pWindow );

bool				ImAppWindowIsOpen( ImAppWindow* pWindow );
void				ImAppGetWindowSize( int* pWidth, int* pHeight, ImAppWindow* pWindow );
void				ImAppGetWindowPosition( int* pX, int* pY, ImAppWindow* pWindow );
ImAppWindowState	ImAppGetWindowState( ImAppWindow* pWindow );

//////////////////////////////////////////////////////////////////////////
// Input

typedef enum ImAppInputEventType
{
	ImAppInputEventType_KeyDown,
	ImAppInputEventType_KeyUp,
	ImAppInputEventType_ButtonDown,
	ImAppInputEventType_ButtonUp,
	ImAppInputEventType_Motion,

} ImAppInputEventType;

typedef union ImAppInputEvent
{
	int bla;
} ImAppInputEvent;

struct ImAppInputPlatform;
typedef struct ImAppInputPlatform ImAppInputPlatform;

ImAppInputPlatform*	ImAppInputPlatformCreate( ImAppWindow* pWindow );
void				ImAppInputPlatformDestroy( ImAppInputPlatform* pPlatformState );

void				ImAppInputPlatformApply( ImAppInputPlatform* pPlatformState, struct nk_context* pNkContext );
