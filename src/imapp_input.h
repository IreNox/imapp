#pragma once

struct ImAppInput;
typedef struct ImAppInput ImAppInput;

struct nk_context;

union SDL_Event;
typedef union SDL_Event SDL_Event;

struct SDL_Window;
typedef struct SDL_Window SDL_Window;

ImAppInput*			ImAppInputCreate( SDL_Window* pSdlWindow, struct nk_context* pNkContext );
void				ImAppInputDestroy( ImAppInput* pInput );

void				ImAppInputBegin( ImAppInput* pInput );
void				ImAppInputHandleEvent( ImAppInput* pInput, const SDL_Event* pSdlEvent );
void				ImAppInputEnd( ImAppInput* pInput );
