#pragma once

#include "nuklear.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

struct ImAppInput;
struct ImAppRenderer;
struct SDL_Window;
struct nk_context;

struct ImAppParameters
{
	int						tickIntervalMs;		// Tick interval. Use 0 to disable. Default: 0

	// Only for windowed Platforms:
	const char*				pWindowTitle;		// Default: "I'm App"
	int						windowWidth;		// Default: 1280
	int						windowHeight;		// Default: 720
};

struct ImAppContext
{
	struct ImAppInput*		pInput;
	struct ImAppRenderer*	pRenderer;

	struct SDL_Window*		pSdlWindow;
	struct nk_context*		pNkContext;
};


//////////////////////////////////////////////////////////////////////////
// These function must be implemented to create a ImApp Application:

// Called at startup to initialize Program and create Context. To customize the App change values in pParameters. Must return a Program Context. Return nullptr to signal an Error.
void*						ImAppProgramInitialize( struct ImAppParameters* pParameters );

// Called for event and every tick
void						ImAppProgramTick( struct ImAppContext* pImAppContext, void* pProgramContext );

// Called after the tick to build the UI
void						ImAppProgramDoUi( struct ImAppContext* pImAppContext, void* pProgramContext );

// Called before shutdown. Free the Program Context here.
void						ImAppProgramShutdown( struct ImAppContext* pImAppContext, void* pProgramContext );


//////////////////////////////////////////////////////////////////////////
// Image

struct nk_image				ImAppImageLoad( struct ImAppContext* pImAppContext, const void* pImageData, size_t imageDataSize );
struct nk_image				ImAppImageFree( struct ImAppContext* pImAppContext, struct nk_image image );

#ifdef __cplusplus
}
#endif
