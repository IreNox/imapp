#include "imapp/imapp.h"

#include "imapp_private.h"

void ImAppQuit( ImAppContext* imAppContext )
{
	ImApp* pImApp = (ImApp*)imAppContext;

	pImApp->running = false;
}
