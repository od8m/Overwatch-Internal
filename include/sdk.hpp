#pragma once
#include "safe_mode.h"
#include <cstdint>
#include <cstdio>
#include <vector>
#include <windows.h>
#include <unordered_map>
#include <cstring>
#include "imgui.h"
#include "stringencryption.hpp"
#include "spoof_call.hpp"

struct ID3D11Device;
struct ID3D11DeviceContext;
struct IDXGISwapChain;
struct ID3D11RenderTargetView;

struct float3 { float x, y, z; };
struct float2 { float x, y; };

inline uint32_t GetTeamMask(uint32_t teamVal)
{
    if (teamVal & (1u << 23)) return (1u << 23);
    if (teamVal & (1u << 24)) return (1u << 24);
    if (teamVal & (1u << 27)) return (1u << 27);
    return 0;
}

struct Matrix {
    float data[16];
    float& at(int r, int c) { return data[r * 4 + c]; }
};

struct EntityComponentCache {
    uint64_t link = 0;
    bool link_cached = false;
    uint64_t health = 0;
    bool health_cached = false;
    uint64_t velocity = 0;
    bool velocity_cached = false;
    uint64_t rotation = 0;
    bool rotation_cached = false;
    uint64_t pc = 0;
    bool pc_cached = false;
    uint64_t team = 0;
    bool team_cached = false;
    uint64_t visibility = 0;
    bool visibility_cached = false;
    uint64_t hc2 = 0;
    bool hc2_cached = false;
};
extern std::unordered_map<uint64_t, EntityComponentCache> g_componentCache;

struct EntityInfo {
    uint64_t addr;
    uint64_t common;
    uint64_t link;
    uint16_t heroId;
    uint32_t uid;
    float health, health_max;
    float hp, hp_max, armor, armor_max, barrier, barrier_max;
    float3 pos;
    float rot_y;
    float3 bones[18];
    float2 boneScreen[18];
    int boneCount;
    bool hasBones;
    bool isLocal;
    bool visible;
    bool hasPC;
    int team;
    uint32_t teamRaw;
    uint64_t pBoneData;
    char playerName[48];

    bool  screenValid;
    bool  boxValid;
    float boxL, boxT, boxR, boxB;
    float minX, minY, maxX, maxY;
    bool  pcLocal;
};

struct OverwatchSDK {
    HANDLE hProcess = nullptr;
    uint64_t dwGameBase = 0;
#ifdef RS_INTERNAL_READER
    template<typename T> T RPM(uint64_t addr) {
        T val{};
        if (addr < 0x10000ULL || addr >= 0x00007FFFFFFFFFFFULL) return val;
        __try { memcpy(&val, (const void*)addr, sizeof(T)); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
        return val;
    }
    template<typename T> void WPM(uint64_t addr, T val) {
        if (addr < 0x10000ULL || addr >= 0x00007FFFFFFFFFFFULL) return;
        __try { memcpy((void*)addr, &val, sizeof(T)); }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
#else
    template<typename T> T RPM(uint64_t addr) {
        T val{}; ReadProcessMemory(hProcess, (LPCVOID)addr, &val, sizeof(T), nullptr); return val;
    }
    template<typename T> void WPM(uint64_t addr, T val) {
        WriteProcessMemory(hProcess, (LPVOID)addr, &val, sizeof(T), nullptr);
    }
#endif
};

struct EntPair { uint64_t e; uint64_t pad; };

extern OverwatchSDK* SDK;
extern std::vector<EntityInfo> g_entities;
extern Matrix g_viewMatrix;
extern float3 g_cameraPos;
extern uint64_t g_vmCameraPtr;
extern float g_screenW, g_screenH;
extern DWORD g_pid;
extern uint64_t g_base;
extern HWND g_hWnd;
extern ID3D11Device* g_pd3dDevice;
extern ID3D11DeviceContext* g_pd3dContext;
extern IDXGISwapChain* g_pSwapChain;
extern ID3D11RenderTargetView* g_mainRT;
extern bool g_running, g_menuOpen;
extern bool g_drawBoxes, g_drawSkeleton, g_drawLines, g_draw3dBox, g_teamCheck, g_visCheck;
extern int g_localTeam;
extern uint32_t g_localTeamMask;
extern uint32_t g_localPlayerUid;
extern bool g_ultBars, g_ultPanel;
extern float g_skelRotOffset;
extern bool g_drawEntList;
extern ImFont* g_defaultFont;
extern ImFont* g_menuFont;
extern bool g_drawHeroIcon;
extern float g_heroIconSize;
extern bool g_heroIconHiddenOnly;

extern bool  g_chams;
extern float g_chamsColor[3];
extern float g_chamsFill;

extern bool  g_freecam;
extern float g_freecamSpeed;
extern bool  g_thirdPerson;
extern float g_thirdPersonDist;

extern char g_activeConfig[64];

extern bool g_drawHpPacks;

struct HpPackInfo {
    uint64_t entityId;
    uint64_t addr;
    float3   pos;        // raw read
    float3   stablePos;  // snapped / cached world position for drawing
};
extern std::vector<HpPackInfo> g_hpPacks;

extern bool g_abilityPanel;
extern bool g_abilityDebug;

extern bool  g_glow;
extern float g_glowColor[3];
extern float g_glowThickness;

extern bool  g_aimVisCheck;
extern bool  g_drawHeroName;
extern bool  g_drawPlayerName;
extern bool  g_drawHpBars;
extern bool  g_showGameInfo;
extern int   g_teamOverride;

extern bool  g_drawRadar;
extern float g_radarSize;
extern float g_radarRange;
extern bool  g_radarShowAllies;
extern bool  g_offscreenArrows;
extern bool  g_behindWarning;
extern bool  g_distanceEsp;
extern bool  g_heroSwapAlert;
extern bool  g_ultReadyAlert;
extern bool  g_allyUltPanel;
extern bool  g_drawMatchTimer;
extern bool  g_matchHudTransparent;
extern bool  g_matchHudShowMap;
extern bool  g_matchHudShowPhase;
extern bool  g_matchHudShowPing;
extern bool  g_matchHudShowTimer;
extern int   g_matchHudStyle; // 0=minimal text, 1=timer hero center, 2=full strip

extern bool  g_drawHeadDot;
extern float g_headDotSize;
extern bool  g_drawFacingLine;
extern float g_facingLineLen;

extern bool  g_requestUnload;
extern char  g_assetHeroPathA[512];
extern char  g_assetAbilityPathA[512];
extern int   g_assetHeroFilesOnDisk;
extern HMODULE g_overlayModule;
extern bool  g_drawHpPackBoxes;
extern bool  g_drawHpPackLabels;
extern float g_hpPackLabelOffset;
extern bool  g_lowHpPulse;
extern bool  g_priorityMarker;
extern bool  g_fovThreatCount;
extern bool  g_offscreenUseFov;
extern float g_markerFov;
extern bool  g_drawMarkerCircle;

inline bool IsFfaTeam(int team, uint32_t mask)
{
    return team == 5 || mask == (1u << 27);
}

inline int GetEffectiveLocalTeam()
{
    if (g_teamOverride == 1) return 1;
    if (g_teamOverride == 2) return 2;
    if (g_teamOverride == 5) return 5;
    return g_localTeam;
}

inline uint32_t GetEffectiveLocalTeamMask()
{
    if (g_teamOverride == 1) return (1u << 23);
    if (g_teamOverride == 2) return (1u << 24);
    if (g_teamOverride == 5) return (1u << 27);
    return g_localTeamMask;
}

inline bool IsLocalIdentified()
{
    if (g_teamOverride != 0) return true;
    if (g_localPlayerUid != 0 && g_localTeamMask != 0) return true;
    if (g_localTeamMask != 0 && g_localTeam == 2) return true;
    return false;
}

inline bool IsEnemyEntity(const EntityInfo& e)
{
    if (e.isLocal) return false;
    if (e.health_max > 1.f && e.health <= 0.f) return false;
    if (!g_teamCheck) return true;

    if (!IsLocalIdentified()) {
        if (g_localPlayerUid && e.uid == g_localPlayerUid) return false;
        return e.hasPC || e.boneCount >= 8 || e.health_max >= 150.f;
    }

    const int effTeam = GetEffectiveLocalTeam();
    const uint32_t effMask = GetEffectiveLocalTeamMask();

    if (IsFfaTeam(effTeam, effMask))
        return true;

    const uint32_t em = GetTeamMask(e.teamRaw);
    if (em && effMask) return em != effMask;

    if (effTeam && e.team) return e.team != effTeam;
    return false;
}

struct GameWorldInfo {
    uint64_t adminPtr = 0;
    uint64_t worldPtr = 0;
    uint64_t sessionPtr = 0;
    uint32_t mapId = 0;
    uint32_t gameMode = 0;
    uint32_t matchState = 0;
    float    gameTime = 0.f;
    float    roundTime = 0.f;
    float    readFov = 0.f;
    uint32_t pingMs = 0;
    uint32_t regionId = 0;
    bool     pingValid = false;
    int      allyCount = 0;
    int      enemyCount = 0;
    char     mapName[48];
    char     phaseName[32];
    char     regionName[16];
    char     timeText[16];
};
extern GameWorldInfo g_worldInfo;
void UpdateGameWorldInfo();

struct GameDebugStats {
    uint64_t entity_list_ptr = 0;
    uint32_t slot_count = 0;
    uint32_t raw_slots = 0;
    uint32_t non_null_slots = 0;
    uint32_t link_count = 0;
    uint32_t common_map_count = 0;
    uint32_t common_match_count = 0;
    uint32_t velocity_count = 0;
    uint32_t screen_valid_count = 0;
    uint32_t final_entities = 0;
    uint64_t vm_enc = 0;
    uint64_t vm_root = 0;
    uint64_t vm_ptr1 = 0;
    uint64_t vm_ptr2 = 0;
    uint64_t vm_ptr3 = 0;
    uint64_t vm_ptr4 = 0;
    bool vm_ok = false;
    float vm_w = 0.f;
    uint64_t comp_qword_base = 0;
    uint64_t comp_salt = 0;
    uint8_t comp_byte = 0;
    uint32_t hp_pack_count = 0;
};
extern GameDebugStats g_debug;
void PrintGameDebug();