#include "features.hpp"
#include "memory.hpp"
#include "gametracker.h"
#include "offsets.hpp"
#include <cmath>
#include <cstring>
#include <cctype>

static float DistSqToCamera(const EntityInfo& e)
{
	float3 ref = (e.boneCount > 0) ? e.bones[0] : e.pos;
	float dx = ref.x - g_cameraPos.x;
	float dy = ref.y - g_cameraPos.y;
	float dz = ref.z - g_cameraPos.z;
	return dx * dx + dy * dy + dz * dz;
}

static bool IsAliveHero(const EntityInfo& e)
{
	if (e.health_max > 1.f && e.health <= 0.f) return false;
	return true;
}

static bool IsLikelyTrainingBot(const EntityInfo& e)
{
	return e.boneCount > 0 && e.boneCount <= 5 && e.health_max <= 150.f;
}

static bool IsLikelyRealHero(const EntityInfo& e)
{
	return e.boneCount >= 12 && e.health_max >= 175.f;
}

static float ScoreLocalCandidate(const EntityInfo& e)
{
	if (!IsAliveHero(e)) return -1e9f;

	float score = 0.f;

	if (e.health_max >= 225.f)     score += 1500.f;
	else if (e.health_max >= 175.f) score += 700.f;
	else if (e.health_max <= 100.f) score -= 700.f;

	if (e.boneCount >= 12)      score += 900.f;
	else if (e.boneCount >= 8)  score += 300.f;
	else if (e.boneCount <= 5)  score -= 300.f;

	if (e.hasPC) score += 250.f;

	score -= sqrtf(DistSqToCamera(e)) * 0.8f;
	return score;
}

static void ApplyLocalTeamFrom(const EntityInfo& e)
{
	g_localTeam = e.team;
	g_localTeamMask = GetTeamMask(e.teamRaw);
	g_localPlayerUid = e.uid;
}

static void ApplyPracticeFallbackTeam()
{
	g_localPlayerUid = 0;
	g_localTeam = 2;
	g_localTeamMask = (1u << 24);
}

struct LocalDetectState {
	uint32_t stickyUid = 0;
	int      stickyTeam = 0;
	uint32_t stickyMask = 0;
	int      missFrames = 0;
	int      emptyFrames = 0;
};
static LocalDetectState s_localDetect;

void ResetLocalPlayerDetection()
{
	s_localDetect = {};
	g_localTeam = 0;
	g_localTeamMask = 0;
	g_localPlayerUid = 0;
}

void DoLocalPlayerDetection() {
	for (auto& e : g_entities)
		e.isLocal = false;

	if (g_entities.empty()) {
		if (++s_localDetect.emptyFrames > 90) {
			s_localDetect.stickyUid = 0;
			s_localDetect.missFrames = 0;
			g_localTeam = 0;
			g_localTeamMask = 0;
			g_localPlayerUid = 0;
		} else if (s_localDetect.stickyMask) {
			g_localTeam = s_localDetect.stickyTeam;
			g_localTeamMask = s_localDetect.stickyMask;
			g_localPlayerUid = s_localDetect.stickyUid;
		}
		return;
	}
	s_localDetect.emptyFrames = 0;

	auto commitLocal = [&](EntityInfo& e) {
		e.isLocal = true;
		ApplyLocalTeamFrom(e);
		s_localDetect.stickyUid = e.uid;
		s_localDetect.stickyTeam = g_localTeam;
		s_localDetect.stickyMask = g_localTeamMask;
		s_localDetect.missFrames = 0;
	};

	bool stickyInList = false;
	if (s_localDetect.stickyUid) {
		for (auto& e : g_entities) {
			if (e.uid == s_localDetect.stickyUid) { stickyInList = true; break; }
		}
	}

	for (auto& e : g_entities) {
		if (!e.pcLocal || !IsAliveHero(e)) continue;
		if (!IsLikelyRealHero(e) && IsLikelyTrainingBot(e)) continue;
		if (stickyInList && e.uid != s_localDetect.stickyUid) continue;
		commitLocal(e);
		return;
	}

	if (s_localDetect.stickyUid) {
		for (auto& e : g_entities) {
			if (e.uid != s_localDetect.stickyUid) continue;
			if (!IsAliveHero(e)) {
				e.isLocal = true;
				g_localTeam = s_localDetect.stickyTeam;
				g_localTeamMask = s_localDetect.stickyMask;
				g_localPlayerUid = s_localDetect.stickyUid;
				s_localDetect.missFrames = 0;
				return;
			}
			if (!IsLikelyRealHero(e) && IsLikelyTrainingBot(e)) break;
			commitLocal(e);
			return;
		}
		if (++s_localDetect.missFrames < 300 && s_localDetect.stickyMask) {
			g_localTeam = s_localDetect.stickyTeam;
			g_localTeamMask = s_localDetect.stickyMask;
			g_localPlayerUid = s_localDetect.stickyUid;
			return;
		}
		s_localDetect.stickyUid = 0;
		s_localDetect.missFrames = 0;
	}

	int bestIdx = -1;
	float bestScore = -1e9f;
	int bestHeroIdx = -1;
	float bestHeroScore = -1e9f;

	for (size_t i = 0; i < g_entities.size(); i++) {
		const EntityInfo& e = g_entities[i];
		float score = ScoreLocalCandidate(e);
		if (score > bestScore) {
			bestScore = score;
			bestIdx = (int)i;
		}
		if (IsLikelyRealHero(e) && score > bestHeroScore) {
			bestHeroScore = score;
			bestHeroIdx = (int)i;
		}
	}

	int pickIdx = (bestHeroIdx >= 0 && bestHeroScore >= 400.f) ? bestHeroIdx : bestIdx;
	float pickScore = (pickIdx == bestHeroIdx) ? bestHeroScore : bestScore;

	bool anyPc = false;
	for (auto& e : g_entities) {
		if (e.hasPC && IsAliveHero(e) && IsLikelyRealHero(e)) { anyPc = true; break; }
	}
	if (anyPc && pickIdx >= 0 && !g_entities[pickIdx].hasPC) {
		float bestPcScore = -1e9f;
		int bestPcIdx = -1;
		for (size_t i = 0; i < g_entities.size(); i++) {
			const EntityInfo& e = g_entities[i];
			if (!e.hasPC || !IsLikelyRealHero(e)) continue;
			float score = ScoreLocalCandidate(e);
			if (score > bestPcScore) { bestPcScore = score; bestPcIdx = (int)i; }
		}
		if (bestPcIdx >= 0 && bestPcScore >= 300.f) {
			pickIdx = bestPcIdx;
			pickScore = bestPcScore;
		}
	}

	if (pickIdx < 0 || pickScore < 200.f) {
		bool allBots = true;
		for (auto& e : g_entities) {
			if (!IsLikelyTrainingBot(e)) { allBots = false; break; }
		}
		if (allBots && !g_entities.empty()) {
			ApplyPracticeFallbackTeam();
			return;
		}
		if (s_localDetect.stickyMask) {
			g_localTeam = s_localDetect.stickyTeam;
			g_localTeamMask = s_localDetect.stickyMask;
			g_localPlayerUid = s_localDetect.stickyUid;
			return;
		}
		g_localTeam = 0;
		g_localTeamMask = 0;
		g_localPlayerUid = 0;
		return;
	}

	EntityInfo& pick = g_entities[pickIdx];
	if (IsLikelyTrainingBot(pick) && !IsLikelyRealHero(pick)) {
		ApplyPracticeFallbackTeam();
		return;
	}

	pick.isLocal = true;
	ApplyLocalTeamFrom(pick);
	s_localDetect.stickyUid = pick.uid;
	s_localDetect.stickyTeam = g_localTeam;
	s_localDetect.stickyMask = g_localTeamMask;
	s_localDetect.missFrames = 0;
}

static const char* MapIdToName(uint32_t id)
{
	switch (id) {
		case 0x01: return "Hanamura";
		case 0x03: return "King's Row";
		case 0x04: return "Numbani";
		case 0x05: return "Hollywood";
		case 0x06: return "Volskaya";
		case 0x07: return "Lijiang Tower";
		case 0x08: return "Nepal";
		case 0x09: return "Ilios";
		case 0x0A: return "Route 66";
		case 0x0B: return "Watchpoint: Gibraltar";
		case 0x0C: return "Eichenwalde";
		case 0x0D: return "Oasis";
		case 0x0E: return "Horizon Lunar Colony";
		case 0x0F: return "Junkertown";
		case 0x10: return "Rialto";
		case 0x11: return "Busan";
		case 0x12: return "Blizzard World";
		case 0x13: return "Paris";
		case 0x14: return "Havana";
		case 0x15: return "Workshop Island";
		case 0x16: return "Practice Range";
		case 0x17: return "Chateau Guillard";
		case 0x18: return "Dorado";
		case 0x19: return "Circuit Royal";
		case 0x1A: return "Midtown";
		case 0x1B: return "Colosseo";
		case 0x1C: return "Esperanca";
		case 0x1D: return "New Queen Street";
		case 0x1E: return "Suravasa";
		case 0x1F: return "Samoa";
		case 0x20: return "Runasapi";
		case 0x21: return "Hanaoka";
		case 0x22: return "Throne of Anubis";
		case 0x23: return "Aatlis";
		case 0x24: return "New Junk City";
		default: return nullptr;
	}
}

static bool _strEqI(const char* a, const char* b)
{
	if (!a || !b) return false;
	for (; *a && *b; a++, b++) {
		char ca = (char)tolower((unsigned char)*a);
		char cb = (char)tolower((unsigned char)*b);
		if (ca != cb) return false;
	}
	return *a == 0 && *b == 0;
}

static bool _strContainsI(const char* hay, const char* needle)
{
	if (!hay || !needle || !needle[0]) return false;
	size_t nlen = strlen(needle);
	size_t hlen = strlen(hay);
	if (nlen > hlen) return false;
	for (size_t i = 0; i + nlen <= hlen; i++) {
		bool ok = true;
		for (size_t j = 0; j < nlen; j++) {
			char ca = (char)tolower((unsigned char)hay[i + j]);
			char cb = (char)tolower((unsigned char)needle[j]);
			if (ca != cb) { ok = false; break; }
		}
		if (ok) return true;
	}
	return false;
}

static const char* kKnownMapNames[] = {
	"Practice Range", "Workshop Island", "Watchpoint: Gibraltar",
	"Horizon Lunar Colony", "Chateau Guillard", "New Queen Street",
	"Circuit Royal", "Blizzard World", "King's Row", "Lijiang Tower",
	"Volskaya", "Junkertown", "New Junk City", "Throne of Anubis",
	"Hanamura", "Numbani", "Hollywood", "Nepal", "Ilios", "Route 66",
	"Eichenwalde", "Oasis", "Rialto", "Busan", "Paris", "Havana",
	"Dorado", "Midtown", "Colosseo", "Esperanca", "Suravasa", "Samoa",
	"Runasapi", "Hanaoka", "Aatlis",
	"Lijiang", "Ilios", "Nepal", "Busan", "Oasis", "Hollywood",
	"Hanamura", "Numbani", "Dorado", "Paris", "Havana", "Rialto",
	"Samoa", "Suravasa", "Esperanca", "Colosseo", "Midtown",
};

static bool ReadRemoteUtf16Map(uint64_t strPtr, char* out, int outSz)
{
	if (!IsValidPtr(strPtr)) return false;
	wchar_t w[64] = {};
	for (int i = 0; i < 63; i++) {
		w[i] = SDK->RPM<wchar_t>(strPtr + (uint64_t)i * 2);
		if (!w[i]) break;
		if (w[i] < 32 || w[i] > 0x7FFF) { out[0] = 0; return false; }
	}
	if (!w[0]) return false;
	WideCharToMultiByte(CP_UTF8, 0, w, -1, out, outSz, nullptr, nullptr);
	return out[0] != 0 && strlen(out) >= 3;
}

static bool ReadRemoteAsciiMap(uint64_t strPtr, char* out, int outSz)
{
	if (!IsValidPtr(strPtr)) return false;
	for (int i = 0; i < outSz - 1; i++) {
		char c = SDK->RPM<char>(strPtr + i);
		if (!c) break;
		if (c < 32 || c > 126) { out[0] = 0; return false; }
		out[i] = c;
		out[i + 1] = 0;
	}
	return out[0] != 0 && strlen(out) >= 3;
}

static bool MatchKnownMapName(const char* s, char* outName, int outSz)
{
	if (!s || !s[0]) return false;
	for (uint32_t id = 1; id <= 0x40; id++) {
		const char* canon = MapIdToName(id);
		if (canon && (_strEqI(s, canon) || _strContainsI(s, canon)))
			{ snprintf(outName, outSz, "%s", canon); return true; }
	}
	for (const char* known : kKnownMapNames) {
		if (_strContainsI(s, known)) {
			if (_strEqI(known, "Lijiang")) { snprintf(outName, outSz, "Lijiang Tower"); return true; }
			for (uint32_t id = 1; id <= 0x40; id++) {
				const char* canon = MapIdToName(id);
				if (canon && _strContainsI(canon, known)) {
					snprintf(outName, outSz, "%s", canon);
					return true;
				}
			}
			snprintf(outName, outSz, "%s", known);
			return true;
		}
	}
	return false;
}

static bool TryReadMapString(uint64_t strPtr, char* outName, int outSz)
{
	char buf[96] = {};
	if (ReadRemoteUtf16Map(strPtr, buf, sizeof(buf)) && MatchKnownMapName(buf, outName, outSz)) return true;
	buf[0] = 0;
	if (ReadRemoteAsciiMap(strPtr, buf, sizeof(buf)) && MatchKnownMapName(buf, outName, outSz)) return true;
	return false;
}

static bool ScanObjectForMapName(uint64_t base, char* outName, int outSz)
{
	if (!IsValidPtr(base)) return false;
	static const int ptrOffs[] = {
		0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48, 0x50,
		0x58, 0x60, 0x68, 0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA0,
		0xA8, 0xB0, 0xB8, 0xC0, 0xC8, 0xD0, 0xD8, 0xE0, 0xE8, 0xF0,
		0x100, 0x108, 0x110, 0x118, 0x120, 0x128, 0x130, 0x138, 0x140,
		0x148, 0x150, 0x158, 0x160, 0x168, 0x170, 0x178, 0x180, 0x188,
		0x190, 0x198, 0x1A0, 0x1A8, 0x1B0, 0x1C0, 0x1D0, 0x1E0, 0x1F0,
		0x200, 0x210, 0x220, 0x230, 0x240, 0x250, 0x260, 0x270, 0x280
	};
	for (int po : ptrOffs) {
		uint64_t p = SDK->RPM<uint64_t>(base + po);
		if (TryReadMapString(p, outName, outSz)) return true;
		if (IsValidPtr(p)) {
			uint64_t p2 = SDK->RPM<uint64_t>(p);
			if (TryReadMapString(p2, outName, outSz)) return true;
		}
	}
	return false;
}

static uint32_t ProbeNamedMapId(uint64_t base)
{
	if (!IsValidPtr(base)) return 0;
	for (uint64_t o = 0; o <= 0x200; o += 4) {
		uint32_t v = SDK->RPM<uint32_t>(base + o);
		if (MapIdToName(v)) return v;
	}
	return 0;
}

static uint32_t ProbeMatchState(uint64_t base)
{
	if (!IsValidPtr(base)) return UINT32_MAX;
	uint32_t preferred = SDK->RPM<uint32_t>(base + 0x30);
	if (preferred <= 6) return preferred;
	for (uint64_t o = 0; o <= 0x120; o += 4) {
		uint32_t v = SDK->RPM<uint32_t>(base + o);
		if (v <= 6 && v > 0) return v;
	}
	return UINT32_MAX;
}

static bool IsPlausiblePingMs(uint32_t v)
{
	return v >= 5 && v <= 200;
}

static uint32_t ReadPingAt(uint64_t ptr, uint64_t off, bool isFloat)
{
	if (!IsValidPtr(ptr)) return 0;
	if (isFloat) {
		float pf = SDK->RPM<float>(ptr + off);
		if (isnan(pf) || pf < 5.f || pf > 200.f) return 0;
		return (uint32_t)(pf + 0.5f);
	}
	uint32_t pi = SDK->RPM<uint32_t>(ptr + off);
	if (!IsPlausiblePingMs(pi)) return 0;
	return pi;
}

struct PingTracker {
	bool     locked = false;
	uint64_t ptr = 0;
	uint64_t off = 0;
	bool     isFloat = false;
	uint32_t displayMs = 0;
	bool     haveDisplay = false;
	uint32_t calibValue = 0;
	int      calibStreak = 0;
	int      failStreak = 0;
	uint64_t nextSampleMs = 0;
	uint64_t sampleIdx = 0;
	uint32_t sampleBuf[7] = {};
	int      sampleCount = 0;
	uint32_t pendingJump = 0;
	int      jumpStreak = 0;
};

static PingTracker g_ping;

static void ResetPingCalibration()
{
	g_ping.locked = false;
	g_ping.ptr = 0;
	g_ping.off = 0;
	g_ping.calibValue = 0;
	g_ping.calibStreak = 0;
	g_ping.failStreak = 0;
	g_ping.sampleCount = 0;
	g_ping.pendingJump = 0;
	g_ping.jumpStreak = 0;
}

static uint32_t MedianPingSample(const uint32_t* vals, int n)
{
	if (n <= 0) return 0;
	uint32_t tmp[7] = {};
	for (int i = 0; i < n; i++) tmp[i] = vals[i];
	for (int i = 1; i < n; i++) {
		uint32_t key = tmp[i];
		int j = i - 1;
		while (j >= 0 && tmp[j] > key) { tmp[j + 1] = tmp[j]; j--; }
		tmp[j + 1] = key;
	}
	return tmp[n / 2];
}

static void CalibratePing(uint64_t session, uint64_t admin, uint64_t world)
{
	const uint64_t bases[] = { session, admin, world };
	const uint64_t offs[] = {
		0x30, 0x34, 0x38, 0x3C, 0x40, 0x44, 0x48, 0x4C,
		0x50, 0x54, 0x58, 0x5C, 0x60, 0x64, 0x68, 0x6C,
		0x70, 0x74, 0x78, 0x7C, 0x80, 0x84, 0x88, 0x8C
	};
	for (uint64_t base : bases) {
		if (!IsValidPtr(base)) continue;
		for (uint64_t o : offs) {
			uint32_t pf = ReadPingAt(base, o, true);
			if (pf) {
				if (g_ping.calibStreak > 0 && g_ping.ptr == base && g_ping.off == o && g_ping.isFloat) {
					int d = (int)pf - (int)g_ping.calibValue;
					if (d < 0) d = -d;
					if (d <= 3) g_ping.calibStreak++;
					else { g_ping.calibValue = pf; g_ping.calibStreak = 1; }
				} else {
					g_ping.ptr = base; g_ping.off = o; g_ping.isFloat = true;
					g_ping.calibValue = pf; g_ping.calibStreak = 1;
				}
				if (g_ping.calibStreak >= 15) {
					g_ping.locked = true;
					if (!g_ping.haveDisplay) { g_ping.displayMs = pf; g_ping.haveDisplay = true; }
					g_ping.failStreak = 0;
					return;
				}
				continue;
			}
			uint32_t pi = ReadPingAt(base, o, false);
			if (pi) {
				if (g_ping.calibStreak > 0 && g_ping.ptr == base && g_ping.off == o && !g_ping.isFloat) {
					int d = (int)pi - (int)g_ping.calibValue;
					if (d < 0) d = -d;
					if (d <= 3) g_ping.calibStreak++;
					else { g_ping.calibValue = pi; g_ping.calibStreak = 1; }
				} else {
					g_ping.ptr = base; g_ping.off = o; g_ping.isFloat = false;
					g_ping.calibValue = pi; g_ping.calibStreak = 1;
				}
				if (g_ping.calibStreak >= 15) {
					g_ping.locked = true;
					if (!g_ping.haveDisplay) { g_ping.displayMs = pi; g_ping.haveDisplay = true; }
					g_ping.failStreak = 0;
					return;
				}
			}
		}
	}
}

static void UpdateStablePing(uint64_t session, uint64_t admin, uint64_t world)
{
	uint64_t now = GetTickCount64();

	if (g_ping.locked) {
		uint32_t raw = ReadPingAt(g_ping.ptr, g_ping.off, g_ping.isFloat);
		if (raw) {
			g_ping.failStreak = 0;
			if (g_ping.nextSampleMs == 0 || now >= g_ping.nextSampleMs) {
				if (g_ping.sampleCount < 7)
					g_ping.sampleBuf[g_ping.sampleCount++] = raw;
				else {
					for (int i = 0; i < 6; i++) g_ping.sampleBuf[i] = g_ping.sampleBuf[i + 1];
					g_ping.sampleBuf[6] = raw;
				}
				if (g_ping.sampleCount >= 5) {
					uint32_t med = MedianPingSample(g_ping.sampleBuf, g_ping.sampleCount);
					uint32_t spread = 0;
					for (int i = 0; i < g_ping.sampleCount; i++) {
						uint32_t d = (med > g_ping.sampleBuf[i]) ? med - g_ping.sampleBuf[i] : g_ping.sampleBuf[i] - med;
						if (d > spread) spread = d;
					}
					if (spread <= 12) {
						if (!g_ping.haveDisplay) {
							g_ping.displayMs = med;
							g_ping.haveDisplay = true;
						} else {
							int diff = (int)med - (int)g_ping.displayMs;
							if (diff < 0) diff = -diff;
							if (diff <= 30)
								g_ping.displayMs = (g_ping.displayMs * 3 + med + 2) / 4;
							else {
								if (g_ping.pendingJump != med) { g_ping.pendingJump = med; g_ping.jumpStreak = 1; }
								else if (++g_ping.jumpStreak >= 3)
									g_ping.displayMs = med;
							}
						}
					}
					g_ping.sampleCount = 0;
					g_ping.nextSampleMs = now + 4500;
				} else {
					g_ping.nextSampleMs = now + 600;
				}
			}
		} else if (++g_ping.failStreak >= 120) {
			ResetPingCalibration();
		}
	} else {
		CalibratePing(session, admin, world);
	}
}

static void ApplyStablePing(GameWorldInfo& info)
{
	if (g_ping.haveDisplay) {
		info.pingMs = g_ping.displayMs;
		info.pingValid = true;
	}
}

struct MapTracker {
	char     stableName[48] = "Unknown";
	uint32_t stableId = 0;
	uint32_t voteId = 0;
	int      voteStreak = 0;
	char     pendingName[48] = {};
	int      pendingNameStreak = 0;
};

static MapTracker g_map;

static void CommitMapName(const char* name, uint32_t idHint)
{
	if (!name || !name[0]) return;
	if (g_map.stableName[0] && strcmp(g_map.stableName, "Unknown") != 0 &&
		strcmp(g_map.stableName, name) != 0) {
		if (strcmp(g_map.pendingName, name) != 0) {
			snprintf(g_map.pendingName, sizeof(g_map.pendingName), "%s", name);
			g_map.pendingNameStreak = 1;
			return;
		}
		if (++g_map.pendingNameStreak < 30) return;
	}
	snprintf(g_map.stableName, sizeof(g_map.stableName), "%s", name);
	g_map.pendingName[0] = 0;
	g_map.pendingNameStreak = 0;
	if (idHint) g_map.stableId = idHint;
}

static void VoteMapId(uint32_t id)
{
	if (!id || !MapIdToName(id)) return;
	if (id == g_map.voteId) {
		if (++g_map.voteStreak >= 30 && g_map.stableId != id) {
			g_map.stableId = id;
			const char* n = MapIdToName(id);
			if (n && (strcmp(g_map.stableName, "Unknown") == 0 || g_map.stableId == id))
				snprintf(g_map.stableName, sizeof(g_map.stableName), "%s", n);
		}
	} else {
		g_map.voteId = id;
		g_map.voteStreak = 1;
	}
}

static void UpdateMapDetection(uint64_t admin, uint64_t session, uint64_t world)
{
	char found[48] = {};
	if (ScanObjectForMapName(world, found, sizeof(found))) {
		CommitMapName(found, 0);
	} else if (ScanObjectForMapName(session, found, sizeof(found))) {
		CommitMapName(found, 0);
	} else if (ScanObjectForMapName(admin, found, sizeof(found))) {
		CommitMapName(found, 0);
	}

	uint32_t id = ProbeNamedMapId(world);
	if (!id) id = ProbeNamedMapId(session);
	if (!id) id = ProbeNamedMapId(admin);
	if (id) VoteMapId(id);

	if (strcmp(g_map.stableName, "Unknown") == 0 && g_map.stableId) {
		const char* n = MapIdToName(g_map.stableId);
		if (n) snprintf(g_map.stableName, sizeof(g_map.stableName), "%s", n);
	}
}

static uint32_t UpdateMatchState(uint64_t world, uint64_t session, uint64_t admin, uint32_t fallback)
{
	uint32_t best = UINT32_MAX;
	uint32_t cands[] = {
		ProbeMatchState(world),
		ProbeMatchState(session),
		ProbeMatchState(admin),
	};
	for (uint32_t c : cands) {
		if (c <= 6) { best = c; break; }
	}
	if (best <= 6) return best;
	return fallback;
}

void UpdateGameWorldInfo()
{
	if (!SDK) return;

	static float    s_stableTime = -1.f;
	static uint32_t s_stableState = 0;
	static uint64_t s_lastSecond = 0;
	static char     s_stablePhase[32] = "Lobby";

	GameWorldInfo info = {};
	info.readFov = SDK->RPM<float>(SDK->dwGameBase + offset::Fov_Changer_RVA);

	uint64_t admin = SDK->RPM<uint64_t>(SDK->dwGameBase + offset::GlobalAdmin_WorldBz_RVA);
	info.adminPtr = admin;
	uint64_t session = 0;
	uint64_t world = 0;

	if (IsValidPtr(admin)) {
		session = SDK->RPM<uint64_t>(admin + 0x58);
		if (!IsValidPtr(session)) session = SDK->RPM<uint64_t>(admin + 0x60);
		if (!IsValidPtr(session)) session = SDK->RPM<uint64_t>(admin + 0x68);
		info.sessionPtr = session;

		world = SDK->RPM<uint64_t>(admin + 0x48);
		if (!IsValidPtr(world)) world = SDK->RPM<uint64_t>(admin + 0x40);
		if (!IsValidPtr(world)) world = SDK->RPM<uint64_t>(admin + 0x50);
		info.worldPtr = world;

		if (IsValidPtr(world)) {
			info.gameMode = SDK->RPM<uint32_t>(world + 0x2C);
			uint32_t rawState = UpdateMatchState(world, session, admin, s_stableState);
			if (rawState <= 6) info.matchState = rawState;
			else info.matchState = s_stableState;

			float rt = SDK->RPM<float>(world + 0x38);
			float gt = SDK->RPM<float>(world + 0x34);
			float pick = rt;
			if (pick < 0.f || pick > 900.f || isnan(pick))
				pick = gt;
			if (pick >= 0.f && pick <= 900.f && !isnan(pick))
				info.gameTime = pick;
		}

		UpdateStablePing(session, admin, world);
		UpdateMapDetection(admin, session, world);

		if (IsValidPtr(session) && !info.regionId) {
			uint32_t rids[] = {
				SDK->RPM<uint32_t>(session + 0x40),
				SDK->RPM<uint32_t>(session + 0x44),
				SDK->RPM<uint32_t>(session + 0x48),
				SDK->RPM<uint32_t>(admin + 0x70),
			};
			for (uint32_t r : rids) {
				if (r >= 1 && r <= 8) { info.regionId = r; break; }
			}
		}
	}

	for (auto& e : g_entities) {
		if (e.health_max > 1.f && e.health <= 0.f) continue;
		if (IsEnemyEntity(e)) info.enemyCount++;
		else if (!e.isLocal) info.allyCount++;
	}

	info.mapId = g_map.stableId;
	snprintf(info.mapName, sizeof(info.mapName), "%s", g_map.stableName);

	if (info.matchState <= 6) {
		s_stableState = info.matchState;
		const char* phase = MatchStateToPhase(info.matchState);
		if (phase) snprintf(s_stablePhase, sizeof(s_stablePhase), "%s", phase);
	}
	info.matchState = s_stableState;
	snprintf(info.phaseName, sizeof(info.phaseName), "%s", s_stablePhase);

	uint64_t nowMs = GetTickCount64();
	if (nowMs - s_lastSecond >= 1000 || s_lastSecond == 0) {
		s_lastSecond = nowMs;
		if (info.gameTime >= 0.f && info.gameTime <= 900.f) {
			if (s_stableTime < 0.f) {
				s_stableTime = info.gameTime;
			} else {
				float diff = fabsf(info.gameTime - s_stableTime);
				if (diff <= 3.f || info.gameTime <= s_stableTime || diff > 25.f)
					s_stableTime = info.gameTime;
				else if (s_stableState == 3)
					s_stableTime = fmaxf(0.f, s_stableTime - 1.f);
			}
		} else if (s_stableTime > 0.f && s_stableState == 3) {
			s_stableTime = fmaxf(0.f, s_stableTime - 1.f);
		}
	}
	info.gameTime = s_stableTime >= 0.f ? s_stableTime : 0.f;
	info.roundTime = info.gameTime;

	FormatGameTime(info.gameTime >= 0.f ? info.gameTime : 0.f, info.timeText, sizeof(info.timeText));

	const char* region = info.regionId ? RegionIdToName(info.regionId) : nullptr;
	if (region) snprintf(info.regionName, sizeof(info.regionName), "%s", region);
	else if (info.regionId) snprintf(info.regionName, sizeof(info.regionName), "R%u", info.regionId);
	else snprintf(info.regionName, sizeof(info.regionName), "?");

	ApplyStablePing(info);
	g_worldInfo = info;
}

void DrawEnemyHpBar(ImDrawList* dl, const EntityInfo& e)
{
	if (!g_drawHpBars || !e.screenValid) return;

	float top, bot, left;
	if (e.boxValid) {
		top = e.boxT; bot = e.boxB; left = e.boxL;
	} else if (e.boneCount > 0 && e.boneScreen[0].x >= 0) {
		top = e.boneScreen[0].y - 40.f;
		bot = e.boneScreen[0].y + 20.f;
		left = e.boneScreen[0].x - 20.f;
	} else return;

	float barH = bot - top;
	if (barH < 8.f) return;

	float maxTotal = e.health_max > 1.f ? e.health_max : 100.f;
	if (maxTotal < 1.f) maxTotal = 100.f;

	const float barW = 4.f;
	const float bx = left - barW - 3.f;

	dl->AddRectFilled(ImVec2(bx - 1, top - 1), ImVec2(bx + barW + 1, bot + 1), IM_COL32(0, 0, 0, 180), 1.f);
	dl->AddRectFilled(ImVec2(bx, top), ImVec2(bx + barW, bot), IM_COL32(30, 30, 30, 200), 1.f);

	float y = bot;
	auto drawSeg = [&](float cur, ImU32 col) {
		if (cur <= 0.f || maxTotal <= 0.f) return;
		float h = barH * (cur / maxTotal);
		if (h < 0.5f) return;
		y -= h;
		dl->AddRectFilled(ImVec2(bx, y), ImVec2(bx + barW, y + h), col, 1.f);
	};

	drawSeg(e.hp, IM_COL32(80, 220, 90, 230));
	drawSeg(e.armor, IM_COL32(240, 200, 50, 230));
	drawSeg(e.barrier, IM_COL32(80, 180, 255, 230));
}

void DrawGameInfoOverlay(ImDrawList* dl)
{
	if (!g_showGameInfo) return;

	ImFont* fnt = g_defaultFont ? g_defaultFont : ImGui::GetFont();
	float fsz = fnt->FontSize;
	float x = 10.f, y = g_screenH * 0.5f - 90.f;
	const float lh = fsz + 3.f;

	auto Line = [&](ImU32 col, const char* txt) {
		dl->AddText(fnt, fsz, ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 200), txt);
		dl->AddText(fnt, fsz, ImVec2(x, y), col, txt);
		y += lh;
	};

	char buf[128];
	dl->AddRectFilled(ImVec2(x - 6, y - 4), ImVec2(x + 280, y + lh * 14 + 4),
		IM_COL32(10, 10, 14, 190), 4.f);

	Line(IM_COL32(120, 220, 255, 255), S("-- Match Info --"));
	snprintf(buf, sizeof(buf), S("Map: %s"), g_worldInfo.mapName);
	Line(IM_COL32(220, 220, 220, 240), buf);
	snprintf(buf, sizeof(buf), S("Phase: %s  Time: %s"), g_worldInfo.phaseName, g_worldInfo.timeText);
	Line(IM_COL32(180, 180, 180, 230), buf);
	if (g_worldInfo.pingValid)
		snprintf(buf, sizeof(buf), S("Ping: %ums  Region: %s"), g_worldInfo.pingMs, g_worldInfo.regionName);
	else
		snprintf(buf, sizeof(buf), S("Ping: ?  Region: %s"), g_worldInfo.regionName);
	Line(IM_COL32(180, 180, 180, 230), buf);
	snprintf(buf, sizeof(buf), S("Mode: %u  Allies: %d  Enemies: %d"),
		g_worldInfo.gameMode, g_worldInfo.allyCount, g_worldInfo.enemyCount);
	Line(IM_COL32(180, 180, 180, 230), buf);

	const char* teamSrc = g_teamOverride ? S("manual") : S("auto");
	snprintf(buf, sizeof(buf), S("Team: %d (0x%X) [%s]"),
		GetEffectiveLocalTeam(), GetEffectiveLocalTeamMask(), teamSrc);
	Line(IM_COL32(255, 220, 100, 240), buf);
	snprintf(buf, sizeof(buf), S("Local UID: %u"), g_localPlayerUid);
	Line(IM_COL32(180, 180, 180, 220), buf);
	snprintf(buf, sizeof(buf), S("Entities: %zu  VM: %s  FOV: %.0f"),
		g_entities.size(), g_debug.vm_ok ? S("OK") : S("BAD"), g_worldInfo.readFov);
	Line(IM_COL32(180, 180, 180, 220), buf);
	snprintf(buf, sizeof(buf), S("Admin: 0x%llX"), (unsigned long long)g_worldInfo.adminPtr);
	Line(IM_COL32(140, 140, 140, 200), buf);
}
