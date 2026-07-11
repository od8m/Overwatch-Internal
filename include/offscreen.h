#pragma once
#include "sdk.hpp"
#include "memory.hpp"
#include "imgui.h"
#include <cmath>

inline float GetFovRadiusPx()
{
	if (g_offscreenUseFov && g_markerFov > 10.f) return g_markerFov;
	return fminf(g_screenW, g_screenH) * 0.22f;
}

inline void DrawMarkerFovCircle(ImDrawList* dl)
{
	if (!g_drawMarkerCircle) return;
	const ImVec2 c(g_screenW * 0.5f, g_screenH * 0.5f);
	dl->AddCircle(c, g_markerFov, IM_COL32(0, 0, 0, 140), 64, 2.5f);
	dl->AddCircle(c, g_markerFov, IM_COL32(255, 255, 255, 180), 64, 1.2f);
}

inline bool EnemyInFovCircle(const EntityInfo& e, float fovR, float& outDistPx)
{
	float sx, sy;
	float3 ref = (e.boneCount > 0) ? e.bones[0] : e.pos;
	if (!WorldToScreen(ref, sx, sy)) return false;
	float cx = g_screenW * 0.5f, cy = g_screenH * 0.5f;
	float dx = sx - cx, dy = sy - cy;
	outDistPx = sqrtf(dx * dx + dy * dy);
	return outDistPx <= fovR;
}

inline void DrawFovThreats(ImDrawList* dl)
{
	const float cx = g_screenW * 0.5f;
	const float cy = g_screenH * 0.5f;
	const float fovR = GetFovRadiusPx();

	if (g_fovThreatCount || g_priorityMarker) {
		int inFov = 0;
		const EntityInfo* best = nullptr;
		float bestHpPct = 2.f;
		float bestDist = 0.f;

		for (auto& e : g_entities) {
			if (!IsEnemyEntity(e)) continue;
			if (e.health_max > 1.f && e.health <= 0.f) continue;
			float dpx = 0.f;
			if (!EnemyInFovCircle(e, fovR, dpx)) continue;
			inFov++;
			float maxHp = e.health_max > 1.f ? e.health_max : 100.f;
			float hpPct = e.health / maxHp;
			if (hpPct < bestHpPct) {
				bestHpPct = hpPct;
				best = &e;
				bestDist = dpx;
			}
		}

		if (g_fovThreatCount && inFov > 0) {
			char buf[8];
			snprintf(buf, sizeof(buf), S("%d"), inFov);
			ImVec2 ts = ImGui::CalcTextSize(buf);
			dl->AddText(ImVec2(cx - ts.x * 0.5f, cy + fovR + 4.f), IM_COL32(255, 255, 255, 160), buf);
		}

		if (g_priorityMarker && best && best->screenValid) {
			float hx = (best->boxValid) ? (best->boxL + best->boxR) * 0.5f : best->boneScreen[0].x;
			float hy = (best->boxValid) ? best->boxT - 6.f : best->boneScreen[0].y - 14.f;
			if (hx >= 0.f) {
				dl->AddLine(ImVec2(hx - 6.f, hy), ImVec2(hx + 6.f, hy), IM_COL32(255, 60, 60, 220), 2.f);
				dl->AddLine(ImVec2(hx, hy - 6.f), ImVec2(hx, hy + 6.f), IM_COL32(255, 60, 60, 220), 2.f);
			}
		}
	}
}

inline void DrawOffscreenArrows(ImDrawList* dl)
{
	if (!g_offscreenArrows) return;

	const float cx = g_screenW * 0.5f;
	const float cy = g_screenH * 0.5f;
	const float fovR = GetFovRadiusPx();
	const float maxDist = 60.f;

	for (auto& e : g_entities) {
		if (!IsEnemyEntity(e)) continue;
		if (e.health_max > 1.f && e.health <= 0.f) continue;

		float sx, sy;
		float3 ref = (e.boneCount > 0) ? e.bones[0] : e.pos;
		if (WorldToScreen(ref, sx, sy)) continue;

		float dx = ref.x - g_cameraPos.x;
		float dy = ref.y - g_cameraPos.y;
		float dz = ref.z - g_cameraPos.z;
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);
		if (dist > maxDist) continue;

		const float* m = g_viewMatrix.data;
		float vx = m[0] * dx + m[4] * dy + m[8] * dz;
		float vy = m[1] * dx + m[5] * dy + m[9] * dz;
		float len = sqrtf(vx * vx + vy * vy);
		if (len < 0.001f) continue;
		vx /= len; vy /= len;

		float ax = cx + vx * fovR;
		float ay = cy + vy * fovR;

		float maxHp = e.health_max > 1.f ? e.health_max : 100.f;
		float hpPct = e.health / maxHp;
		ImU32 col = (hpPct <= 0.35f) ? IM_COL32(255, 80, 80, 200) : IM_COL32(255, 255, 255, 170);

		float sz = 5.f;
		ImVec2 tip(cx + vx * (fovR + sz), cy + vy * (fovR + sz));
		ImVec2 p1(ax - vy * sz, ay + vx * sz);
		ImVec2 p2(ax + vy * sz, ay - vx * sz);
		dl->AddTriangleFilled(tip, p1, p2, col);
	}
}

inline void DrawBehindWarning(ImDrawList* dl)
{
	if (!g_behindWarning) return;

	int behind = 0;
	const float* m = g_viewMatrix.data;
	float fx = m[8], fy = m[9], fz = m[10];
	float flen = sqrtf(fx * fx + fy * fy + fz * fz);
	if (flen > 0.001f) { fx /= flen; fy /= flen; fz /= flen; }

	for (auto& e : g_entities) {
		if (!IsEnemyEntity(e)) continue;
		if (e.health_max > 1.f && e.health <= 0.f) continue;
		float3 ref = (e.boneCount > 0) ? e.bones[0] : e.pos;
		float dx = ref.x - g_cameraPos.x;
		float dy = ref.y - g_cameraPos.y;
		float dz = ref.z - g_cameraPos.z;
		float dist = sqrtf(dx * dx + dy * dy + dz * dz);
		if (dist > 25.f || dist < 0.5f) continue;
		float dot = dx * fx + dy * fy + dz * fz;
		if (dot < -2.f) behind++;
	}

	if (behind <= 0) return;

	char buf[24];
	snprintf(buf, sizeof(buf), S("%d behind"), behind);
	ImVec2 ts = ImGui::CalcTextSize(buf);
	dl->AddText(ImVec2(g_screenW * 0.5f - ts.x * 0.5f, g_screenH - 90.f),
		IM_COL32(255, 100, 100, 200), buf);
}
