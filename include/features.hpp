#pragma once
#include "sdk.hpp"

void DoLocalPlayerDetection();
void ResetLocalPlayerDetection();
void DrawGameInfoOverlay(ImDrawList* dl);
void DrawEnemyHpBar(ImDrawList* dl, const EntityInfo& e);