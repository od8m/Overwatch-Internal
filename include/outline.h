#pragma once
#include "sdk.hpp"
#include "utils.hpp"
#include "offsets.hpp"
#include "imgui.h"
#include <intrin.h>
#include <unordered_map>

struct OutlineGlowCache {
	uint64_t comp = 0;
	uint64_t borderType = UINT64_MAX;
	uint32_t color = 0;
	float thickness = -1.f;
};

static std::unordered_map<uint32_t, OutlineGlowCache> g_outlineGlowCache;
static std::unordered_map<uint32_t, uint64_t> g_outlineCompCache;

inline void ResetOutlineCaches()
{
	g_outlineGlowCache.clear();
	g_outlineCompCache.clear();
}

inline uint32_t PackGlowColor(const float c[3])
{
	uint32_t r = (uint32_t)(c[0] * 255.f + 0.5f);
	uint32_t g = (uint32_t)(c[1] * 255.f + 0.5f);
	uint32_t b = (uint32_t)(c[2] * 255.f + 0.5f);
	if (r > 255) r = 255;
	if (g > 255) g = 255;
	if (b > 255) b = 255;
	return (0xFFu << 24) | (r << 16) | (g << 8) | b;
}

inline uint64_t ResolveOutlineComp(const EntityInfo& e)
{
	auto it = g_outlineCompCache.find(e.uid);
	if (it != g_outlineCompCache.end() && it->second)
		return it->second;

	uint64_t oc = GetDecryptedComponent(e.addr, TYPE_OUTLINE);
	if (!oc || oc < 0x10000ULL || oc >= 0x800000000000ULL)
		oc = GetDecryptedComponent(e.common, TYPE_OUTLINE);
	if (oc && oc >= 0x10000ULL && oc < 0x800000000000ULL)
		g_outlineCompCache[e.uid] = oc;
	return oc;
}

inline void Glow(uint64_t outlineComp, uint64_t borderType, uint32_t color, float thickness)
{
	if (!outlineComp || outlineComp < 0x10000ULL || outlineComp >= 0x800000000000ULL) return;
	__try {
		uint64_t a1 = outlineComp + 0x20;
		int32_t  v1 = SDK->RPM<int32_t>(a1 + 0x68);
		if (v1 <= 0 || v1 > 4096) return;

		uint64_t ptr = SDK->RPM<uint64_t>(a1 + 0x60);
		if (ptr < 0x10000ULL || ptr >= 0x800000000000ULL) return;

		uint64_t borderStruct = ptr + 0x20 * (uint64_t)v1 - 0x20;

		uint64_t res = borderType;
		res -= 0x611ECA4482EA8191ULL;
		res  = _rotr64(_rotr64(res, 61) + 0x3EBFD39CA11323F6ULL, 20);
		res  = (_rotr64(_rotl64(res, 53), 32) + 0x132819D24864091FULL) ^ 0xBB01894FA1FCFFE6ULL;

		if (SDK->RPM<uint64_t>(borderStruct + 0x10) != res)
			SDK->WPM<uint64_t>(borderStruct + 0x10, res);

		if (borderType > 0 && color > 0) {
			if (SDK->RPM<uint32_t>(outlineComp + 0x110) != color)
				SDK->WPM<uint32_t>(outlineComp + 0x110, color);
			if (SDK->RPM<uint32_t>(outlineComp + 0x124) != color)
				SDK->WPM<uint32_t>(outlineComp + 0x124, color);
			if (SDK->RPM<uint8_t>(outlineComp + 0x114) != 1)
				SDK->WPM<uint8_t>(outlineComp + 0x114, (uint8_t)1);
			if (SDK->RPM<float>(outlineComp + 0x130) != thickness)
				SDK->WPM<float>(outlineComp + 0x130, thickness);
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
}

inline void ApplyEngineGlow()
{
	if (!g_glow) return;

	uint32_t col = PackGlowColor(g_glowColor);

	for (auto& e : g_entities) {
		if (e.isLocal) continue;
		if (g_teamCheck && g_localTeam != 0 && e.team == g_localTeam) continue;

		uint64_t oc = ResolveOutlineComp(e);
		if (!oc) continue;

		uint64_t borderType = e.visible ? 1 : 2;

		OutlineGlowCache& st = g_outlineGlowCache[e.uid];
		if (st.comp == oc &&
		    st.borderType == borderType &&
		    st.color == col &&
		    st.thickness == g_glowThickness)
			continue;

		Glow(oc, borderType, col, g_glowThickness);

		st.comp = oc;
		st.borderType = borderType;
		st.color = col;
		st.thickness = g_glowThickness;
	}

	// Drop cache entries for entities that left the match.
	if (g_outlineGlowCache.size() > g_entities.size() + 8) {
		std::unordered_map<uint32_t, OutlineGlowCache> keep;
		for (auto& e : g_entities) {
			auto it = g_outlineGlowCache.find(e.uid);
			if (it != g_outlineGlowCache.end())
				keep[e.uid] = it->second;
		}
		g_outlineGlowCache.swap(keep);
		g_outlineCompCache.clear();
		for (auto& e : g_entities) {
			uint64_t oc = ResolveOutlineComp(e);
			if (oc) (void)oc;
		}
	}
}

inline void ClearEngineGlow()
{
	for (auto& e : g_entities) {
		if (e.isLocal) continue;

		uint64_t oc = ResolveOutlineComp(e);
		if (!oc) continue;

		Glow(oc, 0, 0, 0.f);
	}
	ResetOutlineCaches();
}
