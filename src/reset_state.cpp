#include "reset_state.h"
#include "esp_smooth.h"
#include "gametracker.h"
#include "ulttracker.h"
#include "outline.h"
#include "offsets.hpp"

void ResetReaderRuntime()
{
	if (SDK)
		SDK->dwGameBase = (uint64_t)GetModuleHandleW(nullptr);

	ResetEntityListResolver();
	ResetMemoryAuxCaches();
	ResetLocalPlayerDetection();
	ResetEspSmoothCaches();
	ResetRenderCaches();
	ResetHeroTrackerState();
	ResetAbilityTrackerState();
	ResetOutlineCaches();

	g_entities.clear();
	g_hpPacks.clear();
	g_debug = {};
	memset(&g_worldInfo, 0, sizeof(g_worldInfo));
	g_vmCameraPtr = 0;
	memset(&g_viewMatrix, 0, sizeof(g_viewMatrix));

	ReadEntities();
	DoLocalPlayerDetection();
	ReadViewMatrix();
	UpdateScreenPositions();
}
