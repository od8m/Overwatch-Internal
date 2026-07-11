// imgui-notify — Veil Edition
// Modern glassmorphism toasts with neon accents, icons, and progress bar

#ifndef IMGUI_NOTIFY
#define IMGUI_NOTIFY

#pragma once
#include <vector>
#include <string>
#include "font_awesome_5.h"
#include "fa_solid_900.h"

#define NOTIFY_MAX_MSG_LENGTH           4096
#define NOTIFY_PADDING_X                24.f
#define NOTIFY_PADDING_Y                24.f
#define NOTIFY_PADDING_MESSAGE_Y        10.f
#define NOTIFY_SLIDE_TIME               300
#define NOTIFY_DEFAULT_DISMISS          3000
#define NOTIFY_TOAST_WIDTH              280.f
#define NOTIFY_TOAST_FLAGS              ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoInputs | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoMove

#define NOTIFY_INLINE                   inline
#define NOTIFY_NULL_OR_EMPTY(str)       (!str || !strlen(str))
#define NOTIFY_FORMAT(fn, format, ...)  if (format) { va_list args; va_start(args, format); fn(format, args, ##__VA_ARGS__); va_end(args); }

typedef int ImGuiToastType;
typedef int ImGuiToastPhase;

enum ImGuiToastType_
{
    ImGuiToastType_None,
    ImGuiToastType_Success,
    ImGuiToastType_Warning,
    ImGuiToastType_Error,
    ImGuiToastType_Info,
    ImGuiToastType_COUNT
};

enum ImGuiToastPhase_
{
    ImGuiToastPhase_FadeIn,
    ImGuiToastPhase_Wait,
    ImGuiToastPhase_FadeOut,
    ImGuiToastPhase_Expired,
    ImGuiToastPhase_COUNT
};

static NOTIFY_INLINE float notify_ease(float t)
{
    t = t < 0.f ? 0.f : (t > 1.f ? 1.f : t);
    return t * t * (3.f - 2.f * t);
}

class ImGuiToast
{
private:
    ImGuiToastType  type          = ImGuiToastType_None;
    char            title[NOTIFY_MAX_MSG_LENGTH];
    char            content[NOTIFY_MAX_MSG_LENGTH];
    int             dismiss_time  = NOTIFY_DEFAULT_DISMISS;
    uint64_t        creation_time = 0;

private:
    NOTIFY_INLINE auto set_title(const char* format, va_list args)   { vsnprintf(this->title,   sizeof(this->title),   format, args); }
    NOTIFY_INLINE auto set_content(const char* format, va_list args) { vsnprintf(this->content, sizeof(this->content), format, args); }

public:
    NOTIFY_INLINE auto set_title(const char* format, ...)   -> void { NOTIFY_FORMAT(this->set_title,   format); }
    NOTIFY_INLINE auto set_content(const char* format, ...) -> void { NOTIFY_FORMAT(this->set_content, format); }
    NOTIFY_INLINE auto set_type(const ImGuiToastType& t)    -> void { IM_ASSERT(t < ImGuiToastType_COUNT); this->type = t; }

public:
    NOTIFY_INLINE auto get_title()         -> char*                  { return this->title; }
    NOTIFY_INLINE auto get_content()       -> char*                  { return this->content; }
    NOTIFY_INLINE auto get_type()          -> const ImGuiToastType&  { return this->type; }
    NOTIFY_INLINE auto get_dismiss_time()  -> int                    { return this->dismiss_time; }
    NOTIFY_INLINE auto get_elapsed_time()                            { return GetTickCount64() - this->creation_time; }

    NOTIFY_INLINE auto get_default_title() -> char*
    {
        if (!strlen(this->title))
        {
            if (strlen(this->content))
                return this->content;
            switch (this->type)
            {
            case ImGuiToastType_None:    return NULL;
            case ImGuiToastType_Success: return (char*)skCrypt("Success").decrypt();
            case ImGuiToastType_Warning: return (char*)skCrypt("Warning").decrypt();
            case ImGuiToastType_Error:   return (char*)skCrypt("Error").decrypt();
            case ImGuiToastType_Info:    return (char*)skCrypt("Info").decrypt();
            }
        }
        return this->title;
    }

    NOTIFY_INLINE const char* get_icon()
    {
        switch (this->type)
        {
        case ImGuiToastType_Success: return ICON_FA_CHECK_CIRCLE;
        case ImGuiToastType_Warning: return ICON_FA_EXCLAMATION_TRIANGLE;
        case ImGuiToastType_Error:   return ICON_FA_TIMES_CIRCLE;
        case ImGuiToastType_Info:    return ICON_FA_INFO_CIRCLE;
        default:                     return ICON_FA_BELL;
        }
    }

    NOTIFY_INLINE ImVec4 get_accent_color()
    {
        switch (this->type)
        {
        case ImGuiToastType_Success: return ImVec4(0.0f, 0.9f, 0.7f, 1.0f);
        case ImGuiToastType_Warning: return ImVec4(1.0f, 0.75f, 0.0f, 1.0f);
        case ImGuiToastType_Error:   return ImVec4(1.0f, 0.25f, 0.25f, 1.0f);
        case ImGuiToastType_Info:    return ImVec4(0.0f, 0.85f, 1.0f, 1.0f);
        default:                     return ImVec4(0.5f, 0.5f, 0.6f, 1.0f);
        }
    }

    NOTIFY_INLINE auto get_phase() -> const ImGuiToastPhase&
    {
        const auto elapsed = get_elapsed_time();
        if      (elapsed > (uint64_t)(NOTIFY_SLIDE_TIME + dismiss_time + NOTIFY_SLIDE_TIME)) return ImGuiToastPhase_Expired;
        else if (elapsed > (uint64_t)(NOTIFY_SLIDE_TIME + dismiss_time))                     return ImGuiToastPhase_FadeOut;
        else if (elapsed > (uint64_t)(NOTIFY_SLIDE_TIME))                                    return ImGuiToastPhase_Wait;
        else                                                                                  return ImGuiToastPhase_FadeIn;
    }

    NOTIFY_INLINE auto get_slide_t() -> float
    {
        const auto phase   = get_phase();
        const auto elapsed = (float)get_elapsed_time();
        const float st     = (float)NOTIFY_SLIDE_TIME;

        if (phase == ImGuiToastPhase_FadeIn)
        {
            float t = 1.f - (elapsed / st);
            return notify_ease(t);
        }
        else if (phase == ImGuiToastPhase_FadeOut)
        {
            float t = (elapsed - st - (float)dismiss_time) / st;
            return notify_ease(t);
        }
        return 0.f;
    }

    NOTIFY_INLINE auto get_progress() -> float
    {
        const auto elapsed = (float)get_elapsed_time();
        const float total = (float)(NOTIFY_SLIDE_TIME + this->get_dismiss_time() + NOTIFY_SLIDE_TIME);
        return 1.f - ImClamp(elapsed / total, 0.f, 1.f);
    }

public:
    ImGuiToast(ImGuiToastType type, int dismiss_time = NOTIFY_DEFAULT_DISMISS)
    {
        IM_ASSERT(type < ImGuiToastType_COUNT);
        this->type          = type;
        this->dismiss_time  = dismiss_time;
        this->creation_time = GetTickCount64();
        memset(this->title,   0, sizeof(this->title));
        memset(this->content, 0, sizeof(this->content));
    }

    ImGuiToast(ImGuiToastType type, const char* format, ...)
        : ImGuiToast(type) { NOTIFY_FORMAT(this->set_content, format); }

    ImGuiToast(ImGuiToastType type, int dismiss_time, const char* format, ...)
        : ImGuiToast(type, dismiss_time) { NOTIFY_FORMAT(this->set_content, format); }
};

namespace ImGui
{
    NOTIFY_INLINE std::vector<ImGuiToast> notifications;

    NOTIFY_INLINE VOID InsertNotification(const ImGuiToast& toast)
    {
        notifications.push_back(toast);
    }

    NOTIFY_INLINE VOID RemoveNotification(int index)
    {
        notifications.erase(notifications.begin() + index);
    }

    NOTIFY_INLINE VOID RenderNotifications()
    {
        const auto vp_size = GetMainViewport()->Size;
        const float slide_dist = 340.f;
        float height = 0.f;

        for (int i = 0; i < (int)notifications.size(); i++)
        {
            auto* t = &notifications[i];

            if (t->get_phase() == ImGuiToastPhase_Expired)
            {
                RemoveNotification(i--);
                continue;
            }

            const float slide_t  = t->get_slide_t();
            const float offset_x = slide_t * slide_dist;
            const float alpha    = t->get_phase() == ImGuiToastPhase_FadeIn
                                     ? ImClamp((float)t->get_elapsed_time() / (float)NOTIFY_SLIDE_TIME, 0.f, 1.f)
                                   : t->get_phase() == ImGuiToastPhase_FadeOut
                                      ? ImClamp(1.f - ((float)t->get_elapsed_time() - (float)NOTIFY_SLIDE_TIME - (float)t->get_dismiss_time()) / (float)NOTIFY_SLIDE_TIME, 0.f, 1.f)
                                     : 1.f;

            char window_name[50];
            sprintf_s(window_name, "##VEIL_TOAST%d", i);

            const ImVec4 accent = t->get_accent_color();

            PushStyleColor(ImGuiCol_WindowBg,      ImVec4(0.055f, 0.055f, 0.090f, 0.95f * alpha));
            PushStyleColor(ImGuiCol_Border,         ImVec4(accent.x, accent.y, accent.z, 0.3f * alpha));
            PushStyleColor(ImGuiCol_Text,           ImVec4(0.85f, 0.85f, 0.92f, alpha));
            PushStyleVar(ImGuiStyleVar_WindowRounding, 8.f);
            PushStyleVar(ImGuiStyleVar_WindowPadding,  ImVec2(16.f, 12.f));
            PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);

            SetNextWindowPos(
                ImVec2(vp_size.x - NOTIFY_PADDING_X + offset_x,
                       vp_size.y - NOTIFY_PADDING_Y - height),
                ImGuiCond_Always,
                ImVec2(1.0f, 1.0f)
            );
            SetNextWindowBgAlpha(0.95f * alpha);

            Begin(window_name, NULL, NOTIFY_TOAST_FLAGS);

            // ── Icon + Title row ──
            const auto icon   = t->get_icon();
            const auto title  = t->get_title();
            const auto content = t->get_content();
            const auto default_title = t->get_default_title();

            const char* display_title = nullptr;
            if (!NOTIFY_NULL_OR_EMPTY(title))         display_title = title;
            else if (!NOTIFY_NULL_OR_EMPTY(default_title)) display_title = default_title;

            // Icon
            PushStyleColor(ImGuiCol_Text, ImVec4(accent.x, accent.y, accent.z, alpha));
            Text("%s", icon);
            PopStyleColor();

            SameLine(0, 8.f);

            // Title
            if (display_title)
            {
                PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 1.0f, alpha));
                TextUnformatted(display_title);
                PopStyleColor();
            }

            // Content (only if separate from title)
            if (!NOTIFY_NULL_OR_EMPTY(content) && !NOTIFY_NULL_OR_EMPTY(title))
            {
                Spacing();
                PushTextWrapPos(NOTIFY_TOAST_WIDTH - 32.f);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.7f, alpha * 0.85f));
                TextUnformatted(content);
                PopStyleColor();
                PopTextWrapPos();
            }
            else if (!NOTIFY_NULL_OR_EMPTY(content) && NOTIFY_NULL_OR_EMPTY(title))
            {
                Spacing();
                PushTextWrapPos(NOTIFY_TOAST_WIDTH - 32.f);
                PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.7f, alpha * 0.85f));
                TextUnformatted(content);
                PopStyleColor();
                PopTextWrapPos();
            }

            // ── Progress bar ──
            Spacing();
            const float prog_w = GetContentRegionAvail().x;
            const float prog_h = 2.f;
            const ImVec2 prog_pos = GetCursorScreenPos();
            const ImVec4 bg_col = ImVec4(0.12f, 0.12f, 0.18f, 0.5f * alpha);
            GetWindowDrawList()->AddRectFilled(prog_pos, ImVec2(prog_pos.x + prog_w, prog_pos.y + prog_h), ImColor(bg_col), 1.f);
            const float fill_w = prog_w * t->get_progress();
            if (fill_w > 0.5f)
            {
                GetWindowDrawList()->AddRectFilled(prog_pos, ImVec2(prog_pos.x + fill_w, prog_pos.y + prog_h), ImColor(accent.x, accent.y, accent.z, 0.7f * alpha), 1.f);
                // glow
                GetWindowDrawList()->AddRectFilled(prog_pos, ImVec2(prog_pos.x + fill_w, prog_pos.y + prog_h), ImColor(accent.x, accent.y, accent.z, 0.15f * alpha), 2.f);
            }

            height += GetWindowHeight() + NOTIFY_PADDING_MESSAGE_Y;
            End();

            PopStyleVar(3);
            PopStyleColor(3);
        }
    }

    NOTIFY_INLINE VOID MergeIconsWithLatestFont(float font_size, bool FontDataOwnedByAtlas = false)
    {
        static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };

        ImFontConfig icons_config;
        icons_config.MergeMode            = true;
        icons_config.PixelSnapH           = true;
        icons_config.FontDataOwnedByAtlas = FontDataOwnedByAtlas;

        GetIO().Fonts->AddFontFromMemoryTTF(
            (void*)fa_solid_900, sizeof(fa_solid_900),
            font_size, &icons_config, icons_ranges);
    }
}

#endif
