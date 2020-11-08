#include "imapp/imapp.h"

#include "imapp_private.h"

void ImAppQuit( ImAppContext* pImAppContext )
{
	ImApp* pImApp = (ImApp*)pImAppContext;

	pImApp->running = false;
}
