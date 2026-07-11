#pragma once
#include "sdk.hpp"
#include "offsets.hpp"
#include "imgui.h"
#include <cmath>
#include <vector>
#include <algorithm>

inline ImU32 OverlayChamsColor(float alphaScale)
{
	int a = (int)(g_chamsFill * alphaScale * 255.f + 0.5f);
	int r = (int)(g_chamsColor[0] * 255.f + 0.5f);
	int g = (int)(g_chamsColor[1] * 255.f + 0.5f);
	int b = (int)(g_chamsColor[2] * 255.f + 0.5f);
	if (a > 255) a = 255;
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	return IM_COL32(r, g, b, a);
}

inline void BuildConvexHull(std::vector<ImVec2>& in, std::vector<ImVec2>& out)
{
	out.clear();
	if (in.size() < 3) return;

	auto cross = [](ImVec2 o, ImVec2 a, ImVec2 b) {
		return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x);
	};

	std::sort(in.begin(), in.end(), [](ImVec2 a, ImVec2 b) {
		return a.x < b.x || (a.x == b.x && a.y < b.y);
	});

	for (size_t i = 0; i < in.size(); ++i) {
		while (out.size() >= 2 && cross(out[out.size() - 2], out.back(), in[i]) <= 0.f)
			out.pop_back();
		out.push_back(in[i]);
	}
	for (int i = (int)in.size() - 2, t = (int)out.size() + 1; i >= 0; --i) {
		while (out.size() >= (size_t)t && cross(out[out.size() - 2], out.back(), in[i]) <= 0.f)
			out.pop_back();
		out.push_back(in[i]);
	}
	if (!out.empty()) out.pop_back();
}

inline void DrawOverlayChams(ImDrawList* dl, const EntityInfo& e)
{
	if (!g_chams || !e.screenValid) return;

	ImU32 fillCol   = OverlayChamsColor(0.42f);
	ImU32 edgeCol   = OverlayChamsColor(0.95f);
	ImU32 shadowCol = IM_COL32(0, 0, 0, (int)(g_chamsFill * 100.f));

	if (e.hasBones) {
		static const int hullBones[] = { 0, 1, 4, 5, 6, 7, 12, 13, 3, 8, 9, 10, 11, 14, 15, 16, 17 };
		std::vector<ImVec2> pts;
		pts.reserve(20);
		for (int id : hullBones) {
			if (id >= e.boneCount || e.boneScreen[id].x < 0) continue;
			const float3& b = e.bones[id];
			if (b.x == 0.f && b.y == 0.f && b.z == 0.f) continue;
			pts.push_back(ImVec2(e.boneScreen[id].x, e.boneScreen[id].y));
		}
		if (pts.size() >= 3) {
			std::vector<ImVec2> hull;
			BuildConvexHull(pts, hull);
			if (hull.size() >= 3)
				dl->AddConvexPolyFilled(hull.data(), (int)hull.size(), fillCol);
		}
	}

	if (e.boxValid) {
		dl->AddRect(
			ImVec2(e.boxL, e.boxT), ImVec2(e.boxR, e.boxB),
			edgeCol, 2.f, 0, 1.5f);
	}

	if (!e.hasBones) return;

	for (int li = 0; li < 17; li++) {
		int i1 = g_boneLines[li][0], i2 = g_boneLines[li][1];
		if (i1 >= e.boneCount || i2 >= e.boneCount) continue;
		if (e.boneScreen[i1].x < 0 || e.boneScreen[i2].x < 0) continue;
		const float3& b1 = e.bones[i1];
		const float3& b2 = e.bones[i2];
		if ((b1.x == 0.f && b1.y == 0.f && b1.z == 0.f) ||
		    (b2.x == 0.f && b2.y == 0.f && b2.z == 0.f))
			continue;

		ImVec2 p1(e.boneScreen[i1].x, e.boneScreen[i1].y);
		ImVec2 p2(e.boneScreen[i2].x, e.boneScreen[i2].y);
		dl->AddLine(p1, p2, shadowCol, 6.f);
		dl->AddLine(p1, p2, edgeCol,   2.f);
	}
}

inline void DrawAllOverlayChams(ImDrawList* dl)
{
	if (!g_chams) return;
	for (auto& e : g_entities) {
		if (e.isLocal) continue;
		if (!IsEnemyEntity(e)) continue;
		DrawOverlayChams(dl, e);
	}
}
