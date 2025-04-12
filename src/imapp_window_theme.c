#include "imapp_window_theme.h"

static ImAppWindowTheme s_windowTheme;

typedef struct ImAppWindowThemeState
{
	ImUiPos		clickPosition;
	//ImUiPos		lastPosition;
} ImAppWindowThemeState;

static const ImUiToolboxThemeReflectionField s_windowThemeReflectionFields[] =
{
	{ "Title/Active/Skin",									ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, titleActive.skin ) },
	{ "Title/Active/Text Color",							ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleActive.textColor ) },
	{ "Title/Active/Background Color",						ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleActive.backgroundColor ) },

	{ "Title/Inactive/Skin",								ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, titleInactive.skin ) },
	{ "Title/Inactive/Text Color",							ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleInactive.textColor ) },
	{ "Title/Inactive/Background Color",					ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleInactive.backgroundColor ) },

	{ "Title/Buttons/Minimize/Skin",						ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, titleMinimizeButton.skin ) },
	{ "Title/Buttons/Minimize/Size",						ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleMinimizeButton.size ) },
	{ "Title/Buttons/Minimize/Background Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.backgroundColor ) },
	{ "Title/Buttons/Minimize/Background Inactive Color",	ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.backgroundColorInactive ) },
	{ "Title/Buttons/Minimize/Background Hover Color",		ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.backgroundColorHover ) },
	{ "Title/Buttons/Minimize/Background Clicked Color",	ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.backgroundColorClicked ) },
	{ "Title/Buttons/Minimize/Icon/Image",					ImUiToolboxThemeReflectionType_Image,	offsetof( ImAppWindowTheme, titleMinimizeButton.icon ) },
	{ "Title/Buttons/Minimize/Icon/Size",					ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleMinimizeButton.iconSize ) },
	{ "Title/Buttons/Minimize/Icon/Color",					ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.iconColor ) },
	{ "Title/Buttons/Minimize/Icon/Inactive Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.iconColorInactive ) },
	{ "Title/Buttons/Minimize/Icon/Hover Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.iconColorHover ) },
	{ "Title/Buttons/Minimize/Icon/Clicked Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMinimizeButton.iconColorClicked ) },

	{ "Title/Buttons/Restore/Skin",							ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, titleRestoreButton.skin ) },
	{ "Title/Buttons/Restore/Size",							ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleRestoreButton.size ) },
	{ "Title/Buttons/Restore/Background Color",				ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.backgroundColor ) },
	{ "Title/Buttons/Restore/Background Inactive Color",	ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.backgroundColorInactive ) },
	{ "Title/Buttons/Restore/Background Hover Color",		ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.backgroundColorHover ) },
	{ "Title/Buttons/Restore/Background Clicked Color",		ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.backgroundColorClicked ) },
	{ "Title/Buttons/Restore/Icon/Image",					ImUiToolboxThemeReflectionType_Image,	offsetof( ImAppWindowTheme, titleRestoreButton.icon ) },
	{ "Title/Buttons/Restore/Icon/Size",					ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleRestoreButton.iconSize ) },
	{ "Title/Buttons/Restore/Icon/Color",					ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.iconColor ) },
	{ "Title/Buttons/Restore/Icon/Inactive Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.iconColorInactive ) },
	{ "Title/Buttons/Restore/Icon/Hover Color",				ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.iconColorHover ) },
	{ "Title/Buttons/Restore/Icon/Clicked Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleRestoreButton.iconColorClicked ) },

	{ "Title/Buttons/Maximize/Skin",						ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, titleMaximizeButton.skin ) },
	{ "Title/Buttons/Maximize/Size",						ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleMaximizeButton.size ) },
	{ "Title/Buttons/Maximize/Background Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.backgroundColor ) },
	{ "Title/Buttons/Maximize/Background Inactive Color",	ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.backgroundColorInactive ) },
	{ "Title/Buttons/Maximize/Background Hover Color",		ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.backgroundColorHover ) },
	{ "Title/Buttons/Maximize/Background Clicked Color",	ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.backgroundColorClicked ) },
	{ "Title/Buttons/Maximize/Icon/Image",					ImUiToolboxThemeReflectionType_Image,	offsetof( ImAppWindowTheme, titleMaximizeButton.icon ) },
	{ "Title/Buttons/Maximize/Icon/Size",					ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleMaximizeButton.iconSize ) },
	{ "Title/Buttons/Maximize/Icon/Color",					ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.iconColor ) },
	{ "Title/Buttons/Maximize/Icon/Inactive Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.iconColorInactive ) },
	{ "Title/Buttons/Maximize/Icon/Hover Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.iconColorHover ) },
	{ "Title/Buttons/Maximize/Icon/Clicked Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleMaximizeButton.iconColorClicked ) },

	{ "Title/Buttons/Close/Skin",							ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, titleCloseButton.skin ) },
	{ "Title/Buttons/Close/Size",							ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleCloseButton.size ) },
	{ "Title/Buttons/Close/Background Color",				ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.backgroundColor ) },
	{ "Title/Buttons/Close/Background Inactive Color",		ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.backgroundColorInactive ) },
	{ "Title/Buttons/Close/Background Hover Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.backgroundColorHover ) },
	{ "Title/Buttons/Close/Background Clicked Color",		ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.backgroundColorClicked ) },
	{ "Title/Buttons/Close/Icon/Image",						ImUiToolboxThemeReflectionType_Image,	offsetof( ImAppWindowTheme, titleCloseButton.icon ) },
	{ "Title/Buttons/Close/Icon/Size",						ImUiToolboxThemeReflectionType_Size,	offsetof( ImAppWindowTheme, titleCloseButton.iconSize ) },
	{ "Title/Buttons/Close/Icon/Color",						ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.iconColor ) },
	{ "Title/Buttons/Close/Icon/Inactive Color",			ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.iconColorInactive ) },
	{ "Title/Buttons/Close/Icon/Hover Color",				ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.iconColorHover ) },
	{ "Title/Buttons/Close/Icon/Clicked Color",				ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, titleCloseButton.iconColorClicked ) },

	{ "Title/Padding",										ImUiToolboxThemeReflectionType_Border,	offsetof( ImAppWindowTheme, titlePadding ) },
	{ "Title/Height",										ImUiToolboxThemeReflectionType_Float,	offsetof( ImAppWindowTheme, titleHeight ) },

	{ "Body/Active/Skin",									ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, bodyActive.skin ) },
	{ "Body/Active/Color",									ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, bodyActive.color ) },

	{ "Body/Inactive/Skin",									ImUiToolboxThemeReflectionType_Skin,	offsetof( ImAppWindowTheme, bodyInactive.skin ) },
	{ "Body/Inactive/Color",								ImUiToolboxThemeReflectionType_Color,	offsetof( ImAppWindowTheme, bodyInactive.color ) },

	{ "Body/Padding",										ImUiToolboxThemeReflectionType_Border,	offsetof( ImAppWindowTheme, bodyPadding ) },
};
static_assert(sizeof( ImAppWindowTheme ) == 776u, "window theme changed");

ImUiToolboxThemeReflection ImAppWindowThemeReflectionGet()
{
	ImUiToolboxThemeReflection reflection;
	reflection.fields	= s_windowThemeReflectionFields;
	reflection.count	= IMAPP_ARRAY_COUNT( s_windowThemeReflectionFields );

	return reflection;
}

ImAppWindowTheme* ImAppWindowThemeGet()
{
	return &s_windowTheme;
}

void ImAppWindowThemeSet( const ImAppWindowTheme* windowTheme )
{
	s_windowTheme = *windowTheme;
}

void ImAppWindowThemeFillDefault( ImAppWindowTheme* windowTheme )
{
	const ImUiSkin skin = { IMUI_TEXTURE_HANDLE_INVALID };

	const ImUiColor brightColor					= ImUiColorCreateGray( 192u );
	const ImUiColor mediumColor					= ImUiColorCreateGray( 128u );
	const ImUiColor lowColor					= ImUiColorCreateGray( 96u );

	windowTheme->titleActive.skin				= skin;
	windowTheme->titleActive.textColor			= ImUiColorCreateBlack();
	windowTheme->titleActive.backgroundColor	= brightColor;
	windowTheme->titleInactive.skin				= skin;
	windowTheme->titleInactive.textColor		= ImUiColorCreateBlack();
	windowTheme->titleInactive.backgroundColor	= mediumColor;

	const ImUiImage image = { IMUI_TEXTURE_HANDLE_INVALID };

	ImAppWindowThemeTitleButton* buttons[] = {
		&windowTheme->titleMinimizeButton,
		&windowTheme->titleRestoreButton,
		&windowTheme->titleMaximizeButton,
		&windowTheme->titleCloseButton
	};

	for( uintsize i = 0u; i < IMAPP_ARRAY_COUNT( buttons ); ++i )
	{
		ImAppWindowThemeTitleButton* button = buttons[ i ];

		button->skin					= skin;
		button->size					= ImUiSizeCreateAll( 30.0f );
		button->icon					= image;
		button->iconSize				= ImUiSizeCreateZero();
		button->iconColor				= ImUiColorCreateBlack();
		button->iconColorInactive		= ImUiColorCreateBlack();
		button->iconColorHover			= ImUiColorCreateBlack();
		button->iconColorClicked		= ImUiColorCreateBlack();
		button->backgroundColor			= brightColor;
		button->backgroundColorInactive	= mediumColor;
		button->backgroundColorHover	= mediumColor;
		button->backgroundColorClicked	= lowColor;
	}

	windowTheme->titlePadding									= ImUiBorderCreate( 0.0f, 5.0f, 0.0f, 0.0f );
	windowTheme->titleHeight									= 30.0f;

	windowTheme->bodyActive.skin								= skin;
	windowTheme->bodyActive.color								= ImUiColorCreateWhite();
	windowTheme->bodyInactive.skin								= skin;
	windowTheme->bodyInactive.color								= brightColor;

	windowTheme->bodyPadding									= ImUiBorderCreateAll( 5.0f );
}

bool ImAppWindowThemeTitleButtonUi( ImUiWindow* window, const ImAppWindowThemeTitleButton* buttonTheme, bool hasFocus, const char* alternativeText )
{
	ImUiWidget* button = ImUiWidgetBegin( window );
	ImUiWidgetSetFixedSize( button, buttonTheme->size );

	ImUiWidgetInputState inputState;
	ImUiWidgetGetInputState( button, &inputState );

	ImUiColor iconColor = hasFocus ? buttonTheme->iconColor : buttonTheme->iconColorInactive;
	ImUiColor backgroundColor = hasFocus ? buttonTheme->backgroundColor : buttonTheme->backgroundColorInactive;
	if( inputState.wasPressed && inputState.isMouseDown )
	{
		iconColor = buttonTheme->iconColorClicked;
		backgroundColor = buttonTheme->backgroundColorClicked;
	}
	else if( inputState.isMouseOver )
	{
		iconColor = buttonTheme->iconColorHover;
		backgroundColor = buttonTheme->backgroundColorHover;
	}

	ImUiWidgetDrawSkin( button, &buttonTheme->skin, backgroundColor );

	if( buttonTheme->icon.textureHandle )
	{
		ImUiWidget* buttonIcon = ImUiWidgetBegin( window );
		ImUiWidgetSetAlign( buttonIcon, 0.5f, 0.5f );
		ImUiWidgetSetFixedSize( buttonIcon, buttonTheme->iconSize );

		ImUiWidgetDrawImageColor( buttonIcon, &buttonTheme->icon, iconColor );

		ImUiWidgetEnd( buttonIcon );
	}
	else
	{
		ImUiWidget* buttonText = ImUiToolboxLabelBeginColor( window, alternativeText, iconColor );
		ImUiWidgetSetHAlign( buttonText, 0.5f );

		ImUiWidgetEnd( buttonText );
	}

	ImUiWidgetEnd( button );

	return inputState.wasPressed && inputState.hasMouseReleased;
}

ImUiRect ImAppWindowThemeDoUi( ImAppWindow* appWindow, ImUiSurface* surface )
{
	const bool hasFocus = ImAppWindowHasFocus( appWindow );
	const bool isMaximized = ImAppPlatformWindowGetState( appWindow ) == ImAppWindowState_Maximized;

	ImUiWindow* window = ImUiWindowBegin( surface, "window", ImUiSurfaceGetRect( surface ), 1 );

	ImUiWidget* root = ImUiWidgetBegin( window );
	ImUiWidgetSetStretchOne( root );
	ImUiWidgetSetLayoutVertical( root );

	// title bar
	int buttonsX;
	{
		ImUiWidget* title = ImUiWidgetBegin( window );
		ImUiWidgetSetHStretch( title, 1.0f );
		ImUiWidgetSetLayoutHorizontal( title );
		ImUiWidgetSetFixedHeight( title, s_windowTheme.titleHeight );
		ImUiWidgetSetPadding( title, s_windowTheme.titlePadding );

		const ImAppWindowThemeTitle* titleTheme = hasFocus ? &s_windowTheme.titleActive : &s_windowTheme.titleInactive;
		ImUiWidgetDrawSkin( title, &titleTheme->skin, titleTheme->backgroundColor );

		ImAppWindowThemeState* state = (ImAppWindowThemeState*)ImUiWidgetAllocState( title, sizeof( ImAppWindowThemeState ), 0u );

		ImUiWidgetInputState inputState;
		ImUiWidgetGetInputState( title, &inputState );

		//if( inputState.hasMousePressed )
		//{
		//	state->clickPosition = inputState.relativeMousePos;
		//}

		//if( inputState.wasPressed )
		//{
		//	const ImUiPos mouseMove = ImUiPosSubPos( inputState.relativeMousePos, state->clickPosition );
		//	if( mouseMove.x != 0.0f || mouseMove.y != 0.0f )
		//	{
		//		int x;
		//		int y;
		//		ImAppWindowGetPosition( appWindow, &x, &y );
		//		x += (int)mouseMove.x;
		//		y += (int)mouseMove.y;

		//		ImAppWindowSetPosition( appWindow, x, y );
		//	}
		//}

		if( inputState.isMouseOver &&
			ImUiInputHasMouseButtonDoubleClicked( ImUiWidgetGetContext( title ), ImUiInputMouseButton_Left ) )
		{
			ImAppPlatformWindowSetState( appWindow, isMaximized ? ImAppWindowState_Default : ImAppWindowState_Maximized );
		}

		// title text
		{
			const char* title = ImAppPlatformWindowGetTitle( appWindow );

			ImUiWidget* titleText = ImUiToolboxLabelBeginColor( window, title, titleTheme->textColor );
			ImUiWidgetSetVAlign( titleText, 0.5f );
			ImUiToolboxLabelEnd( titleText );
		}

		ImUiWidget* strecher = ImUiWidgetBegin( window );
		ImUiWidgetSetHStretch( strecher, 1.0f );
		{
			const ImUiRect strecherRect = ImUiWidgetGetRect( strecher );
			buttonsX = (int)(strecherRect.pos.x + strecherRect.size.width);
		}
		ImUiWidgetEnd( strecher );

		// minimize button
		{
			const ImAppWindowThemeTitleButton* buttonTheme = &s_windowTheme.titleMinimizeButton;
			if( ImAppWindowThemeTitleButtonUi( window, buttonTheme, hasFocus, "_") )
			{
				ImAppPlatformWindowSetState( appWindow, ImAppWindowState_Minimized );
			}
		}

		// maximize/restore button
		{
			const ImAppWindowThemeTitleButton* buttonTheme = isMaximized ? &s_windowTheme.titleRestoreButton : &s_windowTheme.titleMaximizeButton;
			const char* alternativeText = isMaximized ? "[]" : "^";
			if( ImAppWindowThemeTitleButtonUi( window, buttonTheme, hasFocus, alternativeText ) )
			{
				ImAppPlatformWindowSetState( appWindow, isMaximized ? ImAppWindowState_Default : ImAppWindowState_Maximized );
			}
		}

		// close button
		{
			const ImAppWindowThemeTitleButton* buttonTheme = &s_windowTheme.titleCloseButton;
			if( ImAppWindowThemeTitleButtonUi( window, buttonTheme, hasFocus, "X" ) )
			{
				ImAppPlatformWindowClose( appWindow );
			}
		}

		ImUiWidgetEnd( title );
	}

	// body
	ImUiRect result;
	{
		ImUiWidget* body = ImUiWidgetBegin( window );
		ImUiWidgetSetStretchOne( body );
		ImUiWidgetSetPadding( body, s_windowTheme.bodyPadding );

		const ImAppWindowThemeBody* bodyTheme = hasFocus ? &s_windowTheme.bodyActive : &s_windowTheme.bodyInactive;
		ImUiWidgetDrawSkin( body, &bodyTheme->skin, bodyTheme->color );

		result = ImUiRectShrinkBorder( ImUiWidgetGetRect( body ), s_windowTheme.bodyPadding );

		ImUiWidgetEnd( body );
	}

	ImUiWidgetEnd( root );
	ImUiWindowEnd( window );

	ImAppPlatformWindowSetTitleBounds( appWindow, (int)ceilf( s_windowTheme.titleHeight ), buttonsX );

	return result;
}
