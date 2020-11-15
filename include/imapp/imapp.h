#pragma once

#include <stdbool.h>
#include <stddef.h>

#if defined(__ANDROID__)
#	define NK_SIZE_TYPE size_t
#	define NK_POINTER_TYPE size_t
#endif

#define NK_BUTTON_TRIGGER_ON_RELEASE
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_INCLUDE_DEFAULT_FONT
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_VERTEX_BUFFER_OUTPUT
#include "nuklear.h"

#ifdef __cplusplus
extern "C"
{
#endif

struct nk_context;

typedef void*(*ImAppAllocatorMalloc)( size_t size );
typedef void*(*ImAppAllocatorFree)(size_t size);

struct ImAppAllocator
{
	ImAppAllocatorMalloc	pMalloc;
	ImAppAllocatorFree		pFree;
	void*					pUserData;
};
typedef struct ImAppAllocator ImAppAllocator;

struct ImAppParameters
{
	int						argc;
	char**					argv;

	ImAppAllocator			allocator;			// Override memory Allocator. Default use malloc/free

	int						tickIntervalMs;		// Tick interval. Use 0 to disable. Default: 0
	bool					defaultFullWindow;	// Opens a default Window over the full size. Default: true

	// Only for windowed Platforms:
	const char*				pWindowTitle;		// Default: "I'm App"
	int						windowWidth;		// Default: 1280
	int						windowHeight;		// Default: 720
};
typedef struct ImAppParameters ImAppParameters;

struct ImAppContext
{
	struct nk_context*		pNkContext;

	int						width;
	int						height;
};
typedef struct ImAppContext ImAppContext;

//////////////////////////////////////////////////////////////////////////
// These function must be implemented to create a ImApp Program:

// Called at startup to initialize Program and create Context. To customize the App change values in pParameters. Must return a Program Context. Return NULL to signal an Error.
void*						ImAppProgramInitialize( ImAppParameters* pParameters );

// Called for every tick to build the UI
void						ImAppProgramDoUi( ImAppContext* pImAppContext, void* pProgramContext );

// Called before shutdown. Free the Program Context here.
void						ImAppProgramShutdown( ImAppContext* pImAppContext, void* pProgramContext );

//////////////////////////////////////////////////////////////////////////
// Control

void						ImAppQuit( ImAppContext* pImAppContext );

//////////////////////////////////////////////////////////////////////////
// Image

struct nk_image				ImAppImageLoadResource( ImAppContext* pImAppContext, const char* pResourceName );
struct nk_image				ImAppImageLoadFromMemory( ImAppContext* pImAppContext, const void* pImageData, size_t imageDataSize );
void						ImAppImageFree( ImAppContext* pImAppContext, struct nk_image image );

#ifdef __cplusplus
}
#endif
