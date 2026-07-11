#pragma once
#include "sdk.hpp"
#include "utils.hpp"
#include <cmath>
#include <unordered_map>

struct EspSmoothEntry {
    float2 bones[18] = {};
    float2 boxCorners[8] = {};
    float  boxL = 0, boxT = 0, boxR = 0, boxB = 0;
    bool   init = false;
    bool   boxInit = false;
};

extern std::unordered_map<uint32_t, EspSmoothEntry> g_espSmooth;
extern std::unordered_map<uint32_t, float3> g_espWorldPos;
extern std::unordered_map<uint32_t, float> g_espWorldRotY;

inline void ResetEspSmoothCaches()
{
	g_espSmooth.clear();
	g_espWorldPos.clear();
	g_espWorldRotY.clear();
}

inline void PruneEspSmooth()
{
    std::unordered_map<uint32_t, bool> live;
    for (auto& e : g_entities)
        if (e.uid) live[e.uid] = true;
    for (auto it = g_espSmooth.begin(); it != g_espSmooth.end(); ) {
        if (!live.count(it->first)) it = g_espSmooth.erase(it);
        else ++it;
    }
}

inline float2 SmoothScreenPoint(EspSmoothEntry& s, int boneIdx, float2 raw)
{
    float2& out = s.bones[boneIdx];
    if (!s.init) {
        out.x = roundf(raw.x * 2.f) * 0.5f;
        out.y = roundf(raw.y * 2.f) * 0.5f;
        return out;
    }
    float dx = raw.x - out.x, dy = raw.y - out.y;
    float d2 = dx * dx + dy * dy;
    float t = (d2 > 100.f) ? 0.85f : (d2 > 16.f) ? 0.45f : 0.18f;
    out.x += dx * t;
    out.y += dy * t;
    out.x = roundf(out.x * 2.f) * 0.5f;
    out.y = roundf(out.y * 2.f) * 0.5f;
    return out;
}

inline void SmoothEntityScreen(EntityInfo& e)
{
    if (!e.uid || !e.screenValid) return;
    EspSmoothEntry& s = g_espSmooth[e.uid];

    for (int bi = 0; bi < e.boneCount; bi++) {
        if (e.boneScreen[bi].x < 0.f) continue;
        float2 sm = SmoothScreenPoint(s, bi, e.boneScreen[bi]);
        e.boneScreen[bi] = sm;
    }

    if (e.boxValid) {
        if (!s.init) {
            s.boxL = e.boxL; s.boxT = e.boxT; s.boxR = e.boxR; s.boxB = e.boxB;
        } else {
            auto lerp = [&](float& cur, float raw, float t) {
                cur += (raw - cur) * t;
                cur = roundf(cur * 2.f) * 0.5f;
            };
            float cx = (e.boxL + e.boxR) * 0.5f;
            float cy = (e.boxT + e.boxB) * 0.5f;
            float scx = (s.boxL + s.boxR) * 0.5f;
            float scy = (s.boxT + s.boxB) * 0.5f;
            float dx = cx - scx, dy = cy - scy;
            float d2 = dx * dx + dy * dy;
            float t = (d2 > 100.f) ? 0.85f : (d2 > 16.f) ? 0.45f : 0.18f;
            lerp(s.boxL, e.boxL, t);
            lerp(s.boxT, e.boxT, t);
            lerp(s.boxR, e.boxR, t);
            lerp(s.boxB, e.boxB, t);
        }
        e.boxL = s.boxL; e.boxT = s.boxT; e.boxR = s.boxR; e.boxB = s.boxB;
    }

    s.init = true;
}

inline float3 SmoothEntityWorldPos(uint32_t uid, float3 raw)
{
    EspSmoothEntry& s = g_espSmooth[uid];
    float3& cur = g_espWorldPos[uid];
    if (!s.init) {
        cur = raw;
        return cur;
    }
    auto lerp1 = [&](float& c, float r) {
        c += (r - c) * 0.22f;
        c = roundf(c * 20.f) / 20.f;
    };
    lerp1(cur.x, raw.x);
    lerp1(cur.y, raw.y);
    lerp1(cur.z, raw.z);
    return cur;
}

inline bool BuildOrientedPlayerBox(const EntityInfo& e, float3 corners[8])
{
    const float halfW = 0.42f;
    const float halfD = 0.42f;
    const float height = 1.85f;
    float3 base = SmoothEntityWorldPos(e.uid, e.pos);
    float footY = base.y;
    float headY = footY + height;
    float cx = base.x;
    float cz = base.z;
    float ry = e.rot_y;

    float3 local[8] = {
        { -halfW, footY, -halfD }, { halfW, footY, -halfD },
        { halfW, footY,  halfD }, { -halfW, footY,  halfD },
        { -halfW, headY, -halfD }, { halfW, headY, -halfD },
        { halfW, headY,  halfD }, { -halfW, headY,  halfD },
    };
    for (int i = 0; i < 8; i++) {
        float3 r = rotate_y(local[i], ry);
        corners[i] = { r.x + cx, r.y, r.z + cz };
    }
    return true;
}

inline float SmoothEntityRotY(uint32_t uid, float raw)
{
    EspSmoothEntry& s = g_espSmooth[uid];
    float& cur = g_espWorldRotY[uid];
    if (!s.init) { cur = raw; return raw; }
    float diff = raw - cur;
    while (diff > 3.14159265f) diff -= 6.2831853f;
    while (diff < -3.14159265f) diff += 6.2831853f;
    cur += diff * 0.18f;
    return cur;
}

inline bool BuildBoneAabbBox(const EntityInfo& e, float3 corners[8])
{
    if (!e.hasBones || e.boneCount <= 0) return false;
    float3 mn = { 1e9f, 1e9f, 1e9f }, mx = { -1e9f, -1e9f, -1e9f };
    int used = 0;
    for (int bi = 0; bi < e.boneCount; bi++) {
        const float3& b = e.bones[bi];
        if (b.x == 0.f && b.y == 0.f && b.z == 0.f) continue;
        if (b.x < mn.x) mn.x = b.x; if (b.y < mn.y) mn.y = b.y; if (b.z < mn.z) mn.z = b.z;
        if (b.x > mx.x) mx.x = b.x; if (b.y > mx.y) mx.y = b.y; if (b.z > mx.z) mx.z = b.z;
        used++;
    }
    if (used < 3) return false;
    const float padXZ = 0.15f, padY = 0.12f;
    mn.x -= padXZ; mn.z -= padXZ; mx.x += padXZ; mx.z += padXZ;
    mn.y -= padY;  mx.y += padY;
    corners[0] = { mn.x, mn.y, mn.z };
    corners[1] = { mx.x, mn.y, mn.z };
    corners[2] = { mx.x, mn.y, mx.z };
    corners[3] = { mn.x, mn.y, mx.z };
    corners[4] = { mn.x, mx.y, mn.z };
    corners[5] = { mx.x, mx.y, mn.z };
    corners[6] = { mx.x, mx.y, mx.z };
    corners[7] = { mn.x, mx.y, mx.z };
    return true;
}

inline void DrawWorldBoxEdges(ImDrawList* dl, const float2 sc[8], ImU32 lineCol, ImU32 shadowCol)
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

inline void StabilizeWorldBoxScreen(uint32_t uid, const float2 raw[8], float2 out[8])
{
    EspSmoothEntry& s = g_espSmooth[uid];
    if (!s.boxInit) {
        for (int i = 0; i < 8; i++) {
            s.boxCorners[i].x = roundf(raw[i].x * 2.f) * 0.5f;
            s.boxCorners[i].y = roundf(raw[i].y * 2.f) * 0.5f;
            out[i] = s.boxCorners[i];
        }
        s.boxInit = true;
        return;
    }
    for (int i = 0; i < 8; i++) {
        float dx = raw[i].x - s.boxCorners[i].x;
        float dy = raw[i].y - s.boxCorners[i].y;
        float d2 = dx * dx + dy * dy;
        float t = (d2 > 100.f) ? 0.85f : (d2 > 16.f) ? 0.45f : 0.18f;
        s.boxCorners[i].x += dx * t;
        s.boxCorners[i].y += dy * t;
        s.boxCorners[i].x = roundf(s.boxCorners[i].x * 2.f) * 0.5f;
        s.boxCorners[i].y = roundf(s.boxCorners[i].y * 2.f) * 0.5f;
        out[i] = s.boxCorners[i];
    }
}
