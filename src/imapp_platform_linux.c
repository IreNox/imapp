#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_LINUX )

#include "imapp_debug.h"
#include "imapp_drop_queue.h"
#include "imapp_event_queue.h"
#include "imapp_internal.h"

#include <EGL/egl.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <wayland-egl.h>

//////////////////////////////////////////////////////////////////////////
// Main

struct ImAppPlatform
{
	ImUiAllocator*				allocator;

	//ImUiInputKey	inputKeyMapping[ SDL_NUM_SCANCODES ];

	char*						resourceBasePath;
	size_t						resourceBasePathLength;

	char*						fontBasePath;
	size_t						fontBasePathLength;

	struct wl_display*			wlDisplay;
	struct wl_registry*			wlRegistry;
	struct wl_compositor*		wlCompositor;
	struct wl_shell*			wlShell;
	struct wl_seat*				wlSeat;
	struct wl_pointer*			wlPointer;
	struct wl_keyboard*			wlKeyboard;
	struct wl_shm*				wlShm;
	struct wl_cursor_theme*		wlCursorTheme;
	struct wl_cursor*			wlDefaultCursor;
	struct wl_surface*			wlCursorSurface;

	EGLDisplay					eglDisplay;

	//SDL_Cursor*		systemCursors[ ImUiInputMouseCursor_MAX ];
};

#include "imapp_platform_pthread.h"

struct ImAppWindow
{
	ImUiAllocator*				allocator;
	ImAppPlatform*				platform;
	ImAppEventQueue				eventQueue;
	ImAppWindowDoUiFunc			uiFunc;

	struct wl_egl_window*		wlWindow;
	struct wl_surface*			wlSurface;
	struct wl_shell_surface*	wlShellSurface;
	struct wl_callback*			wlDrawCallback;

	EGLSurface					eglSurface;
	EGLContext					eglContext;

	int							x;
	int							y;
	int							width;
	int							height;
	ImAppWindowState			state;
	ImAppWindowStyle			style;
	char*						title;
	uintsize					titleCapacity;
	float						dpiScale;

	ImAppWindowDropQueue		drops;
};

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

static void ImAppPlatformWaylandRegistryGlobalCallback( void *data, struct wl_registry *registry, uint32_t name, const char *interface, uint32_t version );
static void ImAppPlatformWaylandRegistryGlobalRemoveCallback( void* data, struct wl_registry* registry, uint32_t name );

static void ImAppPlatformWaylandShellSurfacePingCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t serial );
static void ImAppPlatformWaylandShellSurfaceConfigureCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t edges, int32_t width, int32_t height );
static void ImAppPlatformWaylandShellSurfacePopupDoneCallback( void* data, struct wl_shell_surface* shell_surface );

static void ImAppPlatformWaylandHandleWindowConfigCallback( void* data, struct wl_callback* callback, uint32_t time );
static void ImAppPlatformWaylandHandleWindowDrawCallback( void* data, struct wl_callback* callback, uint32_t time );

static const struct wl_registry_listener s_wlRegistryListener =
{
	  ImAppPlatformWaylandRegistryGlobalCallback,
	  ImAppPlatformWaylandRegistryGlobalRemoveCallback
};

static const struct wl_shell_surface_listener s_wlShellSurfaceListener =
{
	ImAppPlatformWaylandShellSurfacePingCallback,
	ImAppPlatformWaylandShellSurfaceConfigureCallback,
	ImAppPlatformWaylandShellSurfacePopupDoneCallback
};

static struct wl_callback_listener s_wlWindowConfigCallbackListener =
{
	ImAppPlatformWaylandHandleWindowConfigCallback
};

const struct wl_callback_listener s_wlWindowDrawCallbackListener =
{
	ImAppPlatformWaylandHandleWindowDrawCallback
};

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

	// Linux sucks and there one million ways to store fonts, so hardcoded one path for my distribution.
	const char fontsPath[] = "/run/current-system/sw/share/X11/fonts/";

	platform.fontBasePathLength = IMAPP_ARRAY_COUNT( fontsPath ) - 1;
	platform.fontBasePath = malloc( platform.fontBasePathLength + 1 );
	if( !platform.fontBasePath )
	{
		return 1;
	}

	strncpy( platform.fontBasePath, fontsPath, platform.fontBasePathLength + 1 );

	const int result = ImAppMain( &platform, argc, argv );

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	platform->wlDisplay = wl_display_connect( NULL );
	if( !platform->wlDisplay )
	{
		IMAPP_DEBUG_LOGE( "Failed to connect to Wayland server." );
		return false;
	}

	platform->eglDisplay = eglGetDisplay( (EGLNativeDisplayType)platform->wlDisplay );
	if( platform->eglDisplay == EGL_NO_DISPLAY )
	{
		IMAPP_DEBUG_LOGE( "Failed to get to EGL display." );
		return false;
	}

	EGLint major;
	EGLint minor;
	const EGLBoolean initResult = eglInitialize( platform->eglDisplay, &major, &minor );
	if( initResult != EGL_TRUE )
	{
		IMAPP_DEBUG_LOGE( "Failed toinitialize EGL." );
		return false;
	}

	platform->wlRegistry = wl_display_get_registry( platform->wlDisplay );
	wl_registry_add_listener( platform->wlRegistry, &s_wlRegistryListener, platform );
	wl_display_dispatch( platform->wlDisplay );

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

	char exePath[ PATH_MAX ];
	uintsize exePathLength = 0;

	if( isRelativePath )
	{
		const sintsize exeLinkResult = readlink( "/proc/self/exe", exePath, sizeof( exePath ) );
		if( exeLinkResult < 0 )
		{
			return false;
		}

		exePathLength = (uintsize)exeLinkResult;
		platform->resourceBasePathLength += exePathLength;
		platform->resourceBasePathLength -= 2; // for "./"
	}

	platform->resourceBasePath = malloc( platform->resourceBasePathLength + 1 );
	if( !resourcePath )
	{
		return false;
	}

	if( isRelativePath )
	{
		memcpy( platform->resourceBasePath, exePath, exePathLength );
		platform->resourceBasePath[ exePathLength ] = '\0';

		sourcePath += 2;
	}

	{
		char* targetPath = strrchr( platform->resourceBasePath, '/' );
		if( targetPath == NULL )
		{
			targetPath = platform->resourceBasePath;
		}
		else
		{
			targetPath++;
		}

		strncpy( targetPath, sourcePath, platform->resourceBasePathLength - (uintsize)(targetPath - platform->resourceBasePath) + 1 );

		if( needsSeperatorEnd )
		{
			platform->resourceBasePath[ platform->resourceBasePathLength - 1u ] = '/';
			platform->resourceBasePath[ platform->resourceBasePathLength ] = '\0';
		}
	}

	return true;
}

void ImAppPlatformShutdown( ImAppPlatform* platform )
{
	//for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->systemCursors ); ++i )
	//{
	//	SDL_FreeCursor( platform->systemCursors[ i ] );
	//	platform->systemCursors[ i ] = NULL;
	//}

	platform->allocator = NULL;
}

int64_t ImAppPlatformTick( ImAppPlatform* platform, int64_t lastTickValue, int64_t tickInterval )
{
	//int64_t currentTick			= (int64_t)SDL_GetTicks64();
	//const int64_t deltaTicks	= currentTick - lastTickValue;
	//int64_t timeToWait = IMUI_MAX( tickInterval, deltaTicks ) - deltaTicks;

	//if( tickInterval == 0 )
	//{
	//	timeToWait = INT_MAX;
	//}

	//if( timeToWait > 1 )
	//{
	//	SDL_WaitEventTimeout( NULL, (int)timeToWait - 1 );

	//	currentTick = (int64_t)SDL_GetTicks64();
	//}

	//return (int64_t)currentTick;
	return 0;
}

static void ImAppPlatformWaylandRegistryGlobalCallback( void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version )
{

	ImAppPlatform* platform = (ImAppPlatform*)data;

	if( strcmp( interface, "wl_compositor" ) == 0 )
	{
		platform->wlCompositor = (struct wl_compositor*)wl_registry_bind( registry, name, &wl_compositor_interface, 1 );

		platform->wlCursorSurface = wl_compositor_create_surface( platform->wlCompositor );
	}
	else if( strcmp( interface, "wl_shell" ) == 0 )
	{
		platform->wlShell = (struct wl_shell*)wl_registry_bind( registry, name, &wl_shell_interface, 1 );
	}
	else if( strcmp( interface, "wl_seat" ) == 0 )
	{
		//d->seat = static_cast<wl_seat*>(wl_registry_bind( registry, name, &wl_seat_interface, 1 ));
		//wl_seat_add_listener( d->seat, &seat_listener, d );
	}
	else if( strcmp( interface, "wl_shm" ) == 0 )
	{
		//d->shm = static_cast<wl_shm*>(wl_registry_bind( registry, name, &wl_shm_interface, 1 ));
		//d->cursor_theme = wl_cursor_theme_load( NULL, 32, d->shm );
		//d->default_cursor = wl_cursor_theme_get_cursor( d->cursor_theme, "left_ptr" );
	}

}

static void ImAppPlatformWaylandRegistryGlobalRemoveCallback( void* data, struct wl_registry* registry, uint32_t name )
{

}

void ImAppPlatformShowError( ImAppPlatform* pPlatform, const char* message )
{
	//int fd_pipe[ 2 ]; /* fd_pipe[0]: read end of pipe, fd_pipe[1]: write end of pipe */
	//int zenity_major = 0, zenity_minor = 0, output_len = 0;
	//int argc = 5, i;
	//const char *argv[ 5 + 2 /* icon name */ + 2 /* title */ + 2 /* message */ + 2 * MAX_BUTTONS + 1 /* NULL */ ] = {
	//	"zenity", "--question", "--switch", "--no-wrap", "--no-markup"
	//};

	//// KDE: kdialog

	//if( get_zenity_version( &zenity_major, &zenity_minor ) != 0 )
	//{
	//	return;
	//}

	//if( pipe( fd_pipe ) != 0 )
	//{
	//	return;
	//}

	///* https://gitlab.gnome.org/GNOME/zenity/-/commit/c686bdb1b45e95acf010efd9ca0c75527fbb4dea
	// * This commit removed --icon-name without adding a deprecation notice.
	// * We need to handle it gracefully, otherwise no message box will be shown.
	// */
	//argv[ argc++ ] = zenity_major > 3 || (zenity_major == 3 && zenity_minor >= 90) ? "--icon" : "--icon-name";
	//argv[ argc++ ] = "dialog-error";

	//argv[ argc++ ] = "--title=\"I'm app\"";

	//argv[ argc++ ] = "--text";
	//argv[ argc++ ] = message;

	//argv[ argc++ ] = "--extra-button=\"Ok\"";

	//argv[ argc ] = NULL;

	//if( run_zenity( argv, fd_pipe ) == 0 )
	//{
	//	FILE *outputfp = NULL;
	//	char *output = NULL;
	//	char *tmp = NULL;

	//	if( buttonid == NULL )
	//	{
	//		/* if we don't need buttonid, we can return immediately */
	//		close( fd_pipe[ 0 ] );
	//		return 0; /* success */
	//	}
	//	*buttonid = -1;

	//	output = SDL_malloc( output_len + 1 );
	//	if( output == NULL )
	//	{
	//		close( fd_pipe[ 0 ] );
	//		return SDL_OutOfMemory();
	//	}
	//	output[ 0 ] = '\0';

	//	outputfp = fdopen( fd_pipe[ 0 ], "r" );
	//	if( outputfp == NULL )
	//	{
	//		SDL_free( output );
	//		close( fd_pipe[ 0 ] );
	//		return SDL_SetError( "Couldn't open pipe for reading: %s", strerror( errno ) );
	//	}
	//	tmp = fgets( output, output_len + 1, outputfp );
	//	(void)fclose( outputfp );

	//	if( (tmp == NULL) || (*tmp == '\0') || (*tmp == '\n') )
	//	{
	//		SDL_free( output );
	//		return 0; /* User simply closed the dialog */
	//	}

	//	/* It likes to add a newline... */
	//	tmp = SDL_strrchr( output, '\n' );
	//	if( tmp != NULL )
	//	{
	//		*tmp = '\0';
	//	}

	//	/* Check which button got pressed */
	//	for( i = 0; i < messageboxdata->numbuttons; i += 1 )
	//	{
	//		if( messageboxdata->buttons[ i ].text != NULL )
	//		{
	//			if( SDL_strcmp( output, messageboxdata->buttons[ i ].text ) == 0 )
	//			{
	//				*buttonid = messageboxdata->buttons[ i ].buttonid;
	//				break;
	//			}
	//		}
	//	}

	//	SDL_free( output );
	//	return 0; /* success! */
	//}

	//close( fd_pipe[ 0 ] );
	//close( fd_pipe[ 1 ] );
	//return -1; /* run_zenity() calls SDL_SetError(), so message is already set */

	//SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", pMessage, NULL );
}

void ImAppPlatformSetMouseCursor( ImAppPlatform* platform, ImUiInputMouseCursor cursor )
{
	//SDL_SetCursor( platform->systemCursors[ cursor ] );
}

void ImAppPlatformSetClipboardText( ImAppPlatform* platform, const char* text )
{
	//SDL_SetClipboardText( text );
}

void ImAppPlatformGetClipboardText( ImAppPlatform* platform, ImUiContext* imui )
{
	//const char* clipboardText = SDL_GetClipboardText();
	//ImUiInputSetPasteText( imui, clipboardText );
}

ImAppWindow* ImAppPlatformWindowCreate( ImAppPlatform* platform, const char* windowTitle, int x, int y, int width, int height, ImAppWindowStyle style, ImAppWindowState state, ImAppWindowDoUiFunc uiFunc )
{
	if( !platform->wlCompositor )
	{
		return NULL;
	}

	ImAppWindow* window = IMUI_MEMORY_NEW_ZERO( platform->allocator, ImAppWindow );
	if( window == NULL )
	{
		return NULL;
	}

	window->allocator	= platform->allocator;
	window->platform	= platform;
	window->uiFunc		= uiFunc;
	window->width		= width;
	window->height		= height;
	window->dpiScale	= 1.0f;

	window->wlSurface = wl_compositor_create_surface( platform->wlCompositor );
	if( !window->wlSurface )
	{
		IMAPP_DEBUG_LOGE( "Failed to create Wayland Surface." );
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	window->wlShellSurface = wl_shell_get_shell_surface( platform->wlShell, window->wlSurface );
	if( !window->wlShellSurface )
	{
		IMAPP_DEBUG_LOGE( "Failed to create Wayland Shell Surface." );
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	wl_shell_surface_add_listener( window->wlShellSurface, &s_wlShellSurfaceListener, window );

	window->wlWindow = wl_egl_window_create( window->wlSurface, width, height );
	if( !window->wlWindow )
	{
		IMAPP_DEBUG_LOGE( "Failed to create Wayland Window." );
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	wl_shell_surface_set_title( window->wlShellSurface, windowTitle );

	wl_shell_surface_set_toplevel( window->wlShellSurface );

	//callback = wl_display_sync( platform->eglDisplay );
	//wl_callback_add_listener( callback, &configure_callback_listener, this );

	//Uint32 flags = SDL_WINDOW_OPENGL | SDL_WINDOW_SHOWN;
	//switch( style )
	//{
	//case ImAppWindowState_Resizable:
	//	flags |= SDL_WINDOW_RESIZABLE;
	//	break;

	//case ImAppWindowState_Borderless:
	//	flags |= SDL_WINDOW_BORDERLESS;
	//	if( state == ImAppWindowState_Maximized )
	//	{
	//		flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
	//	}
	//	break;
	//}

	//switch( state )
	//{
	//case ImAppWindowState_Default:
	//	flags |= SDL_WINDOW_SHOWN;
	//	break;

	//case ImAppWindowState_Minimized:
	//	flags |= SDL_WINDOW_MINIMIZED;
	//	break;

	//case ImAppWindowState_Maximized:
	//	flags |= SDL_WINDOW_MAXIMIZED;
	//	break;
	//}

	//window->sdlWindow = SDL_CreateWindow( windowTitle, SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), width, height, flags );
	//if( window->sdlWindow == NULL )
	//{
	//	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to create Window.", NULL );

	//	ImUiMemoryFree( platform->allocator, window );
	//	return NULL;
	//}

	ImAppEventQueueConstruct( &window->eventQueue, platform->allocator );

	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{
	IMAPP_ASSERT( window->eglContext == EGL_NO_CONTEXT );

	//while( window->firstNewDrop )
	//{
	//	ImAppWindowDrop* drop = window->firstNewDrop;
	//	window->firstNewDrop = drop->nextDrop;

	//	ImUiMemoryFree( window->platform->allocator, drop );
	//}

	//while( window->firstPoppedDrop )
	//{
	//	ImAppWindowDrop* drop = window->firstPoppedDrop;
	//	window->firstPoppedDrop = drop->nextDrop;

	//	ImUiMemoryFree( window->platform->allocator, drop );
	//}

	ImAppEventQueueDestruct( &window->eventQueue );

	if( window->wlWindow )
	{
		wl_egl_window_destroy( window->wlWindow );
		window->wlWindow = NULL;
	}

	if( window->wlShellSurface )
	{
		wl_shell_surface_destroy( window->wlShellSurface );
		window->wlShellSurface = NULL;
	}

	if( window->wlSurface )
	{
		wl_surface_destroy( window->wlSurface );
		window->wlSurface = NULL;
	}

	ImUiMemoryFree( window->allocator, window );
}

static void ImAppPlatformWaylandShellSurfacePingCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t serial )
{
	wl_shell_surface_pong( shell_surface, serial );
}

static void ImAppPlatformWaylandShellSurfaceConfigureCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t edges, int32_t width, int32_t height )
{
	ImAppWindow* window = (ImAppWindow*)data;

	if( window->wlWindow )
	{
		wl_egl_window_resize( window->wlWindow, width, height, 0, 0 );
	}

	window->width = width;
	window->height = height;

	//if( window->state != ImAppWindowState_Maximized )
	//{
	//	window->window_size = window->geometry;
	//}
}

static void ImAppPlatformWaylandShellSurfacePopupDoneCallback( void* data, struct wl_shell_surface* shell_surface )
{
}

static void ImAppPlatformWaylandHandleWindowConfigCallback( void* data, struct wl_callback* callback, uint32_t time )
{
	ImAppWindow* window = (ImAppWindow*)data;

	wl_callback_destroy( callback );

	//window->configured = 1;

	if( !window->wlDrawCallback )
	{
		ImAppPlatformWaylandHandleWindowDrawCallback( data, NULL, time );
	}
}

static void ImAppPlatformWaylandHandleWindowDrawCallback( void* data, struct wl_callback* callback, uint32_t time )
{
	ImAppWindow* window = (ImAppWindow*)data;

	if( callback )
	{
		wl_callback_destroy( callback );
	}

	//struct wl_region* region;
	//
	//assert( window->callback == callback );
	//window->callback = NULL;


	//if( !window->configured )
	//	return;

	//window->drawPtr( window );

	//if( window->opaque || window->fullscreen )
	//{
	//	region = wl_compositor_create_region( window->display->compositor );
	//	wl_region_add( region, 0, 0, window->geometry.width,
	//					window->geometry.height );
	//	wl_surface_set_opaque_region( window->surface, region );
	//	wl_region_destroy( region );
	//}
	//else
	//{
	//	wl_surface_set_opaque_region( window->surface, NULL );
	//}

	//window->wlDrawCallback = wl_surface_frame( window->wlSurface );
	//wl_callback_add_listener( window->wlDrawCallback, &s_wlWindowDrawCallbackListener, window );

	//eglSwapBuffers( window->platform->eglDisplay, window->eglSurface );
}

bool ImAppPlatformWindowCreateGlContext( ImAppWindow* window )
{
	// Choose config
	EGLint attribList[] =
	{
		EGL_RED_SIZE,			8,
		EGL_GREEN_SIZE,			8,
		EGL_BLUE_SIZE,			8,
		EGL_ALPHA_SIZE,			8,
		EGL_COLOR_BUFFER_TYPE,	EGL_RGB_BUFFER,
		//EGL_ALPHA_SIZE,	 (flags & ES_WINDOW_ALPHA) ? 8 : EGL_DONT_CARE,
		//EGL_SAMPLE_BUFFERS, (flags & ES_WINDOW_MULTISAMPLE) ? 1 : 0,
		EGL_NONE
	};
	//EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };

	EGLConfig config;
	EGLint numConfigs;
	if( !eglChooseConfig( window->platform->eglDisplay, attribList, &config, 1, &numConfigs ) )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	// Create a surface
	window->eglSurface = eglCreateWindowSurface( window->platform->eglDisplay, config, (EGLNativeWindowType)window->wlWindow, NULL );
	if( window->eglSurface == EGL_NO_SURFACE )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	// Create a GL context
	EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE, EGL_NONE };
	window->eglContext = eglCreateContext( window->platform->eglDisplay, config, EGL_NO_CONTEXT, contextAttribs );
	if( window->eglContext == EGL_NO_CONTEXT )
	{
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	const EGLBoolean makeCurrentResult = eglMakeCurrent( window->platform->eglDisplay, window->eglSurface, window->eglSurface, window->eglContext );
	if( makeCurrentResult != EGL_TRUE )
	{
		IMAPP_DEBUG_LOGE( "Failed to set GL context. Result: %d", makeCurrentResult );
		ImAppPlatformWindowDestroyGlContext( window );
		return false;
	}

	return true;
}

void ImAppPlatformWindowDestroyGlContext( ImAppWindow* window )
{
	eglMakeCurrent( window->platform->eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT );

	if( window->eglContext != NULL )
	{
		eglDestroyContext( window->platform->eglDisplay, window->eglContext );
		window->eglContext = EGL_NO_CONTEXT;
	}

	if( window->eglSurface )
	{
		eglDestroySurface( window->platform->eglDisplay, window->eglSurface );
		window->eglSurface = EGL_NO_SURFACE;
	}
}

ImAppWindowDeviceState ImAppPlatformWindowGetGlContextState( const ImAppWindow* window )
{
	return ImAppWindowDeviceState_Ok;
}

void ImAppPlatformWindowUpdate( ImAppWindow* window, ImAppPlatformWindowUpdateCallback callback, void* arg )
{
#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
	SDL_GL_SetSwapInterval( 1 );
#endif

//	while( window->firstPoppedDrop )
//	{
//		ImAppWindowDrop* drop = window->firstPoppedDrop;
//		window->firstPoppedDrop = drop->nextDrop;
//
//		ImUiMemoryFree( window->platform->allocator, drop );
//	}
//
//	SDL_Event sdlEvent;
//	while( SDL_PollEvent( &sdlEvent ) )
//	{
//		switch( sdlEvent.type )
//		{
//		case SDL_KEYDOWN:
//		case SDL_KEYUP:
//			{
//				const SDL_KeyboardEvent* sdlKeyEvent = &sdlEvent.key;
//
//				const ImUiInputKey mappedKey = window->platform->inputKeyMapping[ sdlKeyEvent->keysym.scancode ];
//				if( mappedKey != ImUiInputKey_None )
//				{
//					const ImAppEventType eventType	= sdlKeyEvent->type == SDL_KEYDOWN ? ImAppEventType_KeyDown : ImAppEventType_KeyUp;
//					const bool repeate				= sdlKeyEvent->repeat != 0;
//					const ImAppEvent keyEvent		= { .key = { .type = eventType, .key = mappedKey, .repeat = repeate } };
//					ImAppEventQueuePush( &window->eventQueue, &keyEvent );
//				}
//			}
//			break;
//
//		case SDL_TEXTINPUT:
//			{
//				const SDL_TextInputEvent* textInputEvent = &sdlEvent.text;
//
//				for( const char* pText = textInputEvent->text; *pText != '\0'; ++pText )
//				{
//					const ImAppEvent charEvent = { .character = { .type = ImAppEventType_Character, .character = *pText } };
//					ImAppEventQueuePush( &window->eventQueue, &charEvent );
//				}
//			}
//			break;
//
//		case SDL_MOUSEMOTION:
//			{
//				const SDL_MouseMotionEvent* sdlMotionEvent = &sdlEvent.motion;
//
//				const ImAppEvent motionEvent = { .motion = { .type = ImAppEventType_Motion, .x = sdlMotionEvent->x, .y = sdlMotionEvent->y } };
//				ImAppEventQueuePush( &window->eventQueue, &motionEvent );
//			}
//			break;
//
//		case SDL_MOUSEBUTTONDOWN:
//		case SDL_MOUSEBUTTONUP:
//			{
//				const SDL_MouseButtonEvent* sdlButtonEvent = &sdlEvent.button;
//
//				ImUiInputMouseButton button;
//				switch( sdlButtonEvent->button )
//				{
//				case SDL_BUTTON_LEFT:	button = ImUiInputMouseButton_Left; break;
//				case SDL_BUTTON_MIDDLE:	button = ImUiInputMouseButton_Middle; break;
//				case SDL_BUTTON_RIGHT:	button = ImUiInputMouseButton_Right; break;
//				case SDL_BUTTON_X1:		button = ImUiInputMouseButton_X1; break;
//				case SDL_BUTTON_X2:		button = ImUiInputMouseButton_X2; break;
//
//				default:
//					continue;
//				}
//
//				const ImAppEventType eventType	= sdlButtonEvent->type == SDL_MOUSEBUTTONDOWN ? ImAppEventType_ButtonDown : ImAppEventType_ButtonUp;
//				const ImAppEvent buttonEvent	= { .button = { .type = eventType, .x = sdlButtonEvent->x, .y = sdlButtonEvent->y, .button = button, .repeateCount = sdlButtonEvent->clicks } };
//				ImAppEventQueuePush( &window->eventQueue, &buttonEvent );
//			}
//			break;
//
//		case SDL_MOUSEWHEEL:
//			{
//				const SDL_MouseWheelEvent* sdlWheelEvent = &sdlEvent.wheel;
//
//				const ImAppEvent scrollEvent = { .scroll = { .type = ImAppEventType_Scroll, .x = sdlWheelEvent->x, .y = sdlWheelEvent->y } };
//				ImAppEventQueuePush( &window->eventQueue, &scrollEvent );
//			}
//			break;
//
//		case SDL_WINDOWEVENT:
//			{
//				const SDL_WindowEvent* sdlWindowEvent = &sdlEvent.window;
//				switch( sdlWindowEvent->event )
//				{
//				case SDL_WINDOWEVENT_CLOSE:
//					{
//						const ImAppEvent closeEvent = { .type = ImAppEventType_WindowClose };
//						ImAppEventQueuePush( &window->eventQueue, &closeEvent );
//					}
//					break;
//
//				default:
//					break;
//				}
//			}
//			break;
//
//		case SDL_DROPFILE:
//		case SDL_DROPTEXT:
//			{
//				const SDL_DropEvent* sdlDropEvent = &sdlEvent.drop;
//
//				const uintsize textLength = strlen( sdlDropEvent->file ) + 1u;
//				ImAppWindowDrop* drop = (ImAppWindowDrop*)ImUiMemoryAlloc( window->platform->allocator, sizeof( ImAppWindowDrop ) + textLength );
//				drop->type = sdlEvent.type == SDL_DROPFILE ? ImAppDropType_File : ImAppDropType_Text;
//
//				memcpy( drop->pathOrText, sdlDropEvent->file, textLength );
//				drop->pathOrText[ textLength ] = '\0';
//
//				drop->nextDrop = window->firstNewDrop;
//				window->firstNewDrop = drop;
//			}
//			break;
//
//		case SDL_QUIT:
//#if IMAPP_ENABLED( IMAPP_PLATFORM_WEB )
//			emscripten_cancel_main_loop();
//#else
//			// TODO
//#endif
//			break;
//		}
//	}
}

bool ImAppPlatformWindowPresent( ImAppWindow* window )
{
	if( window->eglContext == EGL_NO_CONTEXT )
	{
		return false;
	}

	if( !eglSwapBuffers( window->platform->eglDisplay, window->eglSurface ) )
	{
		return false;
	}

	return true;
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
	//if( !window->firstNewDrop )
	//{
	//	return false;
	//}

	//ImAppWindowDrop* drop = window->firstNewDrop;
	//outData->type		= drop->type;
	//outData->pathOrText	= drop->pathOrText;

	//window->firstNewDrop = drop->nextDrop;

	//drop->nextDrop = window->firstPoppedDrop;
	//window->firstPoppedDrop = drop;

	//return true;
	return false;
}

void ImAppPlatformWindowGetViewRect( const ImAppWindow* window, int* outX, int* outY, int* outWidth, int* outHeight )
{
	*outX = 0;
	*outY = 0;

	ImAppPlatformWindowGetSize( window, outWidth, outHeight );
}

bool ImAppPlatformWindowHasFocus( const ImAppWindow* window )
{
	// TODO
	return true;
}

void ImAppPlatformWindowGetSize( const ImAppWindow* window, int* outWidth, int* outHeight )
{
	*outWidth	= window->width;
	*outHeight	= window->height;
}

void ImAppPlatformWindowSetSize( ImAppWindow* window, int width, int height )
{
	window->width	= width;
	window->height	= height;

	wl_egl_window_resize( window->wlWindow, width, height, 0, 0 );
}

void ImAppPlatformWindowGetPosition( const ImAppWindow* window, int* outX, int* outY )
{
	//SDL_GetWindowPosition( window->sdlWindow, outX, outY );
}

void ImAppPlatformWindowSetPosition( const ImAppWindow* window, int x, int y )
{

}

ImAppWindowState ImAppPlatformWindowGetState( const ImAppWindow* pWindow )
{
	//const Uint32 flags = SDL_GetWindowFlags( pWindow->sdlWindow );
	//if( flags & SDL_WINDOW_MINIMIZED )
	//{
	//	return ImAppWindowState_Minimized;
	//}
	//if( flags & SDL_WINDOW_MAXIMIZED )
	//{
	//	return ImAppWindowState_Maximized;
	//}

	return ImAppWindowState_Default;
}

void ImAppPlatformWindowSetState( ImAppWindow* window, ImAppWindowState state )
{
}

const char* ImAppPlatformWindowGetTitle( const ImAppWindow* window )
{
	return NULL;
}

void ImAppPlatformWindowSetTitle( ImAppWindow* window, const char* title )
{
}

void ImAppPlatformWindowSetTitleBounds( ImAppWindow* window, int height, int buttonsX )
{
}

float ImAppPlatformWindowGetDpiScale( const ImAppWindow* window )
{
	//float dpi;
	//if( SDL_GetDisplayDPI( SDL_GetWindowDisplayIndex( window->sdlWindow ), &dpi, NULL, NULL ) != 0 )
	//{
	//	return 1.0f;
	//}

	//return dpi / 96.0f;
	return 1.0f;
}

void ImAppPlatformWindowClose( ImAppWindow* window )
{

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
		struct stat fileStats;
		if( fstat( fileno( (FILE*)file ), &fileStats ) != 0 )
		{
			ImAppPlatformResourceClose( platform, file );
			const ImAppBlob result = { NULL, 0u };
			return result;
		}

		length = (uintsize)fileStats.st_size - offset;
		if( length > (uintsize)fileStats.st_size )
		{
			length = 0;
		}
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

	FILE* file = fopen( platform->resourceBasePath, "rb" );
	if( !file )
	{
		ImAppTrace( "Error: Failed to open '%s'\n", platform->resourceBasePath );
		return NULL;
	}

	return (ImAppFile*)file;
}
uintsize ImAppPlatformResourceRead( ImAppFile* file, void* outData, uintsize length, uintsize offset )
{
	FILE* nativeFile = (FILE*)file;

	if( fseek( nativeFile, (long)offset, SEEK_SET ) == -1 )
	{
		return 0u;
	}

	const size_t readResult = fread( outData, 1u, length, nativeFile );
	if( readResult == 0u )
	{
		return 0u;
	}

	return readResult;
}

void ImAppPlatformResourceClose( ImAppPlatform* platform, ImAppFile* file )
{
	FILE* nativeFile = (FILE*)file;
	fclose( nativeFile );
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

	FILE* file = fopen( fontPath, "rb" );
	if( !file )
	{
		ImAppTrace( "Error: Failed to open '%s'\n", fontPath );
		ImUiMemoryFree( platform->allocator, fontPath );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	struct stat fileStats;
	if( fstat( fileno( file ), &fileStats ) != 0 )
	{
		ImUiMemoryFree( platform->allocator, fontPath );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	void* memory = ImUiMemoryAlloc( platform->allocator, (size_t)fileStats.st_size );

	const size_t readResult = fread( memory, 1u, (size_t)fileStats.st_size, file );
	fclose( file );

	if( readResult != fileStats.st_size )
	{
		ImUiMemoryFree( platform->allocator, memory );
		ImUiMemoryFree( platform->allocator, fontPath );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	ImUiMemoryFree( platform->allocator, fontPath );
	const ImAppBlob result = { memory, (size_t)fileStats.st_size };
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

#endif
