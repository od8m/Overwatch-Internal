#pragma once
#include "sdk.hpp"
#include "memory.hpp"
#include "utils.hpp"
#include "heroicons.h"
#include "abilityicons.h"
#include <vector>
#include <algorithm>
#include <cmath>

static inline const char* _hname(uint16_t id)
{
	switch (id) {
		case 0x0002: return S("Reaper");       case 0x0003: return S("Tracer");
		case 0x0004: return S("Mercy");        case 0x0005: return S("Hanzo");
		case 0x0006: return S("Torbjorn");     case 0x0007: return S("Reinhardt");
		case 0x0008: return S("Pharah");       case 0x0009: return S("Winston");
		case 0x000A: return S("Widowmaker");   case 0x0015: return S("Bastion");
		case 0x0016: return S("Symmetra");     case 0x0020: return S("Zenyatta");
		case 0x0029: return S("Genji");        case 0x0040: return S("Roadhog");
		case 0x0042: return S("Cassidy");      case 0x0065: return S("Junkrat");
		case 0x0068: return S("Zarya");        case 0x006E: return S("Soldier:76");
		case 0x0079: return S("Lucio");        case 0x007A: return S("D.Va");
		case 0x00DD: return S("Mei");          case 0x012E: return S("Sombra");
		case 0x012F: return S("Doomfist");     case 0x013B: return S("Ana");
		case 0x013E: return S("Orisa");        case 0x0195: return S("Brigitte");
		case 0x01A2: return S("Moira");        case 0x01CA: return S("Wrecking Ball");
		case 0x01EC: return S("Sojourn");      case 0x0200: return S("Ashe");
		case 0x0206: return S("Echo");         case 0x0221: return S("Baptiste");
		case 0x0231: return S("Kiriko");       case 0x0236: return S("Junker Queen");
		case 0x023B: return S("Sigma");        case 0x028D: return S("Ramattra");
		case 0x0291: return S("LifeWeaver");   case 0x030A: return S("Mauga");
		case 0x031C: return S("Illari");       case 0x032A: return S("Freja");
		case 0x032B: return S("Venture");      case 0x0362: return S("Hazard");
		case 0x0365: return S("Juno");         case 0x03C3: return S("Wuyang");
		case 0x0472: return S("Vendetta");     case 0x04C4: return S("Domina");
		case 0x04D8: return S("Emre");         case 0x04DD: return S("Anran");
		case 0x04E3: return S("Mizuki");       case 0x0516: return S("Jetpack Cat");
		default: return nullptr;
	}
}

static inline int _ult(uint64_t comp, uint64_t link)
{
	constexpr uint64_t	v9 = 96ULL;
	uint64_t			sc;
	uint64_t			v6;
	uint32_t			cnt;
	uint64_t			ptr;
	uint64_t			entry;
	uint64_t			skill;
	float				charge;
	uint32_t			i;
	int					uc;

	sc = DecryptComponent(comp, TYPE_SKILL);
	if (sc < 0x10000ULL || sc >= 0x800000000000ULL)
		sc = DecryptComponent(link, TYPE_SKILL);
	if (sc < 0x10000ULL || sc >= 0x800000000000ULL) return -1;

	uc = -1;
	__try {
		v6  = sc + 0x5B0;
		cnt = SDK->RPM<uint32_t>(v6 + v9 + 8);
		ptr = SDK->RPM<uint64_t>(v6 + v9);
		if (cnt > 0 && cnt < 512 && ptr > 0x10000ULL && ptr < 0x800000000000ULL) {
			entry = ptr + 16ULL * (cnt - 1);
			for (i = 0; i < cnt; i++, entry -= 16) {
				if (SDK->RPM<uint16_t>(entry) == 0x1e32) {
					skill = SDK->RPM<uint64_t>(entry + 8);
					if (skill > 0x10000ULL && skill < 0x800000000000ULL) {
						charge = SDK->RPM<float>(skill + 0x60);
						if (!isnan(charge) && charge >= 0.f) {
							if (charge <= 1.001f)
								uc = (int)(charge * 100.f + 0.5f);
							else if (charge <= 101.f)
								uc = (int)(charge + 0.5f);
						}
					}
					break;
				}
			}
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
	return uc;
}

inline void DrawUltBars(ImDrawList* dl)
{
	float		sx;
	float		sy;
	int			uc;
	float		pct;
	float		bx;
	float		by;
	uint32_t	col;

	if (!g_ultBars) return;
	for (auto& e : g_entities) {
		if (!IsEnemyEntity(e)) continue;
		if (!e.hasBones || e.health <= 0.f) continue;
		if (!e.heroId || !_hname(e.heroId)) continue;
		if (e.hasBones) {
			if (!WorldToScreen(e.bones[0], sx, sy)) continue;
		} else {
			if (!WorldToScreen(e.pos, sx, sy)) continue;
		}
		uc  = _ult(e.common, e.addr);
		if (uc < 0) continue;
		pct = uc / 100.f;
		bx  = sx - 20.f;
		by  = sy - 28.f;
		col = uc >= 100 ? IM_COL32(255, 60, 60, 230) : IM_COL32(255, 200, 50, 200);
		dl->AddRectFilled(ImVec2(bx, by),        ImVec2(bx+40.f, by+4.f),      IM_COL32(0,0,0,130));
		dl->AddRectFilled(ImVec2(bx, by),        ImVec2(bx+40.f*pct, by+4.f),  col);
		dl->AddRect      (ImVec2(bx, by),        ImVec2(bx+40.f, by+4.f),      IM_COL32(80,80,80,160));
	}
}

struct SkillSlot {
	uint16_t hash;
	uint64_t skillPtr;
	uint64_t entryPtr;
	float    cd;         // seconds remaining (interpreted)
	float    raw60;      // raw read for debug
};

struct CdSmoothState {
	float    displayCd = 0.f;
	float    lastRaw = 0.f;
	uint64_t lastMs = 0;
	uint64_t activeUntil = 0;
	float    snap[24] = {};
};
static std::unordered_map<uint64_t, CdSmoothState> g_cdSmooth;
static std::unordered_map<uint16_t, int> g_cdOffsetPref;

inline void ResetAbilityTrackerState()
{
	g_cdSmooth.clear();
	g_cdOffsetPref.clear();
}

static inline uint64_t _abilKey(uint32_t uid, uint16_t hash)
{
	return ((uint64_t)uid << 16) | hash;
}

static inline bool _looksLikeChargePercent(float v)
{
	if (isnan(v) || v < 0.f) return false;
	if (v <= 1.001f) return true;
	if (v <= 101.f && fabsf(v - roundf(v)) < 0.05f) return true;
	return false;
}

static inline bool _looksLikeGarbageCooldown(float v)
{
	if (isnan(v) || v <= 0.05f) return true;
	if (_looksLikeChargePercent(v)) return true;
	if (v > 45.f) return true;
	if (v >= 100.f && fabsf(v - roundf(v)) < 0.01f) return true;
	return false;
}

static inline float _normalizeCooldownSample(float v)
{
	if (_looksLikeGarbageCooldown(v)) return 0.f;
	if (v <= 45.f) return v;
	if (v <= 4500.f) {
		float sec = v * 0.01f;
		return sec <= 45.f ? sec : 0.f;
	}
	return 0.f;
}

static inline float _readSkillCooldown(uint64_t skill, uint64_t entry, uint16_t hash)
{
	if (!skill || hash == 0x1e32) return 0.f;

	auto trySample = [&](int off) -> float {
		if (off < 0 || off > 0xA0) return 0.f;
		return _normalizeCooldownSample(SDK->RPM<float>(skill + off));
	};
	auto tryEntry = [&](int off) -> float {
		if (!entry || off < 0 || off > 0x10) return 0.f;
		return _normalizeCooldownSample(SDK->RPM<float>(entry + off));
	};

	int pref = g_cdOffsetPref[hash];
	if (pref >= 0x100) {
		float v = tryEntry(pref & 0xFF);
		if (v > 0.f) return v;
	} else if (pref >= 0x40 && pref <= 0xA0) {
		float v = trySample(pref);
		if (v > 0.f) return v;
	}

	float best = 0.f;
	int bestOff = -1;
	bool bestEntry = false;
	for (int off = 0x40; off <= 0xA0; off += 4) {
		float v = trySample(off);
		if (v <= 0.f) continue;
		if (best <= 0.f || v < best) { best = v; bestOff = off; bestEntry = false; }
	}
	for (int off = 0; off <= 0x10; off += 4) {
		float v = tryEntry(off);
		if (v <= 0.f) continue;
		if (best <= 0.f || v < best) { best = v; bestOff = off; bestEntry = true; }
	}
	if (bestOff >= 0)
		g_cdOffsetPref[hash] = bestEntry ? (0x100 | bestOff) : bestOff;
	return best;
}

static inline float _smoothCooldown(uint32_t uid, uint16_t hash, float rawCd)
{
	uint64_t key = _abilKey(uid, hash);
	CdSmoothState& s = g_cdSmooth[key];
	uint64_t now = GetTickCount64();
	float dt = s.lastMs ? (float)(now - s.lastMs) / 1000.f : 0.f;
	s.lastMs = now;

	if (rawCd > 0.15f && rawCd <= 45.f) {
		if (s.lastRaw <= 0.15f && rawCd > 0.5f)
			s.activeUntil = now + 1200;
		if (s.lastRaw <= 0.15f || rawCd > s.displayCd + 0.75f)
			s.displayCd = rawCd;
		else if (dt > 0.f)
			s.displayCd = fmaxf(rawCd, s.displayCd - dt);
		s.lastRaw = rawCd;
	} else {
		if (s.displayCd > 0.15f && dt > 0.f)
			s.displayCd = fmaxf(0.f, s.displayCd - dt);
		else
			s.displayCd = 0.f;
		s.lastRaw = 0.f;
	}
	return s.displayCd;
}

static inline float _trackSkillCooldown(uint64_t skill, uint64_t entry, uint16_t hash, uint32_t uid)
{
	if (!skill || hash == 0x1e32) return 0.f;

	uint64_t key = _abilKey(uid, hash);
	CdSmoothState& st = g_cdSmooth[key];
	uint64_t now = GetTickCount64();
	float best = 0.f;
	int idx = 0;
	for (int off = 0x40; off <= 0x90; off += 4, idx++) {
		if (idx >= 24) break;
		float v = SDK->RPM<float>(skill + off);
		float prev = st.snap[idx];
		st.snap[idx] = v;
		float norm = _normalizeCooldownSample(v);
		if (norm > best) best = norm;
		if (prev <= 0.2f && norm >= 0.45f) {
			st.activeUntil = now + 1200;
			g_cdOffsetPref[hash] = off;
		}
	}

	float raw = _readSkillCooldown(skill, entry, hash);
	if (raw <= 0.f) raw = best;
	return _smoothCooldown(uid, hash, raw);
}

static inline void _drawCdOverlay(ImDrawList* dl, float ix, float iy, float abSz, float cdDisp)
{
	char buf[16];
	snprintf(buf, sizeof(buf), S("%.0f"), ceilf(cdDisp));
	ImFont* font = ImGui::GetFont();
	float fs = font ? (font->FontSize * 1.35f) : 16.f;
	ImVec2 ts = font ? font->CalcTextSizeA(fs, 1e9f, 0.f, buf) : ImGui::CalcTextSize(buf);
	float tx = ix + (abSz - ts.x) * 0.5f;
	float ty = iy + (abSz - ts.y) * 0.5f;

	dl->AddRectFilled(ImVec2(ix, iy), ImVec2(ix + abSz, iy + abSz), IM_COL32(48, 48, 52, 215), 3.f);
	dl->AddRect(ImVec2(ix, iy), ImVec2(ix + abSz, iy + abSz), IM_COL32(110, 110, 118, 240), 3.f, 0, 1.5f);
	if (font) {
		dl->AddText(font, fs, ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 220), buf);
		dl->AddText(font, fs, ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), buf);
	} else {
		dl->AddText(ImVec2(tx + 1.f, ty + 1.f), IM_COL32(0, 0, 0, 220), buf);
		dl->AddText(ImVec2(tx, ty), IM_COL32(255, 255, 255, 255), buf);
	}
}

static inline bool _abilityIsActive(uint32_t uid, uint16_t hash, float /*cdDisp*/)
{
	uint64_t now = GetTickCount64();
	const CdSmoothState& s = g_cdSmooth[_abilKey(uid, hash)];
	return now < s.activeUntil;
}

inline int EnumSkillSlots(uint64_t comp, uint64_t link, SkillSlot* out, int maxOut)
{
	uint64_t sc = DecryptComponent(comp, TYPE_SKILL);
	if (sc < 0x10000ULL || sc >= 0x800000000000ULL)
		sc = DecryptComponent(link, TYPE_SKILL);
	if (sc < 0x10000ULL || sc >= 0x800000000000ULL) return 0;

	int n = 0;
	__try {
		uint64_t v6  = sc + 0x5B0;
		uint32_t cnt = SDK->RPM<uint32_t>(v6 + 96 + 8);
		uint64_t ptr = SDK->RPM<uint64_t>(v6 + 96);
		if (cnt == 0 || cnt >= 512 || ptr <= 0x10000ULL || ptr >= 0x800000000000ULL)
			return 0;
		uint64_t entry = ptr;
		for (uint32_t i = 0; i < cnt && n < maxOut; i++, entry += 16) {
			uint16_t hash = SDK->RPM<uint16_t>(entry);
			if (!hash) continue;
			uint64_t skill = SDK->RPM<uint64_t>(entry + 8);
			if (skill <= 0x10000ULL || skill >= 0x800000000000ULL) continue;
			out[n].hash = hash;
			out[n].skillPtr = skill;
			out[n].entryPtr = entry;
			out[n].raw60 = SDK->RPM<float>(skill + 0x60);
			out[n].cd = _readSkillCooldown(skill, entry, hash);
			n++;
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) {}
	return n;
}

static inline int PickUiAbilities(SkillSlot* all, int n, SkillSlot* out4)
{
	int ultIdx = -1;
	for (int i = 0; i < n; i++)
		if (all[i].hash == 0x1e32) { ultIdx = i; break; }

	int picked[4] = {};
	int cnt = 0;
	if (ultIdx > 0) {
		for (int i = ultIdx - 1; i >= 0 && cnt < 4; i--) {
			if (all[i].hash == 0x1e32) continue;
			picked[cnt++] = i;
		}
		for (int i = 0; i < cnt / 2; i++) {
			int t = picked[i]; picked[i] = picked[cnt - 1 - i]; picked[cnt - 1 - i] = t;
		}
	}
	if (cnt < 4) {
		for (int i = 0; i < n && cnt < 4; i++) {
			if (all[i].hash == 0x1e32) continue;
			bool dup = false;
			for (int j = 0; j < cnt; j++)
				if (picked[j] == i) { dup = true; break; }
			if (!dup) picked[cnt++] = i;
		}
	}
	for (int i = 0; i < 4; i++)
		out4[i] = (i < cnt) ? all[picked[i]] : SkillSlot{};
	return cnt;
}

static inline int CollectAbilitySlots(const EntityInfo& e, SkillSlot* outAbils, int maxAbils)
{
	SkillSlot slots[32];
	int n = EnumSkillSlots(e.common, e.addr, slots, 32);
	SkillSlot ui4[4];
	int uiN = PickUiAbilities(slots, n, ui4);
	int count = uiN < maxAbils ? uiN : maxAbils;
	for (int i = 0; i < count; i++)
		outAbils[i] = ui4[i];
	return count;
}

static inline const SkillSlot* _skillForUiSlot(const SkillSlot* abils, int abilN, int uiSlot)
{
	if (uiSlot < 0 || uiSlot >= 4) return nullptr;
	if (uiSlot >= abilN || !abils[uiSlot].hash) return nullptr;
	return &abils[uiSlot];
}

static inline void DrawAbilityRowForEntity(ImDrawList* dl, const EntityInfo& e,
	float rowCenterY, float portraitLeftX, float abSz, float abGap, int ultPct)
{
	const int kTotalSlots = 5;
	char buf[32];
	const ImU32 kPink     = IM_COL32(255, 70, 190, 255);
	const ImU32 kPinkFill = IM_COL32(255, 80, 195, 200);

	SkillSlot abils[4];
	int abilN = CollectAbilitySlots(e, abils, 4);

	float ax = portraitLeftX - (abSz * kTotalSlots + abGap * (kTotalSlots - 1)) - 8.f;
	float abTop = rowCenterY - abSz * 0.5f;

	for (int si = 0; si < kTotalSlots; si++) {
		float ix = ax + si * (abSz + abGap);
		float iy = abTop;
		bool isUltSlot = (si == kTotalSlots - 1);
		const SkillSlot* sk = isUltSlot ? nullptr : _skillForUiSlot(abils, abilN, si);
		uint16_t hash = sk ? sk->hash : 0;
		float cdDisp = sk ? _trackSkillCooldown(sk->skillPtr, sk->entryPtr, hash, e.uid) : 0.f;
		bool isActive = sk && _abilityIsActive(e.uid, hash, cdDisp);
		bool onCd = sk && cdDisp > 0.25f;

		if (isActive) {
			dl->AddRectFilled(ImVec2(ix - 2.f, iy - 2.f), ImVec2(ix + abSz + 2.f, iy + abSz + 2.f),
				kPinkFill, 3.f);
			dl->AddRect(ImVec2(ix - 2.f, iy - 2.f), ImVec2(ix + abSz + 2.f, iy + abSz + 2.f), kPink, 3.f, 0, 2.5f);
		}

		ID3D11ShaderResourceView* abIcon = GetAbilityIconBySlot(e.heroId, si);
		if (sk && !abIcon) abIcon = GetAbilityIcon(e.heroId, hash);

		if (abIcon) {
			const float pad = 2.f;
			ImU32 tint = IM_COL32(255, 255, 255, 240);
			if (isUltSlot) {
				tint = ultPct >= 100 ? IM_COL32(120, 220, 255, 255)
					: IM_COL32(255, 255, 255, 220);
			} else if (onCd) {
				tint = IM_COL32(130, 130, 135, 180);
			} else if (isActive) {
				tint = IM_COL32(255, 255, 255, 255);
			}
			dl->AddImageRounded((ImTextureID)abIcon,
				ImVec2(ix + pad, iy + pad), ImVec2(ix + abSz - pad, iy + abSz - pad),
				ImVec2(0, 0), ImVec2(1, 1), tint, 2.f);
		} else {
			dl->AddRectFilled(ImVec2(ix, iy), ImVec2(ix + abSz, iy + abSz),
				IM_COL32(30, 30, 35, 160), 3.f);
			dl->AddRect(ImVec2(ix, iy), ImVec2(ix + abSz, iy + abSz),
				IM_COL32(255, 255, 255, 50), 3.f, 0, 1.f);
		}

		if (onCd)
			_drawCdOverlay(dl, ix, iy, abSz, cdDisp);

		if (g_abilityDebug && sk) {
			snprintf(buf, sizeof(buf), S("%04X"), hash);
			ImVec2 hs = ImGui::CalcTextSize(buf);
			dl->AddText(ImVec2(ix + (abSz - hs.x) * 0.5f, iy + abSz - hs.y - 1.f),
				IM_COL32(255, 255, 80, 220), buf);
		}
	}
}

inline void DrawAbilityPanel(ImDrawList* dl)
{
	(void)dl;
}

inline void DrawUltPanel(ImDrawList* dl)
{
	struct UltRow { const EntityInfo* ent; int ult; };

	if (!g_ultPanel && !g_abilityPanel) return;

	UltRow rows[12];
	int n = 0;
	uint32_t seenUid[12] = {};
	for (auto& e : g_entities) {
		if (e.isLocal) continue;
		if (!EntityHasPortrait(e)) continue;
		if (e.health_max > 1.f && e.health <= 0.f) continue;
		if (n >= 12) break;
		bool dup = false;
		for (int i = 0; i < n; i++)
			if (seenUid[i] == e.uid) { dup = true; break; }
		if (dup) continue;
		if (g_teamCheck && IsLocalIdentified() && !IsEnemyEntity(e)) continue;
		int uc = _ult(e.common, e.addr);
		if (uc < 0) uc = 0;
		seenUid[n] = e.uid;
		rows[n++] = { &e, uc };
	}
	if (!n) return;

	for (int i = 0; i < n - 1; i++)
		for (int j = i + 1; j < n; j++)
			if (rows[j].ult > rows[i].ult) {
				UltRow t = rows[i]; rows[i] = rows[j]; rows[j] = t;
			}

	const float portR  = 30.f;
	const float abSz   = 32.f;
	const float abGap  = 3.f;
	const float gap    = 10.f;
	const float step   = portR * 2.f + gap;
	const float margin = 16.f;
	const float cx     = g_screenW - margin - portR;
	const float startY = g_screenH * 0.5f - (n * step - gap) * 0.5f;
	const float kPi    = 3.14159265358979323846f;
	const int   segs   = 32;

	for (int i = 0; i < n; i++) {
		const UltRow& row = rows[i];
		float pct = row.ult / 100.f;
		if (pct > 1.f) pct = 1.f;
		if (pct < 0.f) pct = 0.f;

		float pcx = cx;
		float pcy = startY + i * step + portR;

		if (g_abilityPanel)
			DrawAbilityRowForEntity(dl, *row.ent, pcy, pcx - portR, abSz, abGap, row.ult);

		if (g_ultPanel) {
			ImU32 ringCol = (pct >= 1.f) ? IM_COL32(70, 210, 255, 230) : IM_COL32(220, 80, 90, 200);
			for (int s = 0; s < segs; s++) {
				float a0 = -kPi * 0.5f + (float)s / segs * kPi * 2.f;
				float a1 = -kPi * 0.5f + (float)(s + 1) / segs * kPi * 2.f;
				float t  = (float)(s + 1) / segs;
				ImU32 segCol = (t <= pct) ? ringCol : IM_COL32(255, 255, 255, 35);
				float rr = portR + 3.f;
				dl->AddLine(
					ImVec2(pcx + cosf(a0) * rr, pcy + sinf(a0) * rr),
					ImVec2(pcx + cosf(a1) * rr, pcy + sinf(a1) * rr),
					segCol, 2.5f);
			}
		}

		ID3D11ShaderResourceView* icon = GetHeroIcon(row.ent->heroId);
		if (icon) {
			float aspect = GetHeroIconAspect(row.ent->heroId);
			if (aspect <= 0.f) aspect = 1.f;
			float sz = portR * 1.85f;
			float dw = sz, dh = sz;
			if (aspect > 1.f) dh = sz / aspect;
			else if (aspect < 1.f) dw = sz * aspect;
			dl->AddImageRounded((ImTextureID)icon,
				ImVec2(pcx - dw * 0.5f, pcy - dh * 0.5f),
				ImVec2(pcx + dw * 0.5f, pcy + dh * 0.5f),
				ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 245), portR * 0.35f);
		}
	}
}

inline void DrawAllyUltPanel(ImDrawList* dl)
{
	struct UltRow { const EntityInfo* ent; int ult; };

	if (!g_allyUltPanel) return;

	UltRow rows[8];
	int n = 0;
	uint32_t seenUid[8] = {};
	for (auto& e : g_entities) {
		if (e.isLocal) continue;
		if (IsEnemyEntity(e)) continue;
		if (!EntityHasPortrait(e)) continue;
		if (e.health_max > 1.f && e.health <= 0.f) continue;
		if (n >= 8) break;
		bool dup = false;
		for (int i = 0; i < n; i++)
			if (seenUid[i] == e.uid) { dup = true; break; }
		if (dup) continue;
		int uc = _ult(e.common, e.addr);
		if (uc < 0) uc = 0;
		seenUid[n] = e.uid;
		rows[n++] = { &e, uc };
	}
	if (!n) return;

	const float portR  = 22.f;
	const float gap    = 8.f;
	const float step   = portR * 2.f + gap;
	const float cx     = 16.f + portR;
	const float startY = g_screenH * 0.5f - (n * step - gap) * 0.5f;
	const float kPi    = 3.14159265358979323846f;
	const int   segs   = 32;

	for (int i = 0; i < n; i++) {
		float pct = rows[i].ult / 100.f;
		if (pct > 1.f) pct = 1.f;
		float pcx = cx;
		float pcy = startY + i * step + portR;

		if (g_abilityPanel)
			DrawAbilityRowForEntity(dl, *rows[i].ent, pcy, pcx + portR, 24.f, 2.f, rows[i].ult);

		ImU32 ringCol = (pct >= 1.f) ? IM_COL32(80, 255, 200, 210) : IM_COL32(100, 180, 120, 180);
		for (int s = 0; s < segs; s++) {
			float a0 = -kPi * 0.5f + (float)s / segs * kPi * 2.f;
			float a1 = -kPi * 0.5f + (float)(s + 1) / segs * kPi * 2.f;
			float t  = (float)(s + 1) / segs;
			ImU32 segCol = (t <= pct) ? ringCol : IM_COL32(255, 255, 255, 30);
			float rr = portR + 3.f;
			dl->AddLine(
				ImVec2(pcx + cosf(a0) * rr, pcy + sinf(a0) * rr),
				ImVec2(pcx + cosf(a1) * rr, pcy + sinf(a1) * rr),
				segCol, 2.f);
		}

		ID3D11ShaderResourceView* icon = GetHeroIcon(rows[i].ent->heroId);
		if (icon) {
			float aspect = GetHeroIconAspect(rows[i].ent->heroId);
			if (aspect <= 0.f) aspect = 1.f;
			float sz = portR * 1.8f;
			float dw = sz, dh = sz;
			if (aspect > 1.f) dh = sz / aspect;
			else if (aspect < 1.f) dw = sz * aspect;
			dl->AddImageRounded((ImTextureID)icon,
				ImVec2(pcx - dw * 0.5f, pcy - dh * 0.5f),
				ImVec2(pcx + dw * 0.5f, pcy + dh * 0.5f),
				ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, 230), portR * 0.35f);
		}
	}
}
