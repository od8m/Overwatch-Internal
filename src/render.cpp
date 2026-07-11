#include "render.hpp"
#include "memory.hpp"
#include "utils.hpp"
#include "offsets.hpp"
#include "overlaychams.h"
#include "heroicons.h"
#include "ulttracker.h"
#include "features.hpp"
#include "config.h"
#include "rs_menu.h"
#include "radar.h"
#include "offscreen.h"
#include "gametracker.h"
#include "esp_smooth.h"
#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx11.h"
#include <cfloat>
#include <unordered_map>
#include <unordered_set>

std::unordered_map<uint32_t, EspSmoothEntry> g_espSmooth;
std::unordered_map<uint32_t, float3> g_espWorldPos;
std::unordered_map<uint32_t, float> g_espWorldRotY;

void RenderMenu()
{
	DrawRsGui();
}

void UpdateESPLogic() {}

void UpdateAimbot() {}

void UpdateScreenPositions() {
    PruneEspSmooth();
    for (auto& e : g_entities) {
        e.screenValid = false;
        e.boxValid    = false;
        if (e.isLocal) continue;

        float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
        bool anyVis = false;

        if (e.boneCount > 0) {
            for (int bi = 0; bi < e.boneCount; bi++) {
                float sx, sy;
                if (WorldToScreen(e.bones[bi], sx, sy)) {
                    e.boneScreen[bi] = { sx, sy };
                    if (sx < minX) minX = sx;
                    if (sy < minY) minY = sy;
                    if (sx > maxX) maxX = sx;
                    if (sy > maxY) maxY = sy;
                    anyVis = true;
                } else {
                    e.boneScreen[bi] = { -1.f, -1.f };
                }
            }
        } else {
            float sx1, sy1, sx2, sy2;
            float3 head = { e.pos.x, e.pos.y + 1.7f, e.pos.z };
            if (WorldToScreen(e.pos, sx1, sy1) && WorldToScreen(head, sx2, sy2)) {
                float w = fabsf(sy1 - sy2) * 0.45f;
                minX = sx1 - w; maxX = sx1 + w;
                minY = sy2;     maxY = sy1;
                anyVis = true;
            }
        }

        if (!anyVis) continue;
        e.screenValid = true;
        e.minX = minX; e.minY = minY; e.maxX = maxX; e.maxY = maxY;

        if (e.boneCount > 0) {
            float padX = (maxX - minX) * 0.08f + 2.f;
            float padY = (maxY - minY) * 0.06f + 2.f;
            if (padX < 3.f) padX = 3.f;
            if (padY < 3.f) padY = 3.f;
            e.boxT = minY - padY;
            e.boxB = maxY + padY * 0.35f;
            float cx = (minX + maxX) * 0.5f;
            float w  = (maxX - minX) + padX * 2.f;
            e.boxL = cx - w * 0.5f;
            e.boxR = cx + w * 0.5f;
            e.boxValid = true;
        }

        SmoothEntityScreen(e);
    }
}

static void DrawEntList(ImDrawList* dl) {
    if (!g_drawEntList) return;
    float x = 8.f, y = 8.f;
    const float lh = 14.f;

    ImFont* fnt = g_defaultFont ? g_defaultFont : ImGui::GetFont();
    float fsz = fnt->FontSize;
    auto Line = [&](ImU32 col, const char* txt) {
        dl->AddText(fnt, fsz, ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 210), txt);
        dl->AddText(fnt, fsz, ImVec2(x, y), col, txt);
        y += lh;
    };

    char buf[160];
    snprintf(buf, sizeof(buf), S("[DBG] Entities:%zu Visible:%u VM:%s Team:%d"),
        g_entities.size(), g_debug.screen_valid_count, g_debug.vm_ok ? S("OK") : S("BAD"), g_localTeam);
    Line(IM_COL32(0, 230, 255, 255), buf);

    int idx = 0, shown = 0;
    for (auto& e : g_entities) {
        if (e.isLocal) {
            snprintf(buf, sizeof(buf), S("[%02d] LOCAL|HP:%.0f/%.0f"), idx, e.health, e.health_max);
            Line(IM_COL32(255, 220, 90, 255), buf);
            idx++;
            continue;
        }
        bool ally = (g_localTeam != 0 && e.team == g_localTeam);
        float3 hw = (e.boneCount > 0) ? e.bones[0] : e.pos;
        float hsx = (e.boneCount > 0) ? e.boneScreen[0].x : -1.f;
        float hsy = (e.boneCount > 0) ? e.boneScreen[0].y : -1.f;
        snprintf(buf, sizeof(buf),
            S("[%02d] %s|HP:%.0f/%.0f|Hero:0x%04X|Vis:%s|HeadS:(%.0f,%.0f)"),
            idx, ally ? S("ALY") : S("ENM"),
            e.health, e.health_max, (unsigned)e.heroId,
            e.visible ? S("Y") : S("N"), hsx, hsy);
        ImU32 col = ally ? IM_COL32(90, 220, 120, 255) : IM_COL32(255, 100, 100, 255);
        if (!e.screenValid) col = (col & 0x00FFFFFF) | (150u << 24);
        Line(col, buf);
        idx++;
        if (++shown >= 22) break;
    }
}

static void DrawWorldBox(ImDrawList* dl, const float2 sc[8], ImU32 lineCol, ImU32 shadowCol)
{
	static const int edges[12][2] = {
		{0,1},{1,2},{2,3},{3,0},{4,5},{5,6},{6,7},{7,4},{0,4},{1,5},{2,6},{3,7}
	};
	for (int i = 0; i < 12; i++) {
		dl->AddLine(ImVec2(sc[edges[i][0]].x, sc[edges[i][0]].y),
			ImVec2(sc[edges[i][1]].x, sc[edges[i][1]].y), shadowCol, 2.4f);
		dl->AddLine(ImVec2(sc[edges[i][0]].x, sc[edges[i][0]].y),
			ImVec2(sc[edges[i][1]].x, sc[edges[i][1]].y), lineCol, 1.2f);
	}
}

static void DrawWorldBox(ImDrawList* dl, const float3& center, float hx, float hy, float hz,
                         ImU32 lineCol, ImU32 shadowCol)
{
	float3 corners[8] = {
		{ center.x - hx, center.y,     center.z - hz },
		{ center.x + hx, center.y,     center.z - hz },
		{ center.x + hx, center.y,     center.z + hz },
		{ center.x - hx, center.y,     center.z + hz },
		{ center.x - hx, center.y + hy, center.z - hz },
		{ center.x + hx, center.y + hy, center.z - hz },
		{ center.x + hx, center.y + hy, center.z + hz },
		{ center.x - hx, center.y + hy, center.z + hz },
	};
	float2 sc[8];
	for (int i = 0; i < 8; i++) {
		if (!WorldToScreen(corners[i], sc[i].x, sc[i].y)) return;
	}
	DrawWorldBox(dl, sc, lineCol, shadowCol);
}

struct HpScreenCache {
	float2 corners[8];
	float2 label;
	bool   boxValid = false;
	bool   labelValid = false;
};
static std::unordered_map<uint64_t, HpScreenCache> s_hpScreen;

void ResetRenderCaches()
{
	s_hpScreen.clear();
}

static void PruneHpScreenCache()
{
	std::unordered_set<uint64_t> live;
	for (auto& hp : g_hpPacks) live.insert(hp.addr);
	for (auto it = s_hpScreen.begin(); it != s_hpScreen.end(); ) {
		if (!live.count(it->first)) it = s_hpScreen.erase(it);
		else ++it;
	}
}

static bool ProjectWorldBox(const float3& center, float hx, float hy, float hz, float2 out[8])
{
	float3 corners[8] = {
		{ center.x - hx, center.y,     center.z - hz },
		{ center.x + hx, center.y,     center.z - hz },
		{ center.x + hx, center.y,     center.z + hz },
		{ center.x - hx, center.y,     center.z + hz },
		{ center.x - hx, center.y + hy, center.z - hz },
		{ center.x + hx, center.y + hy, center.z - hz },
		{ center.x + hx, center.y + hy, center.z + hz },
		{ center.x - hx, center.y + hy, center.z + hz },
	};
	for (int i = 0; i < 8; i++) {
		if (!WorldToScreen(corners[i], out[i].x, out[i].y)) return false;
	}
	return true;
}

static float2 BoxCenter(const float2 sc[8])
{
	float2 c = {};
	for (int i = 0; i < 8; i++) { c.x += sc[i].x; c.y += sc[i].y; }
	c.x /= 8.f; c.y /= 8.f;
	return c;
}

static void StabilizeScreenBox(uint64_t id, const float2 raw[8], float2 out[8])
{
	float2 rawCenter = BoxCenter(raw);
	HpScreenCache& cache = s_hpScreen[id];
	if (!cache.boxValid) {
		for (int i = 0; i < 8; i++) {
			cache.corners[i].x = roundf(raw[i].x);
			cache.corners[i].y = roundf(raw[i].y);
			out[i] = cache.corners[i];
		}
		cache.boxValid = true;
		return;
	}

	float2 cacheCenter = BoxCenter(cache.corners);
	float dx = rawCenter.x - cacheCenter.x, dy = rawCenter.y - cacheCenter.y;
	float d2 = dx * dx + dy * dy;
	float t = (d2 > 100.f) ? 0.85f : (d2 > 25.f) ? 0.35f : 0.12f;
	for (int i = 0; i < 8; i++) {
		cache.corners[i].x += (raw[i].x - cache.corners[i].x) * t;
		cache.corners[i].y += (raw[i].y - cache.corners[i].y) * t;
		cache.corners[i].x = roundf(cache.corners[i].x * 2.f) * 0.5f;
		cache.corners[i].y = roundf(cache.corners[i].y * 2.f) * 0.5f;
	}
	for (int i = 0; i < 8; i++) out[i] = cache.corners[i];
}

static void StabilizeScreenPoint(uint64_t id, const float2& raw, float2& out)
{
	HpScreenCache& cache = s_hpScreen[id];
	if (!cache.labelValid) {
		out.x = roundf(raw.x);
		out.y = roundf(raw.y);
		cache.label = out;
		cache.labelValid = true;
		return;
	}
	float dx = raw.x - cache.label.x, dy = raw.y - cache.label.y;
	float d2 = dx * dx + dy * dy;
	float t = (d2 > 100.f) ? 0.85f : (d2 > 25.f) ? 0.35f : 0.12f;
	cache.label.x += dx * t;
	cache.label.y += dy * t;
	cache.label.x = roundf(cache.label.x * 2.f) * 0.5f;
	cache.label.y = roundf(cache.label.y * 2.f) * 0.5f;
	out = cache.label;
}

static void DrawHeadDot(ImDrawList* dl, const EntityInfo& e)
{
	if (!g_drawHeadDot || !e.screenValid) return;
	float sx, sy;
	float3 head = (e.boneCount > 0) ? e.bones[0] : float3{ e.pos.x, e.pos.y + 1.75f, e.pos.z };
	if (!WorldToScreen(head, sx, sy)) return;
	float r = g_headDotSize;
	if (r < 1.f) r = 1.f;
	dl->AddCircleFilled(ImVec2(sx, sy), r + 1.f, IM_COL32(0, 0, 0, 180), 12);
	dl->AddCircleFilled(ImVec2(sx, sy), r, IM_COL32(255, 255, 255, 240), 12);
}

static void DrawFacingLine(ImDrawList* dl, const EntityInfo& e)
{
	if (!g_drawFacingLine) return;
	float3 head = (e.boneCount > 0) ? e.bones[0] : float3{ e.pos.x, e.pos.y + 1.75f, e.pos.z };
	float len = g_facingLineLen;
	if (len < 0.5f) len = 0.5f;
	float3 end = {
		head.x + sinf(e.rot_y) * len,
		head.y,
		head.z + cosf(e.rot_y) * len
	};
	float sx1, sy1, sx2, sy2;
	if (!WorldToScreen(head, sx1, sy1) || !WorldToScreen(end, sx2, sy2)) return;
	dl->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2), IM_COL32(0, 0, 0, 180), 2.6f);
	dl->AddLine(ImVec2(sx1, sy1), ImVec2(sx2, sy2), IM_COL32(255, 220, 80, 230), 1.4f);
}

static void DrawHpPacks(ImDrawList* dl) {
    if (!g_drawHpPacks) return;
    PruneHpScreenCache();
    for (auto& hp : g_hpPacks) {
        const float3& c = hp.stablePos;
        float hx = 0.32f, hy = 0.22f, hz = 0.32f;
        const char* txt = S("HP");
        ImU32 lineCol = IM_COL32(80, 255, 120, 230);
        ImU32 txtCol  = IM_COL32(120, 255, 150, 255);
        if (hp.entityId == 0x40000000000480AULL) {
            txt = S("MEGA"); hx = 0.48f; hy = 0.32f; hz = 0.48f;
            lineCol = IM_COL32(255, 200, 80, 230);
            txtCol  = IM_COL32(255, 220, 120, 255);
        } else if (hp.entityId == 0x40000000000005FULL) {
            txt = S("PACK"); hx = 0.38f; hy = 0.26f; hz = 0.38f;
            lineCol = IM_COL32(120, 200, 255, 230);
            txtCol  = IM_COL32(160, 220, 255, 255);
        }

        if (g_drawHpPackBoxes) {
            float2 rawSc[8], drawSc[8];
            if (!ProjectWorldBox(c, hx, hy, hz, rawSc)) continue;
            StabilizeScreenBox(hp.addr, rawSc, drawSc);
            DrawWorldBox(dl, drawSc, lineCol, IM_COL32(0, 0, 0, 160));
        } else if (!g_drawHpPackLabels) {
            continue;
        }

        if (!g_drawHpPackLabels) continue;

        float2 rawLabel, drawLabel;
        float bottomY = -1e9f;
        float centerX = 0.f;
        {
            float2 sc[8];
            if (ProjectWorldBox(c, hx, hy, hz, sc)) {
                for (int i = 0; i < 8; i++) {
                    centerX += sc[i].x;
                    if (sc[i].y > bottomY) bottomY = sc[i].y;
                }
                centerX *= 0.125f;
            } else {
                float3 labelPos = { c.x, c.y + hy + 0.35f, c.z };
                if (!WorldToScreen(labelPos, rawLabel.x, rawLabel.y)) continue;
                StabilizeScreenPoint(hp.addr, rawLabel, drawLabel);
                centerX = drawLabel.x;
                bottomY = drawLabel.y;
            }
        }
        if (bottomY > -1e8f) {
            rawLabel = { centerX, bottomY + g_hpPackLabelOffset };
            StabilizeScreenPoint(hp.addr, rawLabel, drawLabel);
            ImVec2 ts = ImGui::CalcTextSize(txt);
            dl->AddText(ImVec2(drawLabel.x - ts.x * 0.5f + 1, drawLabel.y + 1), IM_COL32(0, 0, 0, 200), txt);
            dl->AddText(ImVec2(drawLabel.x - ts.x * 0.5f, drawLabel.y), txtCol, txt);
        }
    }
}

static bool IsPlayerAlive(const EntityInfo& e)
{
    if (e.health_max > 0.f && e.health <= 0.f) return false;
    return true;
}

static void DrawHeroPortrait(ImDrawList* dl, const EntityInfo& e)
{
    if (!IsPlayerAlive(e)) return;
    if (!EntityHasPortrait(e)) return;

    ID3D11ShaderResourceView* icon = GetHeroIcon(e.heroId);

    float sx, sy;
    if (e.boxValid) {
        sx = (e.boxL + e.boxR) * 0.5f;
        sy = e.boxT;
    } else if (e.boneCount > 0 && e.boneScreen[0].x >= 0.f) {
        sx = e.boneScreen[0].x;
        sy = e.boneScreen[0].y;
    } else {
        float3 head = e.boneCount > 0
            ? e.bones[0]
            : float3{ e.pos.x, e.pos.y + 1.75f, e.pos.z };
        float3 anchor = { head.x, head.y + 0.55f, head.z };
        if (!WorldToScreen(anchor, sx, sy)) return;
    }

    float sz = g_heroIconSize;
    if (sz < 8.f) sz = 8.f;
    float aspect = GetHeroIconAspect(e.heroId);
    if (aspect <= 0.f) aspect = 1.f;
    float dw = sz, dh = sz;
    if (aspect > 1.f)      dh = sz / aspect;
    else if (aspect < 1.f) dw = sz * aspect;
    float ix = sx - dw * 0.5f;
    float iy = sy - dh - 8.f;
    dl->AddRectFilled(ImVec2(ix - 2, iy - 2), ImVec2(ix + dw + 2, iy + dh + 2), IM_COL32(0, 0, 0, 150), 4.f);
    dl->AddImageRounded((ImTextureID)icon, ImVec2(ix, iy), ImVec2(ix + dw, iy + dh),
        ImVec2(0, 0), ImVec2(1, 1), IM_COL32_WHITE, 4.f);
}

void DrawESP() {
    UpdateHeroTracker();
    auto* dl = ImGui::GetForegroundDrawList();

    DrawEntList(dl);
    DrawMatchTimer(dl);
    DrawGameAlerts(dl);
    DrawMarkerFovCircle(dl);
    DrawFovThreats(dl);
    DrawHpPacks(dl);
    DrawAllOverlayChams(dl);
    DrawRadar(dl);
    DrawOffscreenArrows(dl);
    DrawBehindWarning(dl);

    if (g_drawHeroIcon && g_heroIconHiddenOnly) {
        for (auto& e : g_entities) {
            if (e.isLocal) continue;
            if (!IsPlayerAlive(e)) continue;
            if (!IsEnemyEntity(e)) continue;
            if (e.visible) continue;
            DrawHeroPortrait(dl, e);
        }
    }

    if (!(g_drawBoxes || g_drawSkeleton || g_drawLines || g_draw3dBox || g_drawHeroIcon
        || g_drawHeroName || g_drawPlayerName || g_drawHeadDot || g_drawFacingLine || g_chams)) {
        if (g_drawHpBars) {
            for (auto& e : g_entities) {
                if (e.isLocal) continue;
                if (!IsPlayerAlive(e)) continue;
                if (!IsEnemyEntity(e)) continue;
                if (g_visCheck && !e.visible) continue;
                if (!e.screenValid) continue;
                DrawEnemyHpBar(dl, e);
                DrawDistanceEsp(dl, e);
            }
        }
        DrawUltBars(dl); DrawUltPanel(dl); DrawAllyUltPanel(dl);
        DrawGameInfoOverlay(dl);
        return;
    }

    for (auto& e : g_entities) {
        if (e.isLocal) continue;
        if (!IsPlayerAlive(e)) continue;
        if (!IsEnemyEntity(e)) continue;
        if (g_visCheck && !e.visible) continue;
        if (!e.screenValid) continue;

        if (g_drawHeroIcon && !g_heroIconHiddenOnly)
            DrawHeroPortrait(dl, e);

        if (g_drawSkeleton && e.hasBones) {
            for (int li = 0; li < 17; li++) {
                int i1 = g_boneLines[li][0], i2 = g_boneLines[li][1];
                if (i1 >= e.boneCount || i2 >= e.boneCount) continue;
                if (e.boneScreen[i1].x < 0 || e.boneScreen[i2].x < 0) continue;
                if ((e.bones[i1].x==0&&e.bones[i1].y==0&&e.bones[i1].z==0)||
                    (e.bones[i2].x==0&&e.bones[i2].y==0&&e.bones[i2].z==0)) continue;
                dl->AddLine(ImVec2(e.boneScreen[i1].x, e.boneScreen[i1].y),
                            ImVec2(e.boneScreen[i2].x, e.boneScreen[i2].y), IM_COL32(0,0,0,160), 3.0f);
                dl->AddLine(ImVec2(e.boneScreen[i1].x, e.boneScreen[i1].y),
                            ImVec2(e.boneScreen[i2].x, e.boneScreen[i2].y), IM_COL32(255,255,255,255), 1.8f);
            }
        }

        if (g_drawBoxes && e.boxValid) {
            float lft = e.boxL, top = e.boxT, rgt = e.boxR, bot = e.boxB;
            float cw  = (rgt - lft) * 0.20f;
            float ch  = (bot - top) * 0.20f;

            auto Corner = [&](float x1, float y1, float dx, float dy) {
                dl->AddLine(ImVec2(x1, y1), ImVec2(x1+dx, y1),    IM_COL32(0,0,0,200),       3.0f);
                dl->AddLine(ImVec2(x1, y1), ImVec2(x1, y1+dy),    IM_COL32(0,0,0,200),       3.0f);
                dl->AddLine(ImVec2(x1, y1), ImVec2(x1+dx, y1),    IM_COL32(255,255,255,255), 1.5f);
                dl->AddLine(ImVec2(x1, y1), ImVec2(x1, y1+dy),    IM_COL32(255,255,255,255), 1.5f);
            };
            Corner(lft, top, +cw, +ch);
            Corner(rgt, top, -cw, +ch);
            Corner(lft, bot, +cw, -ch);
            Corner(rgt, bot, -cw, -ch);
        }

        DrawEnemyHpBar(dl, e);
        DrawDistanceEsp(dl, e);
        DrawHeadDot(dl, e);
        DrawFacingLine(dl, e);

        if (g_drawHeroName || g_drawPlayerName) {
            float tx = e.boxValid ? (e.boxL + e.boxR) * 0.5f : e.boneScreen[0].x;
            float ty = e.boxValid ? e.boxT - 4.f : e.boneScreen[0].y - 18.f;
            if (tx >= 0) {
                const char* hero = (g_drawHeroName && e.heroId) ? _hname(e.heroId) : nullptr;
                if (hero) {
                    ImVec2 ts = ImGui::CalcTextSize(hero);
                    dl->AddText(ImVec2(tx - ts.x * 0.5f + 1, ty + 1), IM_COL32(0, 0, 0, 200), hero);
                    dl->AddText(ImVec2(tx - ts.x * 0.5f, ty), IM_COL32(255, 255, 255, 240), hero);
                    ty -= ts.y + 2.f;
                }
                if (g_drawPlayerName && e.playerName[0]) {
                    ImVec2 ts = ImGui::CalcTextSize(e.playerName);
                    dl->AddText(ImVec2(tx - ts.x * 0.5f + 1, ty + 1), IM_COL32(0, 0, 0, 200), e.playerName);
                    dl->AddText(ImVec2(tx - ts.x * 0.5f, ty), IM_COL32(180, 220, 255, 240), e.playerName);
                }
            }
        }

        if (g_drawLines) {
            float sx = (e.boxValid) ? (e.boxL + e.boxR) * 0.5f : e.boneScreen[0].x;
            float sy = (e.boxValid) ? e.boxB                     : e.boneScreen[0].y;
            if (sx >= 0)
                dl->AddLine(ImVec2(g_screenW/2, g_screenH/2), ImVec2(sx, sy), IM_COL32(255,255,255,200), 1.0f);
        }

        if (g_draw3dBox) {
            float3 corners[8];
            EntityInfo boxE = e;
            boxE.rot_y = SmoothEntityRotY(e.uid, e.rot_y);
            if (BuildOrientedPlayerBox(boxE, corners)) {
                float2 rawSc[8], drawSc[8];
                bool allVis = true;
                for (int i = 0; i < 8; i++) {
                    if (!WorldToScreen(corners[i], rawSc[i].x, rawSc[i].y)) { allVis = false; break; }
                }
                if (allVis) {
                    StabilizeWorldBoxScreen(e.uid, rawSc, drawSc);
                    DrawWorldBoxEdges(dl, drawSc, IM_COL32(255, 255, 255, 255), IM_COL32(0, 0, 0, 160));
                }
            }
        }
    }
    DrawUltBars(dl);
    DrawUltPanel(dl);
    DrawAllyUltPanel(dl);
    DrawGameInfoOverlay(dl);
}
