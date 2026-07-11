#pragma once
#include "sdk.hpp"
#include "heroicons.h"
#include "imgui.h"
#include <cmath>

inline float GetLocalYaw()
{
	for (auto& e : g_entities) {
		if (e.isLocal) return e.rot_y;
	}
	const float* m = g_viewMatrix.data;
	float fx = m[8], fz = m[10];
	if (fx != 0.f || fz != 0.f)
		return atan2f(fx, fz);
	return 0.f;
}

inline void DrawRadar(ImDrawList* dl)
{
	if (!g_drawRadar) return;

	const float sz    = g_radarSize;
	const float range = g_radarRange;
	const float pad   = 10.f;
	const float x0    = g_screenW - pad - sz;
	const float y0    = pad;
	const float cx    = x0 + sz * 0.5f;
	const float cy    = y0 + sz * 0.5f;
	const float half  = sz * 0.5f - 3.f;
	const float yaw   = GetLocalYaw();
	const float sinY  = sinf(-yaw);
	const float cosY  = cosf(-yaw);

	dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x0 + sz, y0 + sz), IM_COL32(8, 8, 10, 75), 2.f);
	dl->AddRect(ImVec2(x0, y0), ImVec2(x0 + sz, y0 + sz), IM_COL32(255, 255, 255, 35), 2.f);
	dl->AddLine(ImVec2(cx - half, cy), ImVec2(cx + half, cy), IM_COL32(255, 255, 255, 25), 1.f);
	dl->AddLine(ImVec2(cx, cy - half), ImVec2(cx, cy + half), IM_COL32(255, 255, 255, 25), 1.f);
	dl->AddRectFilled(ImVec2(cx - 1.5f, cy - 1.5f), ImVec2(cx + 1.5f, cy + 1.5f), IM_COL32(220, 220, 220, 200));

	for (auto& e : g_entities) {
		if (e.isLocal) continue;
		if (e.health_max > 1.f && e.health <= 0.f) continue;

		bool enemy = IsEnemyEntity(e);
		if (!enemy && !g_radarShowAllies) continue;

		float3 ref = (e.boneCount > 0) ? e.bones[0] : e.pos;
		float dx = ref.x - g_cameraPos.x;
		float dz = ref.z - g_cameraPos.z;
		float rx = dx * cosY - dz * sinY;
		float ry = dx * sinY + dz * cosY;

		float dist = sqrtf(rx * rx + ry * ry);
		if (dist > range) continue;

		float px = cx + (rx / range) * half;
		float py = cy + (ry / range) * half;

		px = fmaxf(x0 + 3.f, fminf(x0 + sz - 3.f, px));
		py = fmaxf(y0 + 3.f, fminf(y0 + sz - 3.f, py));

		ImU32 col = enemy ? IM_COL32(220, 70, 70, 220) : IM_COL32(160, 160, 160, 180);
		if (g_lowHpPulse && enemy) {
			float maxHp = e.health_max > 1.f ? e.health_max : 100.f;
			if (e.health / maxHp <= 0.35f)
				col = IM_COL32(255, 90, 90, 240);
		}

		dl->AddRectFilled(ImVec2(px - 2.f, py - 2.f), ImVec2(px + 2.f, py + 2.f), col);
	}
}
