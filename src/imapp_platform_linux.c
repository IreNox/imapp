#include "imapp_platform.h"

#if IMAPP_ENABLED( IMAPP_PLATFORM_LINUX ) && IMAPP_DISABLED( IMAPP_PLATFORM_SDL )

#include "imapp_debug.h"
#include "imapp_drop_queue.h"
#include "imapp_event_queue.h"
#include "imapp_internal.h"

#include <EGL/egl.h>
#include <linux/input-event-codes.h>
#include <linux/limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/unistd.h>
#include <unistd.h>
#include <wayland-egl.h>
#include <xkbcommon/xkbcommon.h>

#include "xdg-shell.h"
#include "xdg-decoration-unstable-v1-client-protocol.h"

//////////////////////////////////////////////////////////////////////////
// Main

//typedef struct ImAppPlatformLinuxFont
//{
//	const char*					path;
//	uintsize					pathLength;
//} ImAppPlatformLinuxFont;

struct ImAppPlatform
{
	ImUiAllocator*				allocator;

	ImUiInputKey				inputKeyMapping[ 65536u ];

	char*						resourceBasePath;
	uintsize					resourceBasePathLength;

	//ImAppPlatformLinuxFont*		fonts;
	//uintsize					fontsCapacity;
	//uintsize					fontsCount;

	struct wl_display*			wlDisplay;
	struct wl_registry*			wlRegistry;
	struct wl_compositor*		wlCompositor;
	struct wl_subcompositor*	wlSubCompositor;
	struct wl_shell*			wlShell;
	struct wl_seat*				wlSeat;
	struct wl_pointer*			wlPointer;
	struct wl_keyboard*			wlKeyboard;
	struct wl_shm*				wlShm;
	struct wl_cursor_theme*		wlCursorTheme;
	struct wl_cursor*			wlDefaultCursor;
	struct wl_surface*			wlCursorSurface;

	struct xdg_wm_base*			xdgWmBase;
	struct zxdg_decoration_manager_v1* zxdgDecorationManager;

	struct xkb_context*			xkbContext;
	struct xkb_keymap*			xkbKeymap;
	struct xkb_state*			xkbState;

	EGLDisplay					eglDisplay;

	ImAppWindow**				windows;
	uintsize					windowsCapacity;
	uintsize					windowsCount;

	ImAppWindow*				mouseFocusWindow;
	ImAppWindow*				keyboardFocusWindow;

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
	//struct wl_shell_surface*	wlShellSurface;
	//struct wl_callback*			wlDrawCallback;

	struct xdg_surface*			xdgSurface;
	struct xdg_toplevel*		xdgToplevel;
	struct zxdg_toplevel_decoration_v1* xdgDecoration;

	EGLSurface					eglSurface;
	EGLContext					eglContext;

	bool						isInitialized;
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

//static void ImAppPlatformLinuxReadFontConfig( ImAppPlatform* platform );

static void ImAppPlatformWaylandRegistryGlobalCallback( void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version );
static void ImAppPlatformWaylandRegistryGlobalRemoveCallback( void* data, struct wl_registry* registry, uint32_t name );

//static void ImAppPlatformWaylandShellSurfacePingCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t serial );
//static void ImAppPlatformWaylandShellSurfaceConfigureCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t edges, int32_t width, int32_t height );
//static void ImAppPlatformWaylandShellSurfacePopupDoneCallback( void* data, struct wl_shell_surface* shell_surface );

//static void ImAppPlatformWaylandHandleWindowConfigCallback( void* data, struct wl_callback* callback, uint32_t time );
//static void ImAppPlatformWaylandHandleWindowDrawCallback( void* data, struct wl_callback* callback, uint32_t time );

static void ImAppPlatformWaylandHandlePointerEnter( void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y );
static void ImAppPlatformWaylandHandlePointerLeave( void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface );
static void ImAppPlatformWaylandHandlePointerMotion( void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y );
static void ImAppPlatformWaylandHandlePointerButton( void* data, struct wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state );
static void ImAppPlatformWaylandHandlePointerAxis( void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value );

static void ImAppPlatformWaylandHandleKeyboardKeymap( void* data, struct wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size );
static void ImAppPlatformWaylandHandleKeyboardEnter( void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface, struct wl_array* keys );
static void ImAppPlatformWaylandHandleKeyboardLeave( void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface );
static void ImAppPlatformWaylandHandleKeyboardKey( void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state );
static void ImAppPlatformWaylandHandleKeyboardModifiers( void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group );

static void ImAppPlatformWaylandHandleSeatCapabilities( void* data, struct wl_seat* seat, uint32_t capabilities );

static void ImAppPlatformWaylandHandleXdgPing( void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial );

static void ImAppPlatformWaylandHandleXdgSurfaceConfigure( void* data, struct xdg_surface* xdg_surface, uint32_t serial );

static void ImAppPlatformWaylandHandleXdgToplevelConfigure( void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height, struct wl_array* states );
static void ImAppPlatformWaylandHandleXdgToplevelClose( void* data, struct xdg_toplevel* xdg_toplevel );
static void ImAppPlatformWaylandHandleXdgToplevelConfigureBounds( void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height );
static void ImAppPlatformWaylandHandleXdgToplevelWmCapabilities( void* data, struct xdg_toplevel* xdg_toplevel, struct wl_array* capabilities );

static void ImAppPlatformWaylandHandleXdgDecorationConfigure( void* data, struct zxdg_toplevel_decoration_v1* zxdg_toplevel_decoration_v1, uint32_t mode );

static const struct wl_registry_listener s_wlRegistryListener =
{
	&ImAppPlatformWaylandRegistryGlobalCallback,
	&ImAppPlatformWaylandRegistryGlobalRemoveCallback
};

//static const struct wl_shell_surface_listener s_wlShellSurfaceListener =
//{
//	&ImAppPlatformWaylandShellSurfacePingCallback,
//	&ImAppPlatformWaylandShellSurfaceConfigureCallback,
//	&ImAppPlatformWaylandShellSurfacePopupDoneCallback
//};

//static struct wl_callback_listener s_wlWindowConfigCallbackListener =
//{
//	ImAppPlatformWaylandHandleWindowConfigCallback
//};

//static const struct wl_callback_listener s_wlWindowDrawCallbackListener =
//{
//	&ImAppPlatformWaylandHandleWindowDrawCallback
//};

static const struct wl_pointer_listener s_wlWindowPointerListener =
{
	&ImAppPlatformWaylandHandlePointerEnter,
	&ImAppPlatformWaylandHandlePointerLeave,
	&ImAppPlatformWaylandHandlePointerMotion,
	&ImAppPlatformWaylandHandlePointerButton,
	&ImAppPlatformWaylandHandlePointerAxis
};

static const struct wl_keyboard_listener s_wlKeyboardListener =
{
	&ImAppPlatformWaylandHandleKeyboardKeymap,
	&ImAppPlatformWaylandHandleKeyboardEnter,
	&ImAppPlatformWaylandHandleKeyboardLeave,
	&ImAppPlatformWaylandHandleKeyboardKey,
	&ImAppPlatformWaylandHandleKeyboardModifiers
};

static const struct wl_seat_listener s_wlSeatListener =
{
	&ImAppPlatformWaylandHandleSeatCapabilities
};

static const struct xdg_wm_base_listener s_xdgWmBaseListener =
{
	&ImAppPlatformWaylandHandleXdgPing
};

static const struct xdg_surface_listener s_xdgSurfaceListener =
{
	&ImAppPlatformWaylandHandleXdgSurfaceConfigure
};

static const struct xdg_toplevel_listener s_xdgToplevelListener =
{
	&ImAppPlatformWaylandHandleXdgToplevelConfigure,
	&ImAppPlatformWaylandHandleXdgToplevelClose,
	&ImAppPlatformWaylandHandleXdgToplevelConfigureBounds,
	&ImAppPlatformWaylandHandleXdgToplevelWmCapabilities
};

static const struct zxdg_toplevel_decoration_v1_listener s_xdgDecorationListener =
{
	&ImAppPlatformWaylandHandleXdgDecorationConfigure
};

int main( int argc, char* argv[] )
{
	ImAppPlatform platform = { 0 };

#if IMAPP_ENABLED( IMAPP_DEBUG )
	uintsize maxScanCode = 0u;
#endif
	for( size_t i = 0u; i < ImUiInputKey_MAX; ++i )
	{
		const ImUiInputKey keyValue = (ImUiInputKey)i;

		xkb_keysym_t keySymbol = XKB_KEY_NoSymbol;
		switch( keyValue )
		{
		case ImUiInputKey_None:				keySymbol = XKB_KEY_NoSymbol; break;
		case ImUiInputKey_A:				keySymbol = XKB_KEY_A; break;
		case ImUiInputKey_B:				keySymbol = XKB_KEY_B; break;
		case ImUiInputKey_C:				keySymbol = XKB_KEY_C; break;
		case ImUiInputKey_D:				keySymbol = XKB_KEY_D; break;
		case ImUiInputKey_E:				keySymbol = XKB_KEY_E; break;
		case ImUiInputKey_F:				keySymbol = XKB_KEY_F; break;
		case ImUiInputKey_G:				keySymbol = XKB_KEY_G; break;
		case ImUiInputKey_H:				keySymbol = XKB_KEY_H; break;
		case ImUiInputKey_I:				keySymbol = XKB_KEY_I; break;
		case ImUiInputKey_J:				keySymbol = XKB_KEY_J; break;
		case ImUiInputKey_K:				keySymbol = XKB_KEY_K; break;
		case ImUiInputKey_L:				keySymbol = XKB_KEY_L; break;
		case ImUiInputKey_M:				keySymbol = XKB_KEY_M; break;
		case ImUiInputKey_N:				keySymbol = XKB_KEY_N; break;
		case ImUiInputKey_O:				keySymbol = XKB_KEY_O; break;
		case ImUiInputKey_P:				keySymbol = XKB_KEY_P; break;
		case ImUiInputKey_Q:				keySymbol = XKB_KEY_Q; break;
		case ImUiInputKey_R:				keySymbol = XKB_KEY_R; break;
		case ImUiInputKey_S:				keySymbol = XKB_KEY_S; break;
		case ImUiInputKey_T:				keySymbol = XKB_KEY_T; break;
		case ImUiInputKey_U:				keySymbol = XKB_KEY_U; break;
		case ImUiInputKey_V:				keySymbol = XKB_KEY_V; break;
		case ImUiInputKey_W:				keySymbol = XKB_KEY_W; break;
		case ImUiInputKey_X:				keySymbol = XKB_KEY_X; break;
		case ImUiInputKey_Y:				keySymbol = XKB_KEY_Y; break;
		case ImUiInputKey_Z:				keySymbol = XKB_KEY_Z; break;
		case ImUiInputKey_1:				keySymbol = XKB_KEY_1; break;
		case ImUiInputKey_2:				keySymbol = XKB_KEY_2; break;
		case ImUiInputKey_3:				keySymbol = XKB_KEY_3; break;
		case ImUiInputKey_4:				keySymbol = XKB_KEY_4; break;
		case ImUiInputKey_5:				keySymbol = XKB_KEY_5; break;
		case ImUiInputKey_6:				keySymbol = XKB_KEY_6; break;
		case ImUiInputKey_7:				keySymbol = XKB_KEY_7; break;
		case ImUiInputKey_8:				keySymbol = XKB_KEY_8; break;
		case ImUiInputKey_9:				keySymbol = XKB_KEY_9; break;
		case ImUiInputKey_0:				keySymbol = XKB_KEY_0; break;
		case ImUiInputKey_Enter:			keySymbol = XKB_KEY_Return; break;
		case ImUiInputKey_Escape:			keySymbol = XKB_KEY_Escape; break;
		case ImUiInputKey_Backspace:		keySymbol = XKB_KEY_BackSpace; break;
		case ImUiInputKey_Tab:				keySymbol = XKB_KEY_Tab; break;
		case ImUiInputKey_Space:			keySymbol = XKB_KEY_space; break;
		case ImUiInputKey_LeftShift:		keySymbol = XKB_KEY_Shift_L; break;
		case ImUiInputKey_RightShift:		keySymbol = XKB_KEY_Shift_R; break;
		case ImUiInputKey_LeftControl:		keySymbol = XKB_KEY_Control_L; break;
		case ImUiInputKey_RightControl:		keySymbol = XKB_KEY_Control_R; break;
		case ImUiInputKey_LeftAlt:			keySymbol = XKB_KEY_Alt_L; break;
		case ImUiInputKey_RightAlt:			keySymbol = XKB_KEY_Alt_R; break;
		case ImUiInputKey_Minus:			keySymbol = XKB_KEY_minus; break;
		case ImUiInputKey_Equals:			keySymbol = XKB_KEY_equal; break;
		case ImUiInputKey_LeftBracket:		keySymbol = XKB_KEY_bracketleft; break;
		case ImUiInputKey_RightBracket:		keySymbol = XKB_KEY_bracketright; break;
		case ImUiInputKey_Backslash:		keySymbol = XKB_KEY_backslash; break;
		case ImUiInputKey_Semicolon:		keySymbol = XKB_KEY_semicolon; break;
		case ImUiInputKey_Apostrophe:		keySymbol = XKB_KEY_apostrophe; break;
		case ImUiInputKey_Grave:			keySymbol = XKB_KEY_grave; break;
		case ImUiInputKey_Comma:			keySymbol = XKB_KEY_comma; break;
		case ImUiInputKey_Period:			keySymbol = XKB_KEY_period; break;
		case ImUiInputKey_Slash:			keySymbol = XKB_KEY_slash; break;
		case ImUiInputKey_F1:				keySymbol = XKB_KEY_F1; break;
		case ImUiInputKey_F2:				keySymbol = XKB_KEY_F2; break;
		case ImUiInputKey_F3:				keySymbol = XKB_KEY_F3; break;
		case ImUiInputKey_F4:				keySymbol = XKB_KEY_F4; break;
		case ImUiInputKey_F5:				keySymbol = XKB_KEY_F5; break;
		case ImUiInputKey_F6:				keySymbol = XKB_KEY_F6; break;
		case ImUiInputKey_F7:				keySymbol = XKB_KEY_F7; break;
		case ImUiInputKey_F8:				keySymbol = XKB_KEY_F8; break;
		case ImUiInputKey_F9:				keySymbol = XKB_KEY_F9; break;
		case ImUiInputKey_F10:				keySymbol = XKB_KEY_F10; break;
		case ImUiInputKey_F11:				keySymbol = XKB_KEY_F11; break;
		case ImUiInputKey_F12:				keySymbol = XKB_KEY_F12; break;
		case ImUiInputKey_Print:			keySymbol = XKB_KEY_Print; break;
		case ImUiInputKey_Pause:			keySymbol = XKB_KEY_Pause; break;
		case ImUiInputKey_Insert:			keySymbol = XKB_KEY_Insert; break;
		case ImUiInputKey_Delete:			keySymbol = XKB_KEY_Delete; break;
		case ImUiInputKey_Home:				keySymbol = XKB_KEY_Home; break;
		case ImUiInputKey_End:				keySymbol = XKB_KEY_End; break;
		case ImUiInputKey_PageUp:			keySymbol = XKB_KEY_Page_Up; break;
		case ImUiInputKey_PageDown:			keySymbol = XKB_KEY_Page_Down; break;
		case ImUiInputKey_Up:				keySymbol = XKB_KEY_Up; break;
		case ImUiInputKey_Left:				keySymbol = XKB_KEY_Left; break;
		case ImUiInputKey_Down:				keySymbol = XKB_KEY_Down; break;
		case ImUiInputKey_Right:			keySymbol = XKB_KEY_Right; break;
		case ImUiInputKey_Numpad_Divide:	keySymbol = XKB_KEY_KP_Divide; break;
		case ImUiInputKey_Numpad_Multiply:	keySymbol = XKB_KEY_KP_Multiply; break;
		case ImUiInputKey_Numpad_Minus:		keySymbol = XKB_KEY_KP_Subtract; break;
		case ImUiInputKey_Numpad_Plus:		keySymbol = XKB_KEY_KP_Add; break;
		case ImUiInputKey_Numpad_Enter:		keySymbol = XKB_KEY_KP_Enter; break;
		case ImUiInputKey_Numpad_1:			keySymbol = XKB_KEY_KP_1; break;
		case ImUiInputKey_Numpad_2:			keySymbol = XKB_KEY_KP_2; break;
		case ImUiInputKey_Numpad_3:			keySymbol = XKB_KEY_KP_3; break;
		case ImUiInputKey_Numpad_4:			keySymbol = XKB_KEY_KP_4; break;
		case ImUiInputKey_Numpad_5:			keySymbol = XKB_KEY_KP_5; break;
		case ImUiInputKey_Numpad_6:			keySymbol = XKB_KEY_KP_6; break;
		case ImUiInputKey_Numpad_7:			keySymbol = XKB_KEY_KP_7; break;
		case ImUiInputKey_Numpad_8:			keySymbol = XKB_KEY_KP_8; break;
		case ImUiInputKey_Numpad_9:			keySymbol = XKB_KEY_KP_9; break;
		case ImUiInputKey_Numpad_0:			keySymbol = XKB_KEY_KP_0; break;
		case ImUiInputKey_Numpad_Period:	keySymbol = XKB_KEY_KP_Separator; break;
		case ImUiInputKey_MAX:				break;
		}

#if IMAPP_ENABLED( IMAPP_DEBUG )
		maxScanCode = keySymbol > maxScanCode ? keySymbol : maxScanCode;
		IMAPP_ASSERT( keySymbol < IMAPP_ARRAY_COUNT( platform.inputKeyMapping ) );
#endif

		platform.inputKeyMapping[ keySymbol ] = keyValue;
	}
	IMAPP_ASSERT( maxScanCode + 1u == IMAPP_ARRAY_COUNT( platform.inputKeyMapping ) );

	const int result = ImAppMain( &platform, argc, argv );

	return result;
}

bool ImAppPlatformInitialize( ImAppPlatform* platform, ImUiAllocator* allocator, const char* resourcePath )
{
	platform->allocator = allocator;

	//ImAppPlatformLinuxReadFontConfig( platform );

	platform->wlDisplay = wl_display_connect( NULL );
	if( !platform->wlDisplay )
	{
		IMAPP_DEBUG_LOGE( "Failed to connect to Wayland server." );
		return false;
	}

	platform->wlRegistry = wl_display_get_registry( platform->wlDisplay );
	wl_registry_add_listener( platform->wlRegistry, &s_wlRegistryListener, platform );
	wl_display_roundtrip( platform->wlDisplay );

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

	//wl_display_dispatch( platform->wlDisplay );

	platform->xkbContext = xkb_context_new( XKB_CONTEXT_NO_FLAGS );

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

	platform->resourceBasePath = ImUiMemoryAlloc( platform->allocator, platform->resourceBasePathLength + 1 );
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

//static void ImAppPlatformLinuxReadFontConfig( ImAppPlatform* platform )
//{
//	xmlTextReaderPtr fontConfReader = xmlReaderForFile( "/etc/fonts/fonts.conf", NULL, 0 );
//	if( !fontConfReader )
//	{
//		IMAPP_DEBUG_LOGW( "Failed to read '/etc/fonts/fonts.conf'." );
//		return;
//	}
//
//	while( xmlTextReaderRead( fontConfReader ) == 1 )
//	{
//		const int nodeType = xmlTextReaderNodeType( fontConfReader );
//		const char* nodeName = xmlTextReaderConstName( fontConfReader );
//		if( nodeType != XML_READER_TYPE_ELEMENT ||
//			strcmp( nodeName, "dir" ) != 0 ||
//			xmlTextReaderGetAttribute( fontConfReader, "prefix" ) != NULL )
//		{
//			continue;
//		}
//
//		if( xmlTextReaderRead( fontConfReader ) != 1 ||
//			xmlTextReaderNodeType( fontConfReader ) != XML_READER_TYPE_TEXT )
//		{
//			continue;
//		}
//
//		const char* sourceFontPath = xmlTextReaderConstValue( fontConfReader );
//		if( !sourceFontPath )
//		{
//			continue;
//		}
//		const uintsize fontPathLength = strlen( sourceFontPath );
//
//		if( fontPathLength >= PATH_MAX )
//		{
//			IMAPP_DEBUG_LOGW( "Font path '%s' is too long.", sourceFontPath );
//			continue;
//		}
//
//		if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( platform->allocator, platform->fonts, platform->fontsCapacity, platform->fontsCount + 1 ) )
//		{
//			return;
//		}
//
//		ImAppPlatformLinuxFont* font = &platform->fonts[ platform->fontsCount ];
//		font->pathLength = fontPathLength + 1; // +1 for ending '/'
//
//		char* targetFontPath = ImUiMemoryAlloc( platform->allocator, font->pathLength + 1 ); // +1 for null terminator
//		if( !targetFontPath )
//		{
//			return;
//		}
//
//		memcpy( targetFontPath, sourceFontPath, fontPathLength );
//		targetFontPath[ fontPathLength ] = '/';
//		targetFontPath[ fontPathLength + 1 ] = '\0';
//
//		font->path = targetFontPath;
//
//		platform->fontsCount++;
//	}
//
//	xmlFreeTextReader( fontConfReader );
//}

void ImAppPlatformShutdown( ImAppPlatform* platform )
{
	//for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( platform->systemCursors ); ++i )
	//{
	//	SDL_FreeCursor( platform->systemCursors[ i ] );
	//	platform->systemCursors[ i ] = NULL;
	//}

	ImUiMemoryFree( platform->allocator, platform->resourceBasePath );
	platform->resourceBasePath = NULL;
	platform->resourceBasePathLength = 0;

	//for( uintsize i = 0u; i < platform->fontsCount; ++i )
	//{
	//	ImUiMemoryFree( platform->allocator, platform->fonts[ i ].path );
	//}
	//platform->fontsCount = 0;
	//IMUI_MEMORY_ARRAY_FREE( platform->allocator, platform->fonts, platform->fontsCapacity );

	if( platform->eglDisplay != EGL_NO_DISPLAY )
	{
		eglTerminate( platform->eglDisplay );
		platform->eglDisplay = EGL_NO_DISPLAY;
	}

	if( platform->wlDisplay )
	{
		wl_display_disconnect( platform->wlDisplay );
		platform->wlDisplay = NULL;
	}

	platform->allocator = NULL;
}

sint64 ImAppPlatformTick( ImAppPlatform* platform, sint64 lastTickValue, sint64 tickIntervalMs )
{
	wl_display_dispatch_pending( platform->wlDisplay );

	struct timespec timeSpec;
	clock_gettime( CLOCK_REALTIME, &timeSpec );

	sint64 currentTick		= ((sint64)timeSpec.tv_sec*  1000000000) + timeSpec.tv_nsec;
	const sint64 deltaTicks	= currentTick - lastTickValue;
	sint64 timeToWait		= IMUI_MAX( tickIntervalMs*  1000000, deltaTicks ) - deltaTicks;

	//if( tickIntervalMs == 0 )
	//{
	//	timeToWait = INT_MAX;
	//}

	if( timeToWait > 1 )
	{
		// TODO: use event loop
		usleep( (useconds_t)timeToWait / 1000 );

		clock_gettime( CLOCK_REALTIME, &timeSpec );
		currentTick = (sint64)timeSpec.tv_nsec;
	}

	return currentTick;
}

double ImAppPlatformTicksToSeconds( ImAppPlatform* platform, sint64 tickValue )
{
	return (double)tickValue / 1000000000.0;
}

static void ImAppPlatformWaylandRegistryGlobalCallback( void* data, struct wl_registry* registry, uint32_t name, const char* interface, uint32_t version )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;

	IMAPP_DEBUG_LOGI( "Global registry: %s", interface );

	if( strcmp( interface, "wl_compositor" ) == 0 )
	{
		platform->wlCompositor = (struct wl_compositor*)wl_registry_bind( registry, name, &wl_compositor_interface, 1 );
		platform->wlCursorSurface = wl_compositor_create_surface( platform->wlCompositor );
	}
	else if( strcmp( interface, "wl_subcompositor" ) == 0 )
	{
		platform->wlSubCompositor = (struct wl_subcompositor*)wl_registry_bind( registry, name, &wl_subcompositor_interface, 1 );
	}
	else if( strcmp( interface, "wl_shell" ) == 0 )
	{
		platform->wlShell = (struct wl_shell*)wl_registry_bind( registry, name, &wl_shell_interface, 1 );
	}
	else if( strcmp( interface, "wl_seat" ) == 0 )
	{
		platform->wlSeat = wl_registry_bind( registry, name, &wl_seat_interface, 1 );
		wl_seat_add_listener( platform->wlSeat, &s_wlSeatListener, platform );
	}
	else if( strcmp( interface, "wl_shm" ) == 0 )
	{
		//d->shm = static_cast<wl_shm*>(wl_registry_bind( registry, name, &wl_shm_interface, 1 ));
		//d->cursor_theme = wl_cursor_theme_load( NULL, 32, d->shm );
		//d->default_cursor = wl_cursor_theme_get_cursor( d->cursor_theme, "left_ptr" );
	}
	else if( strcmp( interface, "zxdg_decoration_manager_v1" ) == 0 )
	{
		platform->zxdgDecorationManager = wl_registry_bind( registry, name, &zxdg_decoration_manager_v1_interface, 1 );
		xdg_wm_base_add_listener( platform->xdgWmBase, &s_xdgWmBaseListener, platform );
	}
	else if( strcmp( interface, xdg_wm_base_interface.name ) == 0 )
	{
		platform->xdgWmBase = (struct xdg_wm_base*)wl_registry_bind( registry, name, &xdg_wm_base_interface, IMUI_MIN( version, 2 ) );
	}
}

static void ImAppPlatformWaylandRegistryGlobalRemoveCallback( void* data, struct wl_registry* registry, uint32_t name )
{

}

void ImAppPlatformShowError( ImAppPlatform* pPlatform, const char* message )
{
	//int fd_pipe[ 2 ]; /* fd_pipe[0]: read end of pipe, fd_pipe[1]: write end of pipe* /
	//int zenity_major = 0, zenity_minor = 0, output_len = 0;
	//int argc = 5, i;
	//const char* argv[ 5 + 2 /* icon name* / + 2 /* title* / + 2 /* message* / + 2*  MAX_BUTTONS + 1 /* NULL* / ] = {
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
	//*  This commit removed --icon-name without adding a deprecation notice.
	//*  We need to handle it gracefully, otherwise no message box will be shown.
	//* /
	//argv[ argc++ ] = zenity_major > 3 || (zenity_major == 3 && zenity_minor >= 90) ? "--icon" : "--icon-name";
	//argv[ argc++ ] = "dialog-error";

	//argv[ argc++ ] = "--title=\"I'm app\"";

	//argv[ argc++ ] = "--text";
	//argv[ argc++ ] = message;

	//argv[ argc++ ] = "--extra-button=\"Ok\"";

	//argv[ argc ] = NULL;

	//if( run_zenity( argv, fd_pipe ) == 0 )
	//{
	//	FILE* outputfp = NULL;
	//	char* output = NULL;
	//	char* tmp = NULL;

	//	if( buttonid == NULL )
	//	{
	//		/* if we don't need buttonid, we can return immediately* /
	//		close( fd_pipe[ 0 ] );
	//		return 0; /* success* /
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
	//		return 0; /* User simply closed the dialog* /
	//	}

	//	/* It likes to add a newline...* /
	//	tmp = SDL_strrchr( output, '\n' );
	//	if( tmp != NULL )
	//	{
	//		*tmp = '\0';
	//	}

	//	/* Check which button got pressed* /
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
	//	return 0; /* success!* /
	//}

	//close( fd_pipe[ 0 ] );
	//close( fd_pipe[ 1 ] );
	//return -1; /* run_zenity() calls SDL_SetError(), so message is already set* /

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

	if( !IMUI_MEMORY_ARRAY_CHECK_CAPACITY( platform->allocator, platform->windows, platform->windowsCapacity, platform->windowsCount + 1 ) )
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

	if( platform->xdgWmBase )
	{
		window->xdgSurface = xdg_wm_base_get_xdg_surface( platform->xdgWmBase, window->wlSurface );
		xdg_surface_add_listener( window->xdgSurface, &s_xdgSurfaceListener, window );

		window->xdgToplevel = xdg_surface_get_toplevel( window->xdgSurface );
		xdg_toplevel_add_listener( window->xdgToplevel, &s_xdgToplevelListener, window );

		xdg_toplevel_set_title( window->xdgToplevel, windowTitle );
		xdg_toplevel_set_app_id( window->xdgToplevel, windowTitle );
	}

	window->wlWindow = wl_egl_window_create( window->wlSurface, width, height );
	if( !window->wlWindow )
	{
		IMAPP_DEBUG_LOGE( "Failed to create Wayland Window." );
		ImAppPlatformWindowDestroy( window );
		return NULL;
	}

	if( window->xdgToplevel )
	{
		switch( state )
		{
		case ImAppWindowState_Default:
			break;

		case ImAppWindowState_Minimized:
			xdg_toplevel_set_maximized( window->xdgToplevel );
			break;

		case ImAppWindowState_Maximized:
			xdg_toplevel_set_minimized( window->xdgToplevel );
			break;
		}
	}

	if( platform->xdgWmBase )
	{
		wl_surface_commit( window->wlSurface );
		if( window->xdgSurface )
		{
			while( !window->isInitialized )
			{
				wl_display_flush( platform->wlDisplay );
				wl_display_dispatch( platform->wlDisplay );
			}
		}

		if( style != ImAppWindowStyle_Borderless &&
			window->xdgToplevel &&
			platform->zxdgDecorationManager )
		{
			window->xdgDecoration = zxdg_decoration_manager_v1_get_toplevel_decoration( platform->zxdgDecorationManager, window->xdgToplevel );
			zxdg_toplevel_decoration_v1_add_listener( window->xdgDecoration, &s_xdgDecorationListener, window );
		}

		xdg_surface_set_window_geometry( window->xdgSurface, 0, 0, window->width, window->height );

		if( platform->zxdgDecorationManager && window->xdgDecoration )
		{
			//const enum zxdg_toplevel_decoration_v1_mode mode = bordered ?  : ZXDG_TOPLEVEL_DECORATION_V1_MODE_CLIENT_SIDE;
			zxdg_toplevel_decoration_v1_set_mode( window->xdgDecoration, ZXDG_TOPLEVEL_DECORATION_V1_MODE_SERVER_SIDE );
		}
	}
	//else
	//{
		//window->wlShellSurface = wl_shell_get_shell_surface( platform->wlShell, window->wlSurface );
		//if( !window->wlShellSurface )
		//{
		//	IMAPP_DEBUG_LOGE( "Failed to create Wayland Shell Surface." );
		//	ImAppPlatformWindowDestroy( window );
		//	return NULL;
		//}
		//wl_shell_surface_add_listener( window->wlShellSurface, &s_wlShellSurfaceListener, window );
	//
	//	wl_shell_surface_set_title( window->wlShellSurface, windowTitle );
	//	wl_shell_surface_set_toplevel( window->wlShellSurface );
	//}

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

	//window->sdlWindow = SDL_CreateWindow( windowTitle, SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), SDL_WINDOWPOS_UNDEFINED_DISPLAY( 2 ), width, height, flags );
	//if( window->sdlWindow == NULL )
	//{
	//	SDL_ShowSimpleMessageBox( SDL_MESSAGEBOX_ERROR, "I'm App", "Failed to create Window.", NULL );

	//	ImUiMemoryFree( platform->allocator, window );
	//	return NULL;
	//}

	ImAppEventQueueConstruct( &window->eventQueue, platform->allocator );

	platform->windows[ platform->windowsCount++ ] = window;
	return window;
}

void ImAppPlatformWindowDestroy( ImAppWindow* window )
{
	IMAPP_ASSERT( window->eglContext == EGL_NO_CONTEXT );

	ImAppPlatform* platform = window->platform;

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

	//if( window->wlShellSurface )
	//{
	//	wl_shell_surface_destroy( window->wlShellSurface );
	//	window->wlShellSurface = NULL;
	//}

	if( window->wlSurface )
	{
		wl_surface_destroy( window->wlSurface );
		window->wlSurface = NULL;
	}

	for( uintsize i = 0; i < platform->windowsCount; ++i )
	{
		if( platform->windows[ i ] != window )
		{
			continue;
		}

		IMUI_MEMORY_ARRAY_REMOVE_UNSORTED( platform->windows, platform->windowsCount, i );
		break;
	}

	ImUiMemoryFree( window->allocator, window );
}

//static void ImAppPlatformWaylandShellSurfacePingCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t serial )
//{
//	wl_shell_surface_pong( shell_surface, serial );
//}
//
//static void ImAppPlatformWaylandShellSurfaceConfigureCallback( void* data, struct wl_shell_surface* shell_surface, uint32_t edges, int32_t width, int32_t height )
//{
//	ImAppWindow* window = (ImAppWindow*)data;
//
//	if( window->wlWindow )
//	{
//		wl_egl_window_resize( window->wlWindow, width, height, 0, 0 );
//	}
//
//	window->width = width;
//	window->height = height;
//
//	//if( window->state != ImAppWindowState_Maximized )
//	//{
//	//	window->window_size = window->geometry;
//	//}
//}
//
//static void ImAppPlatformWaylandShellSurfacePopupDoneCallback( void* data, struct wl_shell_surface* shell_surface )
//{
//}

//static void ImAppPlatformWaylandHandleWindowConfigCallback( void* data, struct wl_callback* callback, uint32_t time )
//{
//	ImAppWindow* window = (ImAppWindow*)data;
//
//	wl_callback_destroy( callback );
//
//	//window->configured = 1;
//
//	if( !window->wlDrawCallback )
//	{
//		ImAppPlatformWaylandHandleWindowDrawCallback( data, NULL, time );
//	}
//}

//static void ImAppPlatformWaylandHandleWindowDrawCallback( void* data, struct wl_callback* callback, uint32_t time )
//{
//	//ImAppWindow* window = (ImAppWindow*)data;
//
//	if( callback )
//	{
//		wl_callback_destroy( callback );
//	}
//
//	//struct wl_region* region;
//	//
//	//assert( window->callback == callback );
//	//window->callback = NULL;
//
//
//	//if( !window->configured )
//	//	return;
//
//	//window->drawPtr( window );
//
//	//if( window->opaque || window->fullscreen )
//	//{
//	//	region = wl_compositor_create_region( window->display->compositor );
//	//	wl_region_add( region, 0, 0, window->geometry.width,
//	//					window->geometry.height );
//	//	wl_surface_set_opaque_region( window->surface, region );
//	//	wl_region_destroy( region );
//	//}
//	//else
//	//{
//	//	wl_surface_set_opaque_region( window->surface, NULL );
//	//}
//
//	//window->wlDrawCallback = wl_surface_frame( window->wlSurface );
//	//wl_callback_add_listener( window->wlDrawCallback, &s_wlWindowDrawCallbackListener, window );
//
//	//eglSwapBuffers( window->platform->eglDisplay, window->eglSurface );
//}

static void ImAppPlatformWaylandHandlePointerEnter( void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface, wl_fixed_t surface_x, wl_fixed_t surface_y )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;

	for( uintsize i = 0; i < platform->windowsCount; ++i )
	{
		if( platform->windows[ i ]->wlSurface != surface )
		{
			continue;
		}

		platform->mouseFocusWindow = platform->windows[ i ];
		break;
	}
}

static void ImAppPlatformWaylandHandlePointerLeave( void* data, struct wl_pointer* pointer, uint32_t serial, struct wl_surface* surface )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;

	platform->mouseFocusWindow = NULL;
}

static void ImAppPlatformWaylandHandlePointerMotion( void* data, struct wl_pointer* pointer, uint32_t time, wl_fixed_t x, wl_fixed_t y )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;
	if( !platform->mouseFocusWindow )
	{
		return;
	}

	ImAppEvent mouseEvent;
	mouseEvent.motion.type	= ImAppEventType_Motion;
	mouseEvent.motion.x		= wl_fixed_to_int( x );
	mouseEvent.motion.y		= wl_fixed_to_int( y );

	ImAppEventQueuePush( &platform->mouseFocusWindow->eventQueue, &mouseEvent );
}

static void ImAppPlatformWaylandHandlePointerButton( void* data, struct wl_pointer* pointer, uint32_t serial, uint32_t time, uint32_t button, uint32_t state )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;
	if( !platform->mouseFocusWindow )
	{
		return;
	}

	ImAppEvent mouseEvent;
	mouseEvent.button.type	= state == WL_POINTER_BUTTON_STATE_PRESSED ? ImAppEventType_ButtonDown : ImAppEventType_ButtonUp;
	//mouseEvent.button.x		= x;
	//mouseEvent.button.y		= y;

	switch( button )
	{
	case BTN_LEFT:
		mouseEvent.button.button	= ImUiInputMouseButton_Left;
		break;

	case BTN_MIDDLE:
		mouseEvent.button.button	= ImUiInputMouseButton_Middle;
		break;

	case BTN_RIGHT:
		mouseEvent.button.button	= ImUiInputMouseButton_Right;
		break;

	case BTN_BACK:
		mouseEvent.button.button	= ImUiInputMouseButton_X1;
		break;

	case BTN_FORWARD:
		mouseEvent.button.button	= ImUiInputMouseButton_X2;
		break;
	}

	ImAppEventQueuePush( &platform->mouseFocusWindow->eventQueue, &mouseEvent );
}

static void ImAppPlatformWaylandHandlePointerAxis( void* data, struct wl_pointer* pointer, uint32_t time, uint32_t axis, wl_fixed_t value )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;
	if( !platform->mouseFocusWindow )
	{
		return;
	}

	ImAppEvent mouseEvent;
	mouseEvent.scroll.type	= ImAppEventType_Scroll;
	//mouseEvent.button.x		= x;
	//mouseEvent.button.y		= y;

	if( axis == WL_POINTER_AXIS_VERTICAL_SCROLL )
	{
		mouseEvent.scroll.x = wl_fixed_to_int( value );
	}
	else
	{
		mouseEvent.scroll.x = wl_fixed_to_int( value );
	}

	ImAppEventQueuePush( &platform->mouseFocusWindow->eventQueue, &mouseEvent );
}

static void ImAppPlatformWaylandHandleKeyboardKeymap( void* data, struct wl_keyboard* keyboard, uint32_t format, int32_t fd, uint32_t size )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;

	char* keymap_string = mmap( NULL, size, PROT_READ, MAP_SHARED, fd, 0 );
	xkb_keymap_unref( platform->xkbKeymap );
	platform->xkbKeymap = xkb_keymap_new_from_string( platform->xkbContext, keymap_string, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS );
	munmap( keymap_string, size );
	close( fd );
	xkb_state_unref( platform->xkbState );
	platform->xkbState = xkb_state_new( platform->xkbKeymap );
}

static void ImAppPlatformWaylandHandleKeyboardEnter( void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface, struct wl_array* keys )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;

	for( uintsize i = 0; i < platform->windowsCount; ++i )
	{
		if( platform->windows[ i ]->wlSurface != surface )
		{
			continue;
		}

		platform->keyboardFocusWindow = platform->windows[ i ];
		break;
	}
}

static void ImAppPlatformWaylandHandleKeyboardLeave( void* data, struct wl_keyboard* keyboard, uint32_t serial, struct wl_surface* surface )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;

	platform->keyboardFocusWindow = NULL;
}

static void ImAppPlatformWaylandHandleKeyboardKey( void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t time, uint32_t key, uint32_t state )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;
	if( !platform->keyboardFocusWindow )
	{
		return;
	}

	const xkb_keysym_t keySymbol = xkb_state_key_get_one_sym( platform->xkbState, key + 8 );

	if( state == WL_KEYBOARD_KEY_STATE_PRESSED )
	{
		const uint32_t keyCharacter = xkb_keysym_to_utf32( keySymbol );
		if( keyCharacter >= ' ' )
		{
			ImAppEvent keyboardEvent;
			keyboardEvent.character.type		= ImAppEventType_Character;
			keyboardEvent.character.character	= keyCharacter;

			ImAppEventQueuePush( &platform->keyboardFocusWindow->eventQueue, &keyboardEvent );
		}
	}

	ImAppEvent keyboardEvent;
	keyboardEvent.key.type		= state == WL_KEYBOARD_KEY_STATE_PRESSED ? ImAppEventType_KeyDown : ImAppEventType_KeyUp;
	keyboardEvent.key.key		= platform->inputKeyMapping[ keySymbol ];
	//keyboardEvent.key.repeat	= ...;

	ImAppEventQueuePush( &platform->keyboardFocusWindow->eventQueue, &keyboardEvent );
}

static void ImAppPlatformWaylandHandleKeyboardModifiers( void* data, struct wl_keyboard* keyboard, uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group )
{
	ImAppPlatform* platform = (ImAppPlatform*)data;

	xkb_state_update_mask( platform->xkbState, mods_depressed, mods_latched, mods_locked, 0, 0, group );
}

static void ImAppPlatformWaylandHandleSeatCapabilities( void* data, struct wl_seat* seat, uint32_t capabilities )
{
	if( capabilities & WL_SEAT_CAPABILITY_POINTER )
	{
		struct wl_pointer* pointer = wl_seat_get_pointer( seat );
		wl_pointer_add_listener( pointer, &s_wlWindowPointerListener, data );
	}

	if( capabilities & WL_SEAT_CAPABILITY_KEYBOARD )
	{
		struct wl_keyboard* keyboard = wl_seat_get_keyboard( seat );
		wl_keyboard_add_listener( keyboard, &s_wlKeyboardListener, data );
	}
}

static void ImAppPlatformWaylandHandleXdgPing( void* data, struct xdg_wm_base* xdg_wm_base, uint32_t serial )
{
	xdg_wm_base_pong( xdg_wm_base, serial );
}

static void ImAppPlatformWaylandHandleXdgSurfaceConfigure( void* data, struct xdg_surface* xdg_surface, uint32_t serial )
{
	ImAppWindow* window = (ImAppWindow*)data;

	//Wayland_HandleResize( window, window->w, window->h, wind->scale_factor );
	xdg_surface_ack_configure( xdg_surface, serial );

	window->isInitialized = true;

}

static void ImAppPlatformWaylandHandleXdgToplevelConfigure( void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height, struct wl_array* states )
{
}

static void ImAppPlatformWaylandHandleXdgToplevelClose( void* data, struct xdg_toplevel* xdg_toplevel )
{
}

static void ImAppPlatformWaylandHandleXdgToplevelConfigureBounds( void* data, struct xdg_toplevel* xdg_toplevel, int32_t width, int32_t height )
{
}

static void ImAppPlatformWaylandHandleXdgToplevelWmCapabilities( void* data, struct xdg_toplevel* xdg_toplevel, struct wl_array* capabilities )
{
}

static void ImAppPlatformWaylandHandleXdgDecorationConfigure( void* data, struct zxdg_toplevel_decoration_v1* zxdg_toplevel_decoration_v1, uint32_t mode )
{
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
//				for( const char* pText = textInputEvent->text;* pText != '\0'; ++pText )
//				{
//					const ImAppEvent charEvent = { .character = { .type = ImAppEventType_Character, .character =* pText } };
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
	return window->platform->mouseFocusWindow == window ||
		window->platform->keyboardFocusWindow == window;
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
	char fontMatchCmd[ PATH_MAX ];
	snprintf( fontMatchCmd, sizeof( fontMatchCmd ), "fc-match -f \"%%{file}\" %s", fontName );

	FILE* file = NULL;
	char fontPath[ PATH_MAX ];

	FILE* fontMatchProc = popen( fontMatchCmd, "r" );
	if( !fontMatchProc )
	{
		ImAppTrace( "Error: Failed to start '%s'\n", fontMatchCmd );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	uintsize read = 0;
	uintsize readTotal = 0;
	do
	{
		read = fread( fontPath + readTotal, 1, sizeof( fontPath ) - readTotal, fontMatchProc );
		readTotal += read;
	}
	while( read > 0 || readTotal == sizeof( fontPath ) );

	pclose( fontMatchProc );

	file = fopen( fontPath, "rb" );

	//const size_t fontNameLength = strlen( fontName );
	//for( uintsize fontIndex = 0; fontIndex < platform->fontsCount; ++fontIndex )
	//{
	//	const ImAppPlatformLinuxFont* font = &platform->fonts[ fontIndex ];

	//	if( font->pathLength + fontNameLength + 1 >= PATH_MAX )
	//	{
	//		continue;
	//	}

	//	memcpy( fontPath, font->path, font->pathLength );
	//	strncpy( fontPath + font->pathLength, fontName, PATH_MAX - font->pathLength );

	//	file = fopen( fontPath, "rb" );
	//	if( file )
	//	{
	//		break;
	//	}
	//}

	if( !file )
	{
		ImAppTrace( "Error: Failed to open '%s'\n", fontPath );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	struct stat fileStats;
	if( fstat( fileno( file ), &fileStats ) != 0 )
	{
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

	void* memory = ImUiMemoryAlloc( platform->allocator, (size_t)fileStats.st_size );

	const size_t readResult = fread( memory, 1u, (size_t)fileStats.st_size, file );
	fclose( file );

	if( readResult != fileStats.st_size )
	{
		ImUiMemoryFree( platform->allocator, memory );
		const ImAppBlob result = { NULL, 0u };
		return result;
	}

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
