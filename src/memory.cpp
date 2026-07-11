#include "memory.hpp"
#include "heroicons.h"
#include "shared_ipc.h"
#include <psapi.h>
#include <unordered_set>
#include <cstdio>
#include <cstring>
#include <string>
#include <unordered_map>
#include <cmath>

static float3 SnapWorldPos(float3 p)
{
    return {
        roundf(p.x * 20.f) / 20.f,
        roundf(p.y * 20.f) / 20.f,
        roundf(p.z * 20.f) / 20.f
    };
}

static bool ReadRemoteUtf16(uint64_t strPtr, char* out, int outSz)
{
    if (!IsValidPtr(strPtr)) return false;
    wchar_t w[40] = {};
    for (int i = 0; i < 39; i++) {
        w[i] = SDK->RPM<wchar_t>(strPtr + (uint64_t)i * 2);
        if (!w[i]) break;
        if (w[i] < 32 || w[i] > 0x7FFF) { out[0] = 0; return false; }
    }
    if (!w[0]) return false;
    WideCharToMultiByte(CP_UTF8, 0, w, -1, out, outSz, nullptr, nullptr);
    return out[0] != 0 && strlen(out) >= 2;
}

static bool ReadRemoteAscii(uint64_t strPtr, char* out, int outSz)
{
    if (!IsValidPtr(strPtr)) return false;
    for (int i = 0; i < outSz - 1; i++) {
        char c = SDK->RPM<char>(strPtr + i);
        if (!c) break;
        if (c < 32 || c > 126) { out[0] = 0; return false; }
        out[i] = c;
        out[i + 1] = 0;
    }
    return out[0] != 0 && strlen(out) >= 2;
}

static bool IsValidPlayerName(const char* s)
{
    if (!s || !s[0]) return false;
    if (strstr(s, "???")) return false;
    int q = 0, alnum = 0;
    for (const char* p = s; *p; p++) {
        if (*p == '?') q++;
        else if ((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
                 (*p >= '0' && *p <= '9') || *p == '-' || *p == '_' || *p == '#')
            alnum++;
    }
    return alnum >= 2 && q < (int)strlen(s) / 2;
}

static void ScanNameFromBase(uint64_t base, char* out, int outSz)
{
    if (!IsValidPtr(base)) return;
    static const int ptrOffs[] = {
        0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8, 0xC0, 0xC8,
        0xD0, 0xD8, 0xE0, 0xE8, 0xF0, 0xF8, 0x100, 0x108, 0x110, 0x118,
        0x120, 0x128, 0x130, 0x138, 0x140, 0x148, 0x150, 0x158, 0x160,
        0x168, 0x170, 0x178, 0x180, 0x188, 0x190, 0x198, 0x1A0, 0x1A8,
        0x1B0, 0x1B8, 0x1C0, 0x1C8, 0x1D0, 0x1D8, 0x1E0, 0x1E8, 0x1F0,
        0x200, 0x208, 0x210, 0x218, 0x220, 0x228, 0x230, 0x238, 0x240,
        0x248, 0x250, 0x258, 0x260, 0x268, 0x270, 0x278, 0x280, 0x288,
        0x290, 0x298, 0x2A0, 0x2A8, 0x2B0, 0x2B8, 0x2C0, 0x2C8, 0x2D0
    };
    for (int po : ptrOffs) {
        uint64_t p = SDK->RPM<uint64_t>(base + po);
        if (ReadRemoteUtf16(p, out, outSz) && IsValidPlayerName(out)) return;
        if (ReadRemoteAscii(p, out, outSz) && IsValidPlayerName(out)) return;
        if (IsValidPtr(p)) {
            uint64_t p2 = SDK->RPM<uint64_t>(p);
            if (ReadRemoteUtf16(p2, out, outSz) && IsValidPlayerName(out)) return;
            if (ReadRemoteAscii(p2, out, outSz) && IsValidPlayerName(out)) return;
            if (IsValidPtr(p2)) {
                uint64_t p3 = SDK->RPM<uint64_t>(p2);
                if (ReadRemoteUtf16(p3, out, outSz) && IsValidPlayerName(out)) return;
                if (ReadRemoteAscii(p3, out, outSz) && IsValidPlayerName(out)) return;
            }
        }
    }
}

static void TryReadPlayerName(uint64_t lnk, uint64_t pc, uint64_t common, char* out, int outSz);

static std::unordered_map<uint32_t, std::pair<std::string, uint64_t>> g_playerNameCache;
static std::unordered_map<uint64_t, float3> s_hpStableCache;

static void GetPlayerNameCached(uint32_t uid, uint64_t lnk, uint64_t pc, uint64_t common, char* out, int outSz)
{
    out[0] = 0;
    if (!lnk && !pc && !common) return;
    uint64_t now = GetTickCount64();
    auto it = g_playerNameCache.find(uid);
    if (it != g_playerNameCache.end() && now - it->second.second < 8000) {
        strncpy(out, it->second.first.c_str(), outSz - 1);
        out[outSz - 1] = 0;
        return;
    }
    TryReadPlayerName(lnk, pc, common, out, outSz);
    if (IsValidPlayerName(out)) g_playerNameCache[uid] = { out, now };
}

static void TryReadPlayerName(uint64_t lnk, uint64_t pc, uint64_t common, char* out, int outSz)
{
    out[0] = 0;
    if (pc) { ScanNameFromBase(pc, out, outSz); if (IsValidPlayerName(out)) return; out[0] = 0; }
    if (lnk) { ScanNameFromBase(lnk, out, outSz); if (IsValidPlayerName(out)) return; out[0] = 0; }
    if (common) ScanNameFromBase(common, out, outSz);
}

static bool ReadPcLocalFlag(uint64_t pc)
{
    if (!IsValidPtr(pc)) return false;
    static const int offs[] = {
        0x468, 0x470, 0x478, 0x480, 0x488, 0x490, 0x498, 0x4A0,
        0x4A8, 0x4B0, 0x4B8, 0x4C0, 0x4C8, 0x4D0, 0x4D8, 0x4E0
    };
    for (int o : offs) {
        if (SDK->RPM<uint8_t>(pc + o) == 1) return true;
        if (SDK->RPM<uint32_t>(pc + o) == 1) return true;
    }
    return false;
}

static inline void UpdateCameraPos(const Matrix& viewMtx) {
    const float* d = viewMtx.data;
    float A = d[5]*d[10] - d[9]*d[6];
    float B = d[9]*d[2] - d[1]*d[10];
    float C = d[1]*d[6] - d[5]*d[2];
    float Z = d[0]*A + d[4]*B + d[8]*C;
    if (fabsf(Z) > 0.0001f) {
        float D = d[8]*d[6] - d[4]*d[10];
        float E = d[0]*d[10] - d[8]*d[2];
        float F = d[4]*d[2] - d[0]*d[6];
        float G = d[4]*d[9] - d[8]*d[5];
        float H = d[8]*d[1] - d[0]*d[9];
        float K = d[0]*d[5] - d[4]*d[1];
        g_cameraPos.x = -(d[12]*A + d[13]*D + d[14]*G) / Z;
        g_cameraPos.y = -(d[12]*B + d[13]*E + d[14]*H) / Z;
        g_cameraPos.z = -(d[12]*C + d[13]*F + d[14]*K) / Z;
    }
}

static uint64_t s_entityListPtr = 0;
static uint32_t s_entityListSlots = 0;
static int      s_entityListFailFrames = 0;
static uint64_t s_stickyListPtr = 0;
static uint32_t s_stickyListSlots = 0;

static uint32_t ProbeListLiveSlots(uint64_t list, uint32_t regionSlots)
{
    uint32_t probe = regionSlots < 256 ? regionSlots : 256;
    uint32_t live = 0;
    for (uint32_t i = 0; i < probe; i++) {
        uint64_t e = SDK->RPM<uint64_t>(list + (uint64_t)i * sizeof(EntPair));
        if (IsValidPtr(e)) live++;
    }
    return live;
}

static bool QueryListRegion(uint64_t list, uint32_t& regionSlots)
{
    regionSlots = 0;
    if (!IsValidPtr(list)) return false;
    MEMORY_BASIC_INFORMATION mbi{};
    if (!VirtualQueryEx(SDK->hProcess, (LPCVOID)list, &mbi, sizeof(mbi)))
        return false;
    if (mbi.State != MEM_COMMIT) return false;
    regionSlots = (uint32_t)(mbi.RegionSize / sizeof(EntPair));
    return regionSlots > 0;
}

static uint64_t ResolveEntityList(uint32_t& regionSlots) {
    regionSlots = 0;

    uint64_t candidates[2] = {
        SDK->RPM<uint64_t>(SDK->dwGameBase + offset::Address_entity_base),
        SDK->RPM<uint64_t>(SDK->dwGameBase + offset::EntityList_SlotCount_RVA + offset::EntityList_PtrOffset),
    };

    uint64_t bestList = 0;
    uint32_t bestSlots = 0;
    uint32_t bestLive = 0;
    for (uint64_t cand : candidates) {
        uint32_t slots = 0;
        if (!QueryListRegion(cand, slots)) continue;
        uint32_t live = ProbeListLiveSlots(cand, slots);
        if (!bestList || live > bestLive) {
            bestList = cand;
            bestSlots = slots;
            bestLive = live;
        }
    }

    uint32_t stickyLive = 0;
    uint32_t stickySlots = 0;
    if (s_stickyListPtr && QueryListRegion(s_stickyListPtr, stickySlots))
        stickyLive = ProbeListLiveSlots(s_stickyListPtr, stickySlots);

    if (s_stickyListPtr && stickyLive >= 2) {
        bool challengerMuchBetter = bestList && bestList != s_stickyListPtr &&
            bestLive >= stickyLive + 3 && bestLive > (stickyLive * 11) / 10;
        if (!challengerMuchBetter) {
            s_entityListPtr = s_stickyListPtr;
            s_entityListSlots = stickySlots;
            s_entityListFailFrames = 0;
            regionSlots = stickySlots;
            return s_stickyListPtr;
        }
    }

    if (bestList && bestLive > 0) {
        s_stickyListPtr = bestList;
        s_stickyListSlots = bestSlots;
        s_entityListPtr = bestList;
        s_entityListSlots = bestSlots;
        s_entityListFailFrames = 0;
        regionSlots = bestSlots;
        return bestList;
    }

    if (s_entityListPtr && s_entityListFailFrames < 240) {
        s_entityListFailFrames++;
        uint32_t slots = 0;
        if (QueryListRegion(s_entityListPtr, slots)) {
            regionSlots = slots;
            return s_entityListPtr;
        }
    }

    if (bestList) {
        regionSlots = bestSlots;
        return bestList;
    }
    return 0;
}

void ResetEntityListResolver()
{
    s_entityListPtr = 0;
    s_entityListSlots = 0;
    s_entityListFailFrames = 0;
    s_stickyListPtr = 0;
    s_stickyListSlots = 0;
}

void ResetMemoryAuxCaches()
{
    g_playerNameCache.clear();
    s_hpStableCache.clear();
    g_componentCache.clear();
}

void PrintGameDebug() {
#ifndef RS_SILENT_BUILD
    printf(S("[dbg] list=0x%llX region=%u raw=%u nonnull=%u link=%u map=%u match=%u vel=%u ents=%u screen=%u\n"),
        (unsigned long long)g_debug.entity_list_ptr,
        g_debug.slot_count, g_debug.raw_slots, g_debug.non_null_slots, g_debug.link_count,
        g_debug.common_map_count, g_debug.common_match_count,
        g_debug.velocity_count, g_debug.final_entities, g_debug.screen_valid_count);
    printf(S("[dbg] vm enc=0x%llX root=0x%llX ok=%d w=%.3f p1=0x%llX p2=0x%llX p3=0x%llX p4=0x%llX\n"),
        (unsigned long long)g_debug.vm_enc, (unsigned long long)g_debug.vm_root,
        (int)g_debug.vm_ok, g_debug.vm_w,
        (unsigned long long)g_debug.vm_ptr1, (unsigned long long)g_debug.vm_ptr2,
        (unsigned long long)g_debug.vm_ptr3, (unsigned long long)g_debug.vm_ptr4);
    printf(S("[dbg] comp qword=0x%llX salt=0x%llX byte=0x%02X | draw box=%d skel=%d chams=%d team=%d vis=%d\n"),
        (unsigned long long)g_debug.comp_qword_base, (unsigned long long)g_debug.comp_salt,
        g_debug.comp_byte,
        (int)g_drawBoxes, (int)g_drawSkeleton, (int)g_chams,
        (int)g_teamCheck, (int)g_visCheck);
    int localCount = 0, enemyCount = 0;
    for (auto& e : g_entities) {
        if (e.isLocal) localCount++;
        else if (IsEnemyEntity(e)) enemyCount++;
    }
    printf(S("[dbg] localTeam=%d mask=0x%X localUid=%u local=%d enemies=%d espvis=%d teamChk=%d source=%s\n"),
        g_localTeam, g_localTeamMask, g_localPlayerUid, localCount, enemyCount,
        (int)g_aimVisCheck, (int)g_teamCheck,
#ifdef RS_INTERNAL_READER
        "internal-overlay");
#else
        g_internalReaderActive ? "internal-dll" : "external-rpm");
#endif
    for (auto& e : g_entities) {
        printf(S("[dbg]  uid=%u local=%d team=%d raw=0x%X vis=%d hp=%.0f bones=%d\n"),
            e.uid, (int)e.isLocal, e.team, e.teamRaw, (int)e.visible, e.health, e.boneCount);
    }
#endif
}

static bool CopyEntityListMemory(uint64_t lp, EntPair* raw, SIZE_T count)
{
#ifdef RS_INTERNAL_READER
    __try {
        memcpy(raw, (const void*)lp, count * sizeof(EntPair));
        return true;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
#else
    return ReadProcessMemory(SDK->hProcess, (LPCVOID)lp, raw, count * sizeof(EntPair), nullptr) != 0;
#endif
}

static void ReadEntitiesImpl();

void ReadEntities() {
    ReadEntitiesImpl();

    static int s_zeroStreak = 0;
    static int s_lastGoodCount = 0;
    const int count = (int)g_entities.size();

    if (count == 0) {
        s_zeroStreak++;
        if (s_lastGoodCount >= 3 && s_zeroStreak >= 600) {
            if (s_zeroStreak == 600 || (s_zeroStreak > 600 && s_zeroStreak % 800 == 0)) {
                ResetEntityListResolver();
                ResetMemoryAuxCaches();
#ifdef RS_INTERNAL_READER
                if (SDK) SDK->dwGameBase = (uint64_t)GetModuleHandleW(nullptr);
#endif
            }
        }
    } else {
        if (count >= 3)
            s_lastGoodCount = count;
        s_zeroStreak = 0;
    }
}

static void ReadEntitiesImpl() {
    g_entities.clear();
    g_debug = {};
    if (!SDK || !SDK->hProcess) return;

    g_debug.comp_qword_base = SDK->RPM<uint64_t>(SDK->dwGameBase + offset::OW_COMPONENT_QWORD);
    g_debug.comp_byte = SDK->RPM<uint8_t>(SDK->dwGameBase + offset::OW_COMPONENT_BYTE);
    if (g_debug.comp_qword_base)
        g_debug.comp_salt = SDK->RPM<uint64_t>(g_debug.comp_qword_base + offset::ComponentXorQwordOffset);

    uint32_t regionSlots = 0;
    uint64_t lp = ResolveEntityList(regionSlots);
    g_debug.entity_list_ptr = lp;
    g_debug.slot_count = regionSlots;
    if (!lp || !regionSlots) return;

    SIZE_T cnt = regionSlots;
    if (cnt > 8192) cnt = 8192;
    g_debug.raw_slots = (uint32_t)cnt;

    EntPair* raw = (EntPair*)malloc(cnt * sizeof(EntPair));
    if (!raw) return;

    SIZE_T readCount = 0;
    for (SIZE_T tryCnt = cnt; tryCnt > 0; ) {
        if (CopyEntityListMemory(lp, raw, tryCnt)) {
            readCount = tryCnt;
            break;
        }
        if (tryCnt <= 0x400) break;
        tryCnt -= 0x400;
    }
    if (!readCount) {
        free(raw);
        return;
    }
    cnt = readCount;
    g_debug.raw_slots = (uint32_t)cnt;

    for (size_t i = 0; i < cnt; i++) {
        if (IsValidPtr(raw[i].e))
            g_debug.non_null_slots++;
    }

    g_componentCache.clear();

    std::unordered_map<uint32_t, uint64_t> commonMap;
    commonMap.reserve(cnt);
    for (size_t x = 0; x < cnt; x++) {
        uint64_t e = raw[x].e;
        if (!IsValidPtr(e)) continue;
        uint64_t bm0 = SDK->RPM<uint64_t>(e + 0x110);
        if (!(bm0 & (1ULL << TYPE_HEALTH))) continue;
        uint32_t uid = SDK->RPM<uint32_t>(e + 0x138);
        if (uid) commonMap[uid] = e;
    }
    g_debug.common_map_count = (uint32_t)commonMap.size();

    auto findCommon = [&](uint32_t uid, uint64_t skip) -> uint64_t {
        if (!uid) return 0;
        auto it = commonMap.find(uid);
        if (it != commonMap.end() && it->second != skip)
            return it->second;
        for (size_t x = 0; x < cnt; x++) {
            uint64_t possible = raw[x].e;
            if (!IsValidPtr(possible) || possible == skip) continue;
            if (SDK->RPM<uint32_t>(possible + 0x138) == uid)
                return possible;
        }
        return 0;
    };

    g_hpPacks.clear();
    if (g_drawHpPacks) {
        for (size_t i = 0; i < cnt; i++) {
            uint64_t e = raw[i].e;
            if (!IsValidPtr(e)) continue;
            uint64_t idPtr = SDK->RPM<uint64_t>(e + 0x30) & 0xFFFFFFFFFFFFFFC0ULL;
            if (!IsValidPtr(idPtr)) continue;
            uint64_t entityId = SDK->RPM<uint64_t>(idPtr + 0x10);
            if (entityId != 0x400000000000060ULL &&
                entityId != 0x40000000000480AULL &&
                entityId != 0x40000000000005FULL) continue;
            uint64_t mesh = DecryptComponent(e, TYPE_VELOCITY);
            if (!IsValidPtr(mesh)) continue;
            float3 p = SDK->RPM<float3>(mesh + 0x3D0);
            if (isnan(p.x) || isnan(p.y) || isnan(p.z)) continue;
            if (fabsf(p.x) < 0.01f && fabsf(p.y) < 0.01f && fabsf(p.z) < 0.01f) continue;
            if (fabsf(p.x) > 20000.f || fabsf(p.y) > 20000.f || fabsf(p.z) > 20000.f) continue;

            float3 q = {
                roundf(p.x * 10.f) / 10.f,
                roundf(p.y * 10.f) / 10.f,
                roundf(p.z * 10.f) / 10.f
            };

            float3 stable = q;
            auto it = s_hpStableCache.find(e);
            if (it != s_hpStableCache.end()) {
                float dx = q.x - it->second.x, dy = q.y - it->second.y, dz = q.z - it->second.z;
                if (dx * dx + dy * dy + dz * dz < 4.f)
                    stable = it->second;
            }
            s_hpStableCache[e] = stable;

            HpPackInfo hp = {};
            hp.entityId = entityId;
            hp.addr = e;
            hp.pos = p;
            hp.stablePos = stable;
            g_hpPacks.push_back(hp);
        }
    }
    g_debug.hp_pack_count = (uint32_t)g_hpPacks.size();

    std::unordered_set<uint64_t> seen;
    for (size_t i = 0; i < cnt; i++) {
        uint64_t a = raw[i].e;
        if (!IsValidPtr(a)) continue;

        uint64_t lnk = GetDecryptedComponent(a, TYPE_LINK);
        if (!lnk) {
            InvalidateEntityComponentCache(a);
            lnk = GetDecryptedComponent(a, TYPE_LINK);
        }
        if (!lnk) continue;
        g_debug.link_count++;

        uint32_t uid = SDK->RPM<uint32_t>(lnk + 0xD4);
        uint64_t common = findCommon(uid, a);
        if (!common) {
            uint64_t bmSelf = SDK->RPM<uint64_t>(a + 0x110);
            if (bmSelf & (1ULL << TYPE_HEALTH))
                common = a;
        }
        if (!common || seen.count(common)) continue;
        g_debug.common_match_count++;
        seen.insert(common);

        float hp = 0, hp_max = 0;
        float armor = 0, armor_max = 0;
        float barrier = 0, barrier_max = 0;
        float3 pos = {};
        float rot_y = 0;
        float3 bones[18] = {};
        int boneCount = 0;
        bool hasBones = false;

        uint64_t hc = GetDecryptedComponent(common, TYPE_HEALTH);
        if (!hc) {
            InvalidateEntityComponentCache(common);
            hc = GetDecryptedComponent(common, TYPE_HEALTH);
        }
        if (hc) {
            hp_max = SDK->RPM<float>(hc + 0xDC);
            hp = SDK->RPM<float>(hc + 0xE0);
            armor_max = SDK->RPM<float>(hc + 0x21C);
            armor = SDK->RPM<float>(hc + 0x220);
            barrier_max = SDK->RPM<float>(hc + 0x35C);
            barrier = SDK->RPM<float>(hc + 0x360);
            if (hp < 0.f) hp = 0.f;
            if (armor < 0.f) armor = 0.f;
            if (barrier < 0.f) barrier = 0.f;
            if (hp_max < 0.f) hp_max = 0.f;
            if (armor_max < 0.f) armor_max = 0.f;
            if (barrier_max < 0.f) barrier_max = 0.f;
        }

        uint64_t vc = GetDecryptedComponent(common, TYPE_VELOCITY);
        if (!vc) {
            InvalidateEntityComponentCache(common);
            vc = GetDecryptedComponent(common, TYPE_VELOCITY);
        }
        if (!vc) vc = GetDecryptedComponent(a, TYPE_VELOCITY);
        if (!vc) continue;
        g_debug.velocity_count++;

        pos = SDK->RPM<float3>(vc + 0x200);
        pos.y -= 1.0f;
        pos = SnapWorldPos(pos);

        uint64_t rc = GetDecryptedComponent(common, TYPE_ROTATION);
        if (rc) {
            uint64_t rp = SDK->RPM<uint64_t>(rc + 0x888);
            if (rp) {
                float3 rot = SDK->RPM<float3>(rp + 0x90C);
                rot_y = rot.x;
            }
        }
        rot_y += g_skelRotOffset;

        uint64_t pBoneData = SDK->RPM<uint64_t>(vc + 0x8B0);
        if (!pBoneData)
            pBoneData = SDK->RPM<uint64_t>(vc + 0x8A0);

        if (pBoneData) {
            uint64_t bonesBase = SDK->RPM<uint64_t>(pBoneData + 0x20);
            if (bonesBase) {
                int indices[18];
                for (int bi = 0; bi < 18; bi++)
                    indices[bi] = get_bone_array_index(pBoneData, g_boneIds[bi]);
                int uniqueCount = 0;
                for (int bi = 0; bi < 18; bi++) {
                    bool dup = false;
                    for (int bj = 0; bj < bi; bj++)
                        if (indices[bi] == indices[bj]) { dup = true; break; }
                    if (!dup) uniqueCount++;
                }
                if (uniqueCount >= 12) {
                    boneCount = 18;
                    for (int bi = 0; bi < 18; bi++) {
                        float3 lb = SDK->RPM<float3>(bonesBase + (0x30 * indices[bi]) + 0x20);
                        float3 rot = rotate_y(lb, rot_y);
                        bones[bi] = SnapWorldPos({ rot.x + pos.x, rot.y + pos.y, rot.z + pos.z });
                    }
                    hasBones = true;
                } else {
                    static const int botBoneIds[5] = {17, 16, 3, 13, 54};
                    boneCount = 5;
                    for (int bi = 0; bi < 5; bi++) {
                        int idx = get_bone_array_index(pBoneData, botBoneIds[bi]);
                        float3 lb = SDK->RPM<float3>(bonesBase + (0x30 * idx) + 0x20);
                        float3 rot = rotate_y(lb, rot_y);
                        bones[bi] = SnapWorldPos({ rot.x + pos.x, rot.y + pos.y, rot.z + pos.z });
                    }
                    hasBones = true;
                }
            }
        }

        bool isLocal = false;
        uint64_t pcTry = GetDecryptedComponent(a, TYPE_PLAYERCONTROLLER);
        if (!pcTry) pcTry = GetDecryptedComponent(common, TYPE_PLAYERCONTROLLER);
        bool hasPC = (pcTry != 0);
        bool pcLocal = ReadPcLocalFlag(pcTry);
        if (pcLocal) isLocal = true;

        int entityTeam = 0;
        uint32_t teamVal = 0;
        uint64_t tb = GetDecryptedComponent(common, TYPE_TEAM);
        if (!tb) tb = GetDecryptedComponent(a, TYPE_TEAM);
        if (tb) {
            teamVal = SDK->RPM<uint32_t>(tb + 0x58);
            if (teamVal & (1 << 23)) entityTeam = 1;
            else if (teamVal & (1 << 24)) entityTeam = 2;
            else if (teamVal & (1 << 27)) entityTeam = 5;
            else entityTeam = (int)(teamVal & 0xFF) + 10;
        }

        bool entityVis = true;
        uint64_t vb = GetDecryptedComponent(a, TYPE_P_VISIBILITY);
        if (vb) entityVis = DecryptVis(vb);

        uint16_t heroId = 0;
        uint64_t hc2 = GetDecryptedComponent(a, TYPE_P_HEROID);
        if (hc2) {
            uint64_t hid64 = SDK->RPM<uint64_t>(hc2 + 0xD0);
            heroId = (uint16_t)(hid64 & 0xFFFF);
            if (!heroId && hid64)
                heroId = (uint16_t)((hid64 >> 16) & 0xFFFF);
        }
        if (IsTrainingBotHeroId(heroId))
            heroId = 0;
        if (boneCount > 0 && boneCount <= 5 && hp_max <= 150.f)
            heroId = 0;

        char playerName[48] = {};
        GetPlayerNameCached(uid, lnk, pcTry, common, playerName, sizeof(playerName));

        EntityInfo ei = {};
        ei.addr = a; ei.common = common; ei.link = lnk; ei.heroId = heroId;
        ei.uid = uid;
        ei.hp = hp; ei.hp_max = hp_max;
        ei.armor = armor; ei.armor_max = armor_max;
        ei.barrier = barrier; ei.barrier_max = barrier_max;
        ei.health = hp + armor + barrier;
        ei.health_max = hp_max + armor_max + barrier_max;
        if (ei.health_max < 1.f)
            ei.health_max = hp_max > 1.f ? hp_max : 0.f;
        ei.pos = pos;
        ei.rot_y = rot_y; ei.isLocal = isLocal; ei.pcLocal = pcLocal; ei.visible = entityVis;
        ei.hasBones = hasBones; ei.hasPC = hasPC; ei.boneCount = boneCount;
        ei.team = entityTeam; ei.teamRaw = teamVal; ei.pBoneData = pBoneData;
        memcpy(ei.playerName, playerName, sizeof(ei.playerName));
        if (boneCount > 0) memcpy(ei.bones, bones, sizeof(bones));
        g_entities.push_back(ei);
    }

    g_debug.final_entities = (uint32_t)g_entities.size();
    free(raw);
}

void ReadViewMatrix() {
    g_debug.vm_enc = SDK->RPM<uint64_t>(SDK->dwGameBase + offset::OW_VIEWMATRIX_ENC);
    if (!g_debug.vm_enc || g_debug.vm_enc == 0x7F00FD1944EF26AAULL) {
        g_debug.vm_ok = false;
        return;
    }

    g_debug.vm_root = (g_debug.vm_enc - offset::ViewMatrix_Sub) ^ offset::ViewMatrix_Xor;
    if (!IsValidPtr(g_debug.vm_root)) {
        g_debug.vm_ok = false;
        return;
    }

    g_debug.vm_ptr1 = SDK->RPM<uint64_t>(g_debug.vm_root + offset::VM_P1);
    g_debug.vm_ptr2 = SDK->RPM<uint64_t>(g_debug.vm_ptr1 + offset::VM_P2);

    if (!IsValidPtr(g_debug.vm_ptr1) || !IsValidPtr(g_debug.vm_ptr2)) {
        g_debug.vm_ok = false;
        return;
    }

    g_vmCameraPtr = g_debug.vm_ptr2;

    if (g_freecam) {
        g_debug.vm_ok = true;
        return;
    }

    Matrix viewMtx = SDK->RPM<Matrix>(g_debug.vm_ptr2 + offset::VM_ViewMatrix);
    UpdateCameraPos(viewMtx);

    Matrix projMtx = SDK->RPM<Matrix>(g_debug.vm_ptr2 + offset::VM_ProjMatrix);
    projMtx.data[3] = 0.f;
    g_viewMatrix = MulMatrix(viewMtx, projMtx);
    g_debug.vm_w = g_viewMatrix.data[15];
    g_debug.vm_ok = fabsf(g_viewMatrix.data[0]) > 0.0001f || fabsf(g_viewMatrix.data[15]) > 0.0001f;
}

bool WorldToScreen(float3 world, float& sx, float& sy) {
    float* m = g_viewMatrix.data;
    float w = m[3] * world.x + m[7] * world.y + m[11] * world.z + m[15];
    if (w < 0.01f) return false;
    float x = m[0] * world.x + m[4] * world.y + m[8] * world.z + m[12];
    float y = m[1] * world.x + m[5] * world.y + m[9] * world.z + m[13];
    float invW = 1.0f / w;
    sx = (g_screenW * 0.5f) * (1.0f + x * invW);
    sy = (g_screenH * 0.5f) * (1.0f - y * invW);
    return true;
}
