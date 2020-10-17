#include "SDL.h"

int main()
{
	bool running = true;
	while (running)
	{
		SDL_Event sdlEvent;
		while (SDL_PollEvent(&sdlEvent))
		{
			// todo
		}
	}

	return 0;
}