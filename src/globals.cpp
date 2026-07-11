#include "sdk.hpp"
#include "heroicons.h"
#include "gametracker.h"
#include <vector>

OverwatchSDK* SDK = nullptr;
char g_activeConfig[64] = "default";
std::unordered_map<uint16_t, ID3D11ShaderResourceView*> g_heroIcons;
std::unordered_map<uint16_t, float> g_heroIconAspect;
std::unordered_map<uint32_t, ID3D11ShaderResourceView*> g_abilityIcons;
std::vector<EntityInfo> g_entities;
Matrix g_viewMatrix;
float3 g_cameraPos = {};
uint64_t g_vmCameraPtr = 0;
float g_screenW = 1920, g_screenH = 1080;
DWORD g_pid = 0;
uint64_t g_base = 0;
HWND g_hWnd = nullptr;
ID3D11Device* g_pd3dDevice = nullptr;
ID3D11DeviceContext* g_pd3dContext = nullptr;
IDXGISwapChain* g_pSwapChain = nullptr;
ID3D11RenderTargetView* g_mainRT = nullptr;
bool g_running = true;
bool g_menuOpen = false;
bool g_drawBoxes = false, g_drawSkeleton = false, g_drawLines = false, g_draw3dBox = true, g_teamCheck = true, g_visCheck = false;
int g_localTeam = 0;
uint32_t g_localTeamMask = 0;
uint32_t g_localPlayerUid = 0;
bool g_ultBars = false, g_ultPanel = false;
float g_skelRotOffset = 2.5f;
bool g_drawEntList = true;
ImFont* g_defaultFont = nullptr;
ImFont* g_menuFont = nullptr;
bool g_drawHeroIcon = true;
float g_heroIconSize = 50.f;
bool g_heroIconHiddenOnly = false;

bool  g_chams = false;
float g_chamsColor[3] = { 1.f, 0.2f, 0.9f };
float g_chamsFill = 0.6f;

bool  g_freecam = false;
float g_freecamSpeed = 18.f;
bool  g_thirdPerson = false;
float g_thirdPersonDist = 4.f;

bool g_drawHpPacks = true;
std::vector<HpPackInfo> g_hpPacks;

bool g_abilityPanel = false;
bool g_abilityDebug = false;

bool  g_glow = false;
float g_glowColor[3] = { 1.f, 0.2f, 0.9f };
float g_glowThickness = 1.6f;

bool  g_aimVisCheck = true;
bool  g_drawHeroName = false;
bool  g_drawPlayerName = false;
bool  g_drawHpBars = true;
bool  g_showGameInfo = false;
int   g_teamOverride = 0;

bool  g_drawRadar = false;
float g_radarSize = 140.f;
float g_radarRange = 45.f;
bool  g_radarShowAllies = true;
bool  g_offscreenArrows = true;
bool  g_behindWarning = true;
bool  g_distanceEsp = false;
bool  g_heroSwapAlert = false;
bool  g_ultReadyAlert = false;
bool  g_allyUltPanel = true;
bool  g_drawMatchTimer = false;
bool  g_matchHudTransparent = true;
bool  g_matchHudShowMap = true;
bool  g_matchHudShowPhase = true;
bool  g_matchHudShowPing = true;
bool  g_matchHudShowTimer = true;
int   g_matchHudStyle = 1;

bool  g_drawHeadDot = false;
float g_headDotSize = 4.f;
bool  g_drawFacingLine = false;
float g_facingLineLen = 2.5f;

bool  g_requestUnload = false;

char g_assetHeroPathA[512] = {};
char g_assetAbilityPathA[512] = {};
int  g_assetHeroFilesOnDisk = 0;
HMODULE g_overlayModule = nullptr;
bool  g_drawHpPackBoxes = true;
bool  g_drawHpPackLabels = true;
float g_hpPackLabelOffset = 2.f;
bool  g_lowHpPulse = true;
bool  g_priorityMarker = true;
bool  g_fovThreatCount = true;
bool  g_offscreenUseFov = true;
float g_markerFov = 60.f;
bool  g_drawMarkerCircle = false;

std::unordered_map<uint64_t, EntityComponentCache> g_componentCache;
GameDebugStats g_debug;
GameWorldInfo g_worldInfo = {};
std::vector<GameAlert> g_gameAlerts;
HeroTrackerState g_heroTracker;
