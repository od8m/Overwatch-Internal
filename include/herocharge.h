#pragma once
#include "sdk.hpp"
#include "memory.hpp"
#include "ulttracker.h"
#include <cmath>

inline int NormalizeSkillCharge(float val)
{
	if (isnan(val) || val < 0.f) return -1;
	if (val <= 1.001f) return (int)(val * 100.f + 0.5f);
	if (val <= 101.f) return (int)(val + 0.5f);
	return -1;
}

inline uint16_t GetLocalHeroId()
{
	for (auto& e : g_entities) {
		if (e.isLocal) return e.heroId;
	}
	return 0;
}

inline bool GetLocalEntity(uint64_t& common, uint64_t& link)
{
	for (auto& e : g_entities) {
		if (!e.isLocal) continue;
		common = e.common;
		link = e.addr;
		return true;
	}
	return false;
}

inline int GetLocalSkillChargePct(uint16_t hash)
{
	uint64_t common = 0, link = 0;
	if (!GetLocalEntity(common, link)) return -1;

	SkillSlot slots[24];
	int n = EnumSkillSlots(common, link, slots, 24);
	for (int i = 0; i < n; i++) {
		if (hash && slots[i].hash != hash) continue;
		return NormalizeSkillCharge(slots[i].raw60);
	}
	return -1;
}

inline int GetLocalMaxSkillChargePct()
{
	uint64_t common = 0, link = 0;
	if (!GetLocalEntity(common, link)) return -1;

	int best = -1;
	SkillSlot slots[24];
	int n = EnumSkillSlots(common, link, slots, 24);
	for (int i = 0; i < n; i++) {
		int pct = NormalizeSkillCharge(slots[i].raw60);
		if (pct > best) best = pct;
	}
	return best;
}
