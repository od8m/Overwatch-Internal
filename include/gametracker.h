#pragma once
#include "sdk.hpp"
#include "ulttracker.h"
#include "imgui.h"
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cmath>
#include <algorithm>
#include <cfloat>

struct GameAlert {
	char     text[96];
	uint64_t untilMs;
	ImU32    color;
};

extern std::vector<GameAlert> g_gameAlerts;

struct HeroTrackerState {
	std::unordered_map<uint32_t, uint16_t> lastHero;
	std::unordered_map<uint32_t, bool>     wasDead;
	std::unordered_map<uint32_t, int>      lastUlt;
};
extern HeroTrackerState g_heroTracker;

inline void ResetHeroTrackerState()
{
	g_heroTracker = HeroTrackerState{};
	g_gameAlerts.clear();
}

inline void PushGameAlert(const char* txt, ImU32 col, uint32_t durationMs = 4000)
{
	GameAlert a = {};
	strncpy(a.text, txt, sizeof(a.text) - 1);
	a.untilMs = GetTickCount64() + durationMs;
	a.color = col;
	g_gameAlerts.push_back(a);
	if (g_gameAlerts.size() > 12)
		g_gameAlerts.erase(g_gameAlerts.begin());
}

inline const char* MatchStateToPhase(uint32_t state)
{
	switch (state) {
		case 0: return "Lobby";
		case 1: return "Waiting";
		case 2: return "Setup";
		case 3: return "In Progress";
		case 4: return "Overtime";
		case 5: return "Round End";
		case 6: return "Match End";
		default: return nullptr;
	}
}

inline const char* RegionIdToName(uint32_t id)
{
	switch (id) {
		case 1: return "US";
		case 2: return "EU";
		case 3: return "KR";
		case 4: return "CN";
		case 5: return "APAC";
		default: return nullptr;
	}
}

inline void FormatGameTime(float seconds, char* out, int n)
{
	if (seconds < 0.f || isnan(seconds) || seconds > 7200.f) {
		snprintf(out, n, "--:--");
		return;
	}
	int total = (int)seconds;
	int mins = total / 60;
	int secs = total % 60;
	snprintf(out, n, "%d:%02d", mins, secs);
}

inline void UpdateHeroTracker()
{
	auto& s_lastHero = g_heroTracker.lastHero;
	auto& s_wasDead = g_heroTracker.wasDead;
	auto& s_lastUlt = g_heroTracker.lastUlt;

	for (auto& e : g_entities) {
		if (!e.uid || !e.heroId) continue;

		auto it = s_lastHero.find(e.uid);
		if (it != s_lastHero.end() && it->second != 0 && it->second != e.heroId) {
			if (g_heroSwapAlert) {
				const char* oldN = _hname(it->second);
				const char* newN = _hname(e.heroId);
				char buf[96];
				if (IsEnemyEntity(e))
					snprintf(buf, sizeof(buf), S("Enemy swapped: %s -> %s"),
						oldN ? oldN : "?", newN ? newN : "?");
				else if (!e.isLocal)
					snprintf(buf, sizeof(buf), S("Ally swapped: %s -> %s"),
						oldN ? oldN : "?", newN ? newN : "?");
				else
					snprintf(buf, sizeof(buf), S("You swapped to %s"), newN ? newN : "?");
				PushGameAlert(buf, IM_COL32(255, 220, 80, 255));
			}
		}
		s_lastHero[e.uid] = e.heroId;

		bool dead = e.health_max > 1.f && e.health <= 0.f;
		auto dit = s_wasDead.find(e.uid);
		if (dit != s_wasDead.end() && dit->second && !dead && IsEnemyEntity(e) && g_heroSwapAlert) {
			const char* name = _hname(e.heroId);
			char buf[64];
			snprintf(buf, sizeof(buf), S("%s respawned"), name ? name : "Enemy");
			PushGameAlert(buf, IM_COL32(255, 120, 120, 255), 2500);
		}
		s_wasDead[e.uid] = dead;

		if (IsEnemyEntity(e) && !dead && g_ultReadyAlert) {
			int uc = _ult(e.common, e.addr);
			int& prev = s_lastUlt[e.uid];
			if (uc >= 100 && prev >= 0 && prev < 100) {
				const char* name = _hname(e.heroId);
				char buf[64];
				snprintf(buf, sizeof(buf), S("%s ULT READY"), name ? name : "Enemy");
				PushGameAlert(buf, IM_COL32(0, 255, 255, 255), 3500);
			}
			prev = uc;
		}
	}

	uint64_t now = GetTickCount64();
	g_gameAlerts.erase(
		std::remove_if(g_gameAlerts.begin(), g_gameAlerts.end(),
			[now](const GameAlert& a) { return now >= a.untilMs; }),
		g_gameAlerts.end());
}

inline void DrawGameAlerts(ImDrawList* dl)
{
	if (g_gameAlerts.empty()) return;

	float y = 80.f;
	for (auto& a : g_gameAlerts) {
		ImVec2 ts = ImGui::CalcTextSize(a.text);
		float bx = g_screenW * 0.5f - ts.x * 0.5f - 10.f;
		dl->AddRectFilled(ImVec2(bx, y), ImVec2(bx + ts.x + 20.f, y + ts.y + 8.f),
			IM_COL32(10, 10, 14, 200), 4.f);
		dl->AddRect(ImVec2(bx, y), ImVec2(bx + ts.x + 20.f, y + ts.y + 8.f), a.color, 4.f);
		dl->AddText(ImVec2(bx + 10.f, y + 4.f), a.color, a.text);
		y += ts.y + 14.f;
	}
}

inline void DrawMatchTimer(ImDrawList* dl)
{
	if (!g_drawMatchTimer) return;

	const char* timeTxt = g_worldInfo.timeText[0] ? g_worldInfo.timeText : "--:--";

	char subLine[160] = {};
	int subLen = 0;
	auto appendPart = [&](const char* s) {
		if (!s || !s[0]) return;
		if (subLen > 0) { subLen += snprintf(subLine + subLen, sizeof(subLine) - subLen, "  ·  "); }
		subLen += snprintf(subLine + subLen, sizeof(subLine) - subLen, "%s", s);
	};
	if (g_matchHudShowMap)   appendPart(g_worldInfo.mapName);
	if (g_matchHudShowPhase) appendPart(g_worldInfo.phaseName);
	if (g_matchHudShowPing && g_worldInfo.pingValid) {
		char pingBuf[16];
		snprintf(pingBuf, sizeof(pingBuf), "%ums", g_worldInfo.pingMs);
		appendPart(pingBuf);
	}

	const float topPad = 10.f;
	const ImU32 shadow = IM_COL32(0, 0, 0, 180);
	const ImU32 textCol = IM_COL32(235, 240, 248, g_matchHudTransparent ? 230 : 255);
	const ImU32 timerCol = IM_COL32(255, 255, 255, g_matchHudTransparent ? 245 : 255);
	const ImU32 subCol = IM_COL32(180, 190, 205, g_matchHudTransparent ? 200 : 230);

	if (g_matchHudStyle == 1) {
		// Center hero timer — no box, large digits
		if (g_matchHudShowTimer) {
			ImFont* font = ImGui::GetFont();
			float fs = ImGui::GetFontSize() * 1.65f;
			ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.f, timeTxt);
			float tx = g_screenW * 0.5f - ts.x * 0.5f;
			float ty = topPad;
			dl->AddText(font, fs, ImVec2(tx + 1.f, ty + 1.f), shadow, timeTxt);
			dl->AddText(font, fs, ImVec2(tx, ty), timerCol, timeTxt);
			if (subLine[0]) {
				ImVec2 ss = ImGui::CalcTextSize(subLine);
				float sx = g_screenW * 0.5f - ss.x * 0.5f;
				float sy = ty + ts.y + 2.f;
				dl->AddText(ImVec2(sx + 1.f, sy + 1.f), shadow, subLine);
				dl->AddText(ImVec2(sx, sy), subCol, subLine);
			}
		} else if (subLine[0]) {
			ImVec2 ss = ImGui::CalcTextSize(subLine);
			float sx = g_screenW * 0.5f - ss.x * 0.5f;
			dl->AddText(ImVec2(sx + 1.f, topPad + 1.f), shadow, subLine);
			dl->AddText(ImVec2(sx, topPad), subCol, subLine);
		}
		return;
	}

	char line[192] = {};
	int len = 0;
	auto addSeg = [&](const char* s) {
		if (!s || !s[0]) return;
		if (len > 0) len += snprintf(line + len, sizeof(line) - len, "  ·  ");
		len += snprintf(line + len, sizeof(line) - len, "%s", s);
	};
	if (g_matchHudShowMap)   addSeg(g_worldInfo.mapName);
	if (g_matchHudShowPhase) addSeg(g_worldInfo.phaseName);
	if (g_matchHudShowTimer) addSeg(timeTxt);
	if (g_matchHudShowPing && g_worldInfo.pingValid) {
		char pingBuf[16];
		snprintf(pingBuf, sizeof(pingBuf), "%ums", g_worldInfo.pingMs);
		addSeg(pingBuf);
	}
	if (!line[0]) return;

	ImVec2 ts = ImGui::CalcTextSize(line);
	float bx = g_screenW * 0.5f - ts.x * 0.5f - 10.f;
	float by = topPad;

	if (g_matchHudStyle == 2 && !g_matchHudTransparent) {
		dl->AddRectFilled(ImVec2(bx, by), ImVec2(bx + ts.x + 20.f, by + ts.y + 8.f),
			IM_COL32(8, 10, 16, 190), 4.f);
		dl->AddRect(ImVec2(bx, by), ImVec2(bx + ts.x + 20.f, by + ts.y + 8.f),
			IM_COL32(60, 70, 90, 120), 4.f);
		by += 4.f; bx += 10.f;
	} else {
		bx += 10.f;
	}

	dl->AddText(ImVec2(bx + 1.f, by + 1.f), shadow, line);
	dl->AddText(ImVec2(bx, by), textCol, line);
}

inline void DrawDistanceEsp(ImDrawList* dl, const EntityInfo& e)
{
	if (!g_distanceEsp || !e.screenValid) return;

	float3 ref = (e.boneCount > 0) ? e.bones[0] : e.pos;
	float dx = ref.x - g_cameraPos.x;
	float dy = ref.y - g_cameraPos.y;
	float dz = ref.z - g_cameraPos.z;
	float dist = sqrtf(dx * dx + dy * dy + dz * dz);

	char buf[16];
	snprintf(buf, sizeof(buf), S("%.0fm"), dist);

	float tx = e.boxValid ? (e.boxL + e.boxR) * 0.5f : e.boneScreen[0].x;
	float ty = e.boxValid ? e.boxB + 4.f : e.boneScreen[0].y + 22.f;
	ImVec2 ts = ImGui::CalcTextSize(buf);
	dl->AddText(ImVec2(tx - ts.x * 0.5f + 1, ty + 1), IM_COL32(0, 0, 0, 200), buf);
	dl->AddText(ImVec2(tx - ts.x * 0.5f, ty), IM_COL32(200, 220, 255, 240), buf);
}
