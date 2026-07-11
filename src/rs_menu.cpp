#include "rs_menu.h"
#include "menu_style.h"
#include "config.h"
#include "reset_state.h"
#include "shared_ipc.h"
#include "sdk.hpp"
#include "utils.hpp"
#include "heroicons.h"
#include "abilityicons.h"
#include "imgui.h"
#include <cstring>
#include <cstdio>
#include <vector>
#include <string>

struct RsMenuState {
    int main_tab = 0;
    int top_tab = 0;
    int visuals_subtab = 0;
};
static RsMenuState g_rsMenu;
static char g_cfgStatus[160] = {};
static uint64_t g_cfgStatusUntil = 0;
static int* g_keyCapture = nullptr;

static void SetCfgStatus(const char* msg)
{
    if (!msg) return;
    strncpy(g_cfgStatus, msg, sizeof(g_cfgStatus) - 1);
    g_cfgStatus[sizeof(g_cfgStatus) - 1] = 0;
    g_cfgStatusUntil = GetTickCount64() + 2500;
}

void InitRsMenuStyle()
{
    SetupMenuStyle();
}

static bool DrawTabButton(const char* label, bool is_selected, float width = 0.f)
{
    ImVec2 text_size = ImGui::CalcTextSize(label);
    ImVec2 button_size = ImVec2(width > 0.f ? width : text_size.x + 6.f, text_size.y + 2.f);
    ImVec4 bg_color = is_selected ? ImVec4(0.25f, 0.25f, 0.25f, 1.f) : ImVec4(0.05f, 0.05f, 0.05f, 1.f);
    ImGui::PushStyleColor(ImGuiCol_Button, bg_color);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(bg_color.x + 0.1f, bg_color.y + 0.1f, bg_color.z + 0.1f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, bg_color);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.f, 1.f, 1.f, 1.f));
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.78f, 0.78f, 0.78f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0.f, 1.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
    bool clicked = ImGui::Button(label, button_size);
    ImGui::PopStyleVar(3);
    ImGui::PopStyleColor(5);
    return clicked;
}

static bool DrawTriangleControl(const char* id, float* v, float step)
{
    ImGui::PushID(id);
    bool changed = false;
    ImVec2 button_size = ImVec2(20.f, 20.f);
    ImGui::InvisibleButton(id, button_size);
    bool hovered = ImGui::IsItemHovered();
    bool clicked = ImGui::IsItemClicked();
    bool held = ImGui::IsItemActive();
    ImVec2 pos = ImGui::GetItemRectMin();
    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImU32 button_color = ImGui::GetColorU32(hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button);
    ImU32 border_color = ImGui::GetColorU32(ImGuiCol_Border);
    draw_list->AddRectFilled(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y), button_color);
    draw_list->AddRect(pos, ImVec2(pos.x + button_size.x, pos.y + button_size.y), border_color);
    float center_x = pos.x + button_size.x * 0.5f;
    float center_y = pos.y + button_size.y * 0.5f;
    ImU32 triangle_color = ImGui::GetColorU32(ImGuiCol_Text);
    draw_list->AddTriangleFilled(
        ImVec2(center_x, center_y - 5.f),
        ImVec2(center_x - 3.5f, center_y - 1.f),
        ImVec2(center_x + 3.5f, center_y - 1.f), triangle_color);
    draw_list->AddTriangleFilled(
        ImVec2(center_x, center_y + 5.f),
        ImVec2(center_x - 3.5f, center_y + 1.f),
        ImVec2(center_x + 3.5f, center_y + 1.f), triangle_color);
    ImVec2 mouse_pos = ImGui::GetMousePos();
    bool is_top_half = (mouse_pos.y - pos.y) < button_size.y * 0.5f;
    static float hold_timer = 0.f;
    static const char* hold_id = nullptr;
    static bool hold_inc = false;
    if (clicked) {
        if (is_top_half) { *v += step; changed = true; hold_id = id; hold_inc = true; hold_timer = 0.f; }
        else { *v -= step; changed = true; hold_id = id; hold_inc = false; hold_timer = 0.f; }
    }
    if (held && hold_id == id) {
        hold_timer += ImGui::GetIO().DeltaTime;
        if (hold_timer > 0.35f) {
            if (hold_inc) *v += step; else *v -= step;
            changed = true;
            hold_timer = 0.3f;
        }
    } else if (!held && hold_id == id) {
        hold_id = nullptr;
        hold_timer = 0.f;
    }
    ImGui::PopID();
    return changed;
}

static bool InputFloatWithArrows(const char* label, float* v, float step = 1.f, const char* format = "%.1f")
{
    ImGui::PushID(label);
    bool changed = false;
    ImGui::BeginGroup();
    ImGui::PushItemWidth(65.f);
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(3.f, 2.f));
    changed = ImGui::InputFloat("##input", v, 0.f, 0.f, format, ImGuiInputTextFlags_None);
    ImGui::PopStyleVar();
    ImGui::PopItemWidth();
    ImGui::SameLine(0, 2.f);
    changed = DrawTriangleControl("##triangles", v, step) || changed;
    ImGui::EndGroup();
    ImGui::PopID();
    return changed;
}

static const char* KeyName(int k)
{
    switch (k) {
        case VK_LBUTTON: return "mouse1";
        case VK_RBUTTON: return "mouse2";
        case VK_MBUTTON: return "mouse3";
        case VK_XBUTTON1: return "mouse4";
        case VK_XBUTTON2: return "mouse5";
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: return "shift";
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: return "ctrl";
        case VK_MENU: return "alt";
        case VK_SPACE: return "space";
        case VK_CAPITAL: return "capslock";
        case VK_TAB: return "tab";
    }
    static char s[16];
    if ((k >= 'A' && k <= 'Z') || (k >= '0' && k <= '9')) { s[0] = (char)tolower(k); s[1] = 0; return s; }
    if (k >= VK_F1 && k <= VK_F12) { snprintf(s, sizeof(s), "f%d", k - VK_F1 + 1); return s; }
    snprintf(s, sizeof(s), "[%d]", k);
    return s;
}

static void KeyBindRow(const char* label, int* key)
{
    ImGui::Text("%s", label);
    ImGui::SameLine(200.f);
    ImGui::PushID(label);
    if (g_keyCapture == key) {
        ImGui::TextColored(ImVec4(1.f, 0.4f, 0.4f, 1.f), "press key...");
        for (int k = 1; k <= 0xFE; k++) {
            if (k == VK_INSERT) continue;
            BYTE keys[256] = {};
            if (!GetKeyboardState(keys)) break;
            if (keys[k] & 0x80) {
                if (k == VK_ESCAPE) { g_keyCapture = nullptr; break; }
                *key = k;
                g_keyCapture = nullptr;
                break;
            }
        }
    } else {
        char buf[32];
        snprintf(buf, sizeof(buf), "%s##btn", KeyName(*key));
        if (ImGui::Button(buf, ImVec2(80.f, 0.f))) g_keyCapture = key;
    }
    ImGui::PopID();
}

static void DrawVisualsPlayersTab()
{
    ImGui::Checkbox("2D box", &g_drawBoxes);
    ImGui::Checkbox("3D box", &g_draw3dBox);
    ImGui::Checkbox("Skeleton", &g_drawSkeleton);
    ImGui::Checkbox("Snap lines", &g_drawLines);
    ImGui::Checkbox("Head dot", &g_drawHeadDot);
    if (g_drawHeadDot) {
        ImGui::SameLine(180.f);
        InputFloatWithArrows("##headdotsz", &g_headDotSize, 0.5f, "%.1f");
    }
    ImGui::Checkbox("Facing line", &g_drawFacingLine);
    if (g_drawFacingLine) {
        ImGui::SameLine(180.f);
        InputFloatWithArrows("##facinglen", &g_facingLineLen, 0.25f, "%.1f m");
    }
    ImGui::Checkbox("Engine outline", &g_glow);
    if (g_glow) {
        ImGui::SameLine(180.f);
        ImGui::SetNextItemWidth(80.f);
        ImGui::ColorEdit3("##glowcol", g_glowColor, ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_NoLabel);
        ImGui::SameLine();
        ImGui::Text("Thickness"); ImGui::SameLine();
        InputFloatWithArrows("##glowthick", &g_glowThickness, 0.2f, "%.1f");
    }
    ImGui::Checkbox("Team check", &g_teamCheck);
    ImGui::Checkbox("Vis check", &g_visCheck);
    ImGui::Checkbox("HP bars", &g_drawHpBars);
}

static void DrawVisualsTagsTab()
{
    ImGui::Checkbox("Hero name", &g_drawHeroName);
    ImGui::Checkbox("Player name", &g_drawPlayerName);
    ImGui::Checkbox("Hero portrait", &g_drawHeroIcon);
    if (g_drawHeroIcon)
        ImGui::Checkbox("Portrait when hidden only", &g_heroIconHiddenOnly);
    ImGui::Text("Portrait size"); ImGui::SameLine(180.f);
    InputFloatWithArrows("##iconsize", &g_heroIconSize, 1.f, "%.0f");
    ImGui::Text("Skeleton rot"); ImGui::SameLine(180.f);
    InputFloatWithArrows("##skelrot", &g_skelRotOffset, 0.05f, "%.2f");
    ImGui::Checkbox("Ult bars", &g_ultBars);
    ImGui::Checkbox("Ult panel", &g_ultPanel);
    ImGui::Checkbox("Ally ult panel", &g_allyUltPanel);
    ImGui::Checkbox("Ability tracker", &g_abilityPanel);
    ImGui::Checkbox("Ability debug", &g_abilityDebug);
    ImGui::Checkbox("Distance tags", &g_distanceEsp);
    ImGui::Checkbox("Entity list", &g_drawEntList);

    const char* teamModes[] = { "Auto", "Team 1", "Team 2", "FFA" };
    int teamIdx = 0;
    if (g_teamOverride == 1) teamIdx = 1;
    else if (g_teamOverride == 2) teamIdx = 2;
    else if (g_teamOverride == 5) teamIdx = 3;
    ImGui::SetNextItemWidth(120.f);
    if (ImGui::Combo("Team", &teamIdx, teamModes, 4)) {
        g_teamOverride = (teamIdx == 1) ? 1 : (teamIdx == 2) ? 2 : (teamIdx == 3) ? 5 : 0;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Swap")) {
        if (g_teamOverride == 1) g_teamOverride = 2;
        else if (g_teamOverride == 2) g_teamOverride = 1;
        else g_teamOverride = (GetEffectiveLocalTeam() == 1) ? 2 : 1;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Auto")) g_teamOverride = 0;
}

static void DrawVisualsWorldTab()
{
    ImGui::Checkbox("Health packs", &g_drawHpPacks);
    ImGui::Checkbox("Pack boxes", &g_drawHpPackBoxes);
    ImGui::Checkbox("Pack labels", &g_drawHpPackLabels);
    ImGui::Text("Label offset"); ImGui::SameLine(180.f);
    InputFloatWithArrows("##hplabeloff", &g_hpPackLabelOffset, 2.f, "%.0f px");

    ImGui::Checkbox("Match timer", &g_drawMatchTimer);
    if (g_drawMatchTimer) {
        const char* styles[] = { "Minimal", "Center", "Strip" };
        ImGui::Combo("Style", &g_matchHudStyle, styles, IM_ARRAYSIZE(styles));
        ImGui::Checkbox("Transparent", &g_matchHudTransparent);
        ImGui::Checkbox("Timer", &g_matchHudShowTimer);
        ImGui::Checkbox("Map", &g_matchHudShowMap);
        ImGui::Checkbox("Phase", &g_matchHudShowPhase);
        ImGui::Checkbox("Ping", &g_matchHudShowPing);
    }
}

static void DrawMiscTab()
{
    ImGui::Checkbox("Flycam", &g_freecam);
    if (g_freecam) {
        ImGui::Text("Speed"); ImGui::SameLine(180.f);
        InputFloatWithArrows("##freecamspeed", &g_freecamSpeed, 1.f, "%.0f");
    }
    ImGui::Checkbox("Third person", &g_thirdPerson);
    if (g_thirdPerson) {
        ImGui::Text("Distance"); ImGui::SameLine(180.f);
        InputFloatWithArrows("##tpdist", &g_thirdPersonDist, 0.5f, "%.1f");
    }

    ImGui::Checkbox("Hero swap", &g_heroSwapAlert);
    ImGui::Checkbox("Ult ready", &g_ultReadyAlert);
    ImGui::Checkbox("Low HP pulse", &g_lowHpPulse);
    ImGui::Checkbox("Behind warning", &g_behindWarning);
    ImGui::Checkbox("Off-screen arrows", &g_offscreenArrows);
}

static void DrawInfoTab()
{
    ImGui::Text("Entities: %zu", g_entities.size());
    ImGui::Text("Screen valid: %u", g_debug.screen_valid_count);
    ImGui::Text("View matrix: %s", g_debug.vm_ok ? "OK" : "BAD");
    ImGui::Text("Team: %d (%s)", GetEffectiveLocalTeam(), g_teamOverride ? "manual" : "auto");
    ImGui::Text("Hero icons: %zu  |  Ability icons: %zu", g_heroIcons.size(), g_abilityIcons.size());
    if (g_assetHeroFilesOnDisk > 0)
        ImGui::TextDisabled("Portrait files: %d", g_assetHeroFilesOnDisk);

    ImGui::Spacing();
    ImGui::Text("Map: %s", g_worldInfo.mapName);
    ImGui::Text("Phase: %s  |  %s", g_worldInfo.phaseName, g_worldInfo.timeText);
    if (g_worldInfo.pingValid)
        ImGui::Text("Ping: %u ms", g_worldInfo.pingMs);
    ImGui::Text("Allies: %d  |  Enemies: %d", g_worldInfo.allyCount, g_worldInfo.enemyCount);

    ImGui::Spacing();
    if (ImGui::SmallButton("Reset reader")) {
        ResetReaderRuntime();
        SetCfgStatus("Reset");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload icons")) {
        ReloadAllIconAssets();
        SetCfgStatus("Icons reloaded");
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("Unload")) {
        g_requestUnload = true;
        g_running = false;
    }
    if (GetTickCount64() < g_cfgStatusUntil && g_cfgStatus[0])
        ImGui::TextDisabled("%s", g_cfgStatus);
}

static void DrawRadarTab()
{
    ImGui::Checkbox("Radar", &g_drawRadar);
    if (g_drawRadar) {
        ImGui::Text("Size"); ImGui::SameLine(120.f);
        InputFloatWithArrows("##radarsz", &g_radarSize, 5.f, "%.0f");
        ImGui::Text("Range"); ImGui::SameLine(120.f);
        InputFloatWithArrows("##radarrng", &g_radarRange, 5.f, "%.0f");
        ImGui::Checkbox("Show allies", &g_radarShowAllies);
    }

    ImGui::Checkbox("Off-screen arrows", &g_offscreenArrows);
    if (g_offscreenArrows)
        ImGui::Checkbox("Use marker radius", &g_offscreenUseFov);
    ImGui::Checkbox("Marker circle", &g_drawMarkerCircle);
    if (g_drawMarkerCircle || g_offscreenUseFov) {
        ImGui::Text("Radius"); ImGui::SameLine(120.f);
        InputFloatWithArrows("##markerfov", &g_markerFov, 5.f, "%.0f");
    }
    ImGui::Checkbox("FOV enemy count", &g_fovThreatCount);
    ImGui::Checkbox("Lowest HP in FOV", &g_priorityMarker);
    ImGui::Checkbox("Behind warning", &g_behindWarning);
    ImGui::Checkbox("Hero swap popup", &g_heroSwapAlert);
    ImGui::Checkbox("Ult ready popup", &g_ultReadyAlert);
}

static void DrawConfigTab()
{
    static char nameBuf[64] = "default";
    static std::vector<std::string> cfgList;
    static int selected = 0;

    if (nameBuf[0] == 0 || !strcmp(nameBuf, "default"))
        strncpy(nameBuf, g_activeConfig, sizeof(nameBuf) - 1);

    CfgList(cfgList);
    if (selected >= (int)cfgList.size()) selected = 0;

    ImGui::Text("Config name");
    ImGui::SetNextItemWidth(-1.f);
    ImGui::InputText("##cfgname", nameBuf, sizeof(nameBuf));

    if (ImGui::Button("Save", ImVec2(80.f, 0.f))) {
        if (CfgSavNamed(nameBuf)) {
            SetCfgStatus("Saved");
            strncpy(g_activeConfig, nameBuf, sizeof(g_activeConfig) - 1);
            g_activeConfig[sizeof(g_activeConfig) - 1] = 0;
        } else SetCfgStatus("Save failed");
        CfgList(cfgList);
        for (int i = 0; i < (int)cfgList.size(); i++) {
            if (cfgList[i] == nameBuf) { selected = i; break; }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh", ImVec2(80.f, 0.f))) CfgList(cfgList);

    ImGui::Spacing();
    ImGui::Text("Saved configs");
    ImGui::BeginChild("##cfglist", ImVec2(0, 220.f), true);
    for (int i = 0; i < (int)cfgList.size(); i++) {
        bool isSel = (selected == i);
        bool isActive = !strcmp(g_activeConfig, cfgList[i].c_str());
        char label[80];
        snprintf(label, sizeof(label), "%s%s", cfgList[i].c_str(), isActive ? "  *" : "");
        if (ImGui::Selectable(label, isSel)) {
            selected = i;
            strncpy(nameBuf, cfgList[i].c_str(), sizeof(nameBuf) - 1);
        }
    }
    if (cfgList.empty())
        ImGui::TextDisabled("No configs");
    ImGui::EndChild();

    if (ImGui::Button("Load", ImVec2(80.f, 0.f))) {
        if (selected >= 0 && selected < (int)cfgList.size()) {
            if (CfgLdNamed(cfgList[selected].c_str())) SetCfgStatus("Loaded");
            else SetCfgStatus("Load failed");
            strncpy(nameBuf, cfgList[selected].c_str(), sizeof(nameBuf) - 1);
        } else if (nameBuf[0]) {
            if (CfgLdNamed(nameBuf)) SetCfgStatus("Loaded");
            else SetCfgStatus("Not found");
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete", ImVec2(80.f, 0.f))) {
        const char* del = (selected >= 0 && selected < (int)cfgList.size())
            ? cfgList[selected].c_str() : nameBuf;
        if (del[0] && CfgDeleteNamed(del)) {
            SetCfgStatus("Deleted");
            CfgList(cfgList);
            selected = 0;
        } else SetCfgStatus("Delete failed");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset", ImVec2(80.f, 0.f))) {
        CfgReset();
        SetCfgStatus("Defaults");
    }

    if (GetTickCount64() < g_cfgStatusUntil && g_cfgStatus[0])
        ImGui::TextDisabled("%s", g_cfgStatus);
}

void DrawRsGui()
{
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.f, 520.f), ImGuiCond_Always);
    ImGui::PushFont(g_menuFont ? g_menuFont : ImGui::GetFont());
    ImGui::Begin("Overwatch Internal Private", nullptr,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.f, 0.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2.f, 2.f));

    ImGui::Text("Overwatch Internal Private");
    float window_width = ImGui::GetWindowWidth();
    ImGui::SameLine(window_width - 25.f);
    if (DrawTabButton("X", false, 20.f)) g_menuOpen = false;

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 window_pos = ImGui::GetWindowPos();
    ImVec2 window_size = ImGui::GetWindowSize();
    float separator_y = ImGui::GetCursorScreenPos().y;
    ImU32 separator_color = ImGui::GetColorU32(ImGuiCol_Separator);
    draw_list->AddLine(ImVec2(window_pos.x, separator_y),
        ImVec2(window_pos.x + window_size.x, separator_y), separator_color, 1.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.f);

    float tab_width = 60.f;
    struct SeparatorPos { float x, y, y_end; };
    SeparatorPos separators[8];
    int separator_count = 0;
    ImVec2 rect_min, rect_max;

    ImGui::SetCursorPosX(0.f);
    if (DrawTabButton("Visuals", g_rsMenu.main_tab == 1 && g_rsMenu.top_tab == 0, tab_width)) {
        g_rsMenu.main_tab = 1; g_rsMenu.top_tab = 0;
    }
    rect_max = ImGui::GetItemRectMax(); rect_min = ImGui::GetItemRectMin();
    separators[separator_count++] = { rect_max.x, rect_min.y, rect_max.y };
    ImGui::SameLine(0, 0.f);

    if (DrawTabButton("Misc", g_rsMenu.main_tab == 2 && g_rsMenu.top_tab == 0, tab_width)) {
        g_rsMenu.main_tab = 2; g_rsMenu.top_tab = 0;
    }
    rect_max = ImGui::GetItemRectMax(); rect_min = ImGui::GetItemRectMin();
    separators[separator_count++] = { rect_max.x, rect_min.y, rect_max.y };

    float right_start = window_width - tab_width * 3.f;
    ImGui::SameLine(right_start);

    if (DrawTabButton("Info", g_rsMenu.top_tab == 1, tab_width)) g_rsMenu.top_tab = 1;
    rect_min = ImGui::GetItemRectMin(); rect_max = ImGui::GetItemRectMax();
    separators[separator_count++] = { rect_min.x, rect_min.y, rect_max.y };
    separators[separator_count++] = { rect_max.x, rect_min.y, rect_max.y };
    ImGui::SameLine(0, 0.f);

    if (DrawTabButton("Config", g_rsMenu.top_tab == 2, tab_width)) g_rsMenu.top_tab = 2;
    rect_max = ImGui::GetItemRectMax(); rect_min = ImGui::GetItemRectMin();
    separators[separator_count++] = { rect_max.x, rect_min.y, rect_max.y };
    ImGui::SameLine(0, 0.f);

    if (DrawTabButton("Radar", g_rsMenu.top_tab == 3, tab_width)) g_rsMenu.top_tab = 3;

    for (int i = 0; i < separator_count; i++)
        draw_list->AddLine(ImVec2(separators[i].x, separators[i].y),
            ImVec2(separators[i].x, separators[i].y_end), separator_color, 1.f);

    float separator_bottom_y = ImGui::GetCursorScreenPos().y;
    draw_list->AddLine(ImVec2(window_pos.x, separator_bottom_y),
        ImVec2(window_pos.x + window_size.x, separator_bottom_y), separator_color, 1.f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.f);

    if (g_rsMenu.main_tab == 1 && g_rsMenu.top_tab == 0) {
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.f);
        ImGui::SetCursorPosX(0.f);
        if (DrawTabButton("Players", g_rsMenu.visuals_subtab == 0, tab_width)) g_rsMenu.visuals_subtab = 0;
        rect_max = ImGui::GetItemRectMax(); rect_min = ImGui::GetItemRectMin();
        SeparatorPos sub_sep = { rect_max.x, rect_min.y, rect_max.y };
        ImGui::SameLine(0, 0.f);
        if (DrawTabButton("Tags", g_rsMenu.visuals_subtab == 1, tab_width)) g_rsMenu.visuals_subtab = 1;
        rect_max = ImGui::GetItemRectMax(); rect_min = ImGui::GetItemRectMin();
        SeparatorPos tags_sep = { rect_max.x, rect_min.y, rect_max.y };
        ImGui::SameLine(0, 0.f);
        if (DrawTabButton("World", g_rsMenu.visuals_subtab == 2, tab_width)) g_rsMenu.visuals_subtab = 2;
        rect_max = ImGui::GetItemRectMax(); rect_min = ImGui::GetItemRectMin();
        SeparatorPos world_sep = { rect_max.x, rect_min.y, rect_max.y };
        draw_list->AddLine(ImVec2(sub_sep.x, sub_sep.y), ImVec2(sub_sep.x, sub_sep.y_end), separator_color, 1.f);
        draw_list->AddLine(ImVec2(tags_sep.x, tags_sep.y), ImVec2(tags_sep.x, tags_sep.y_end), separator_color, 1.f);
        draw_list->AddLine(ImVec2(world_sep.x, world_sep.y), ImVec2(world_sep.x, world_sep.y_end), separator_color, 1.f);
        float subtab_sep_y = ImGui::GetCursorScreenPos().y;
        draw_list->AddLine(ImVec2(window_pos.x, subtab_sep_y),
            ImVec2(window_pos.x + window_size.x, subtab_sep_y), separator_color, 1.f);
        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 1.f);
    }

    ImGui::PopStyleVar(2);
    ImGui::Spacing();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.f, 3.f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.f, 3.f));
    ImGui::BeginChild("##content", ImVec2(0, 0), false);

    if (g_rsMenu.top_tab == 1) DrawInfoTab();
    else if (g_rsMenu.top_tab == 2) DrawConfigTab();
    else if (g_rsMenu.top_tab == 3) DrawRadarTab();
    else if (g_rsMenu.main_tab == 1) {
        if (g_rsMenu.visuals_subtab == 0) DrawVisualsPlayersTab();
        else if (g_rsMenu.visuals_subtab == 1) DrawVisualsTagsTab();
        else DrawVisualsWorldTab();
    }
    else if (g_rsMenu.main_tab == 2) DrawMiscTab();
    else {
        g_rsMenu.main_tab = 1;
        DrawVisualsPlayersTab();
    }

    ImGui::EndChild();
    ImGui::PopStyleVar(2);
    ImGui::End();
    ImGui::PopFont();
}
