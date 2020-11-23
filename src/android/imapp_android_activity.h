#pragma once

struct ANativeWindow;
typedef struct ANativeWindow ANativeWindow;

struct ImAppWindow
{
	ANativeWindow*	pNativeWindow;

	bool			isRunning;
};

ImAppWindow*	AcquireNativeWindow();
void			ReleaseNativeWindow( ImAppWindow* pWindow );
