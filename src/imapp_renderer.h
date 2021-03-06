#pragma once

#define IMAPP_RENDERER_OPENGL TIKI_ON
//#define IMAPP_RENDERER_VULKAN TIKI_ON

#if !defined( IMAPP_RENDERER_OPENGL )
#	define IMAPP_RENDERER_OPENGL		TIKI_OFF
#endif

#if !defined( IMAPP_RENDERER_VULKAN )
#	define IMAPP_RENDERER_VULKAN		TIKI_OFF
#endif

#if IMAPP_DISABLED( IMAPP_RENDERER_OPENGL ) && IMAPP_DISABLED( IMAPP_RENDERER_VULKAN )
#	error No Graphics-API enabled
#endif

#if IMAPP_ENABLED( IMAPP_RENDERER_OPENGL ) && IMAPP_ENABLED( IMAPP_RENDERER_VULKAN )
#	error Only one Graphics-API can be enabled at the same time
#endif

struct ImAppWindow;
typedef struct ImAppWindow ImAppWindow;

struct ImAppRenderer;
typedef struct ImAppRenderer ImAppRenderer;

struct ImAppRendererTexture;
typedef struct ImAppRendererTexture ImAppRendererTexture;

struct nk_context;
struct nk_font;

ImAppRenderer*			ImAppRendererCreate( ImAppAllocator* pAllocator, ImAppPlatform* pPlatform, ImAppWindow* pWindow );
void					ImAppRendererDestroy( ImAppRenderer* pRenderer );

bool					ImAppRendererRecreateResources( ImAppRenderer* pRenderer );

struct nk_font*			ImAppRendererCreateDefaultFont( ImAppRenderer* pRenderer, struct nk_context* pNkContext );

ImAppRendererTexture*	ImAppRendererTextureCreateFromFile( ImAppRenderer* pRenderer, const void* pFilename );
ImAppRendererTexture*	ImAppRendererTextureCreateFromMemory( ImAppRenderer* pRenderer, const void* pData, int width, int height );
void					ImAppRendererTextureDestroy( ImAppRenderer* pRenderer, ImAppRendererTexture* pTexture );

void					ImAppRendererDraw( ImAppRenderer* pRenderer, struct nk_context* pNkContext, int width, int height );
