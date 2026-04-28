#include "menu.h"

#include <fstream>
#include <cstdio>
#include <cstring>

// ================================================================
//  GAMBODJAN Menu — FOV Changer (dark theme, red accents)
// ================================================================

static const ImVec4 col_bg        = ImVec4(0.11f, 0.10f, 0.10f, 1.00f);
static const ImVec4 col_sidebar   = ImVec4(0.08f, 0.07f, 0.07f, 1.00f);
static const ImVec4 col_accent    = ImVec4(0.80f, 0.18f, 0.18f, 1.00f);
static const ImVec4 col_accent_hi = ImVec4(0.90f, 0.28f, 0.28f, 1.00f);
static const ImVec4 col_accent_dk = ImVec4(0.55f, 0.12f, 0.12f, 1.00f);
static const ImVec4 col_text      = ImVec4(0.90f, 0.90f, 0.90f, 1.00f);
static const ImVec4 col_text_dim  = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
static const ImVec4 col_panel     = ImVec4(0.14f, 0.13f, 0.13f, 1.00f);
static const ImVec4 col_border    = ImVec4(0.22f, 0.20f, 0.20f, 1.00f);
static const ImVec4 col_green     = ImVec4(0.20f, 0.80f, 0.20f, 1.00f);
static const ImVec4 col_red       = ImVec4(0.90f, 0.20f, 0.20f, 1.00f);
static const ImVec4 col_toggle_on = ImVec4(0.80f, 0.18f, 0.18f, 1.00f);
static const ImVec4 col_toggle_off= ImVec4(0.30f, 0.30f, 0.30f, 1.00f);

// ================================================================
//  Config save/load
// ================================================================
void menu::SaveConfig(const char* filename) {
    char path[512];
    snprintf(path, sizeof(path), "%s.cfg", filename);
    FILE* f = fopen(path, "w");
    if (!f) return;
    auto& s = state;

    fprintf(f, "fov_enabled=%d\n", s.fov_enabled);
    fprintf(f, "fov_value=%.1f\n", s.fov_value);
    fprintf(f, "menu_opacity=%.2f\n", s.menu_opacity);
    fprintf(f, "save_on_close=%d\n", s.save_on_close);
    fprintf(f, "theme_index=%d\n", s.theme_index);

    fclose(f);
}

void menu::LoadConfig(const char* filename) {
    char path[512];
    snprintf(path, sizeof(path), "%s.cfg", filename);
    FILE* f = fopen(path, "r");
    if (!f) return;
    auto& s = state;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        char key[128]; char val[128];
        if (sscanf(line, "%127[^=]=%127s", key, val) != 2) continue;

        if (!strcmp(key, "fov_enabled"))         s.fov_enabled = atoi(val);
        if (!strcmp(key, "fov_value"))            s.fov_value = (float)atof(val);
        if (!strcmp(key, "menu_opacity"))          s.menu_opacity = (float)atof(val);
        if (!strcmp(key, "save_on_close"))         s.save_on_close = atoi(val);
        if (!strcmp(key, "theme_index"))           s.theme_index = atoi(val);
    }
    fclose(f);
}

// ================================================================
//  Custom toggle widget
// ================================================================
static bool ToggleSwitch(const char* label, bool* v) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, nullptr, true);

    const float height = ImGui::GetFrameHeight() * 0.75f;
    const float width = height * 1.8f;
    const float radius = height * 0.5f;

    const ImVec2 pos = window->DC.CursorPos;
    const ImRect total_bb(pos, ImVec2(pos.x + width + style.ItemInnerSpacing.x + label_size.x, pos.y + height));
    ImGui::ItemSize(total_bb, style.FramePadding.y);
    if (!ImGui::ItemAdd(total_bb, id)) return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(total_bb, id, &hovered, &held);
    if (pressed) { *v = !*v; ImGui::MarkItemEdited(id); }

    const ImRect track(pos, ImVec2(pos.x + width, pos.y + height));
    const ImU32 track_col = ImGui::GetColorU32(*v ? col_toggle_on : col_toggle_off);
    window->DrawList->AddRectFilled(track.Min, track.Max, track_col, radius);

    const float knob_x = *v ? (pos.x + width - radius) : (pos.x + radius);
    window->DrawList->AddCircleFilled(ImVec2(knob_x, pos.y + radius), radius - 2.0f,
        ImGui::GetColorU32(ImVec4(1, 1, 1, 1)));

    ImGui::RenderText(ImVec2(pos.x + width + style.ItemInnerSpacing.x, pos.y + (height - label_size.y) * 0.5f), label);
    return pressed;
}

// ================================================================
//  Icon button (sidebar)
// ================================================================
static bool IconButton(const char* label, bool selected, float size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        selected ? col_accent    : ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered,  selected ? col_accent_hi : ImVec4(0.2f, 0.2f, 0.2f, 0.5f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,   col_accent_dk);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
    bool clicked = ImGui::Button(label, ImVec2(size, size));
    ImGui::PopStyleVar();
    ImGui::PopStyleColor(3);
    return clicked;
}

// ================================================================
//  FOV Tab
// ================================================================
static void RenderFOV() {
    using namespace menu;
    auto& s = state;

    const char* sub_labels[] = { "FOV CHANGER", "OFFSETS INFO" };
    float sub_w = ImGui::GetContentRegionAvail().x * 0.5f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    for (int i = 0; i < 2; i++) {
        if (i > 0) ImGui::SameLine();
        bool sel = ((int)s.current_sub == i);
        ImGui::PushStyleColor(ImGuiCol_Button,       sel ? col_accent : col_panel);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col_accent_hi);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  col_accent_dk);
        if (ImGui::Button(sub_labels[i], ImVec2(sub_w, 30)))
            s.current_sub = (SubTab)i;
        ImGui::PopStyleColor(3);
    }
    ImGui::PopStyleVar();
    ImGui::Spacing(); ImGui::Spacing();

    if (s.current_sub == SubTab::Sub1) {
        // --- Camera Distance (Dota 2 zoom) ---
        ImGui::TextColored(col_accent, "Camera Distance Control");
        ImGui::Spacing();

        ToggleSwitch("Camera Hack", &s.fov_enabled);
        ImGui::Spacing();

        if (s.fov_enabled) {
            ImGui::TextColored(col_text_dim, "Camera Distance");
            ImGui::PushStyleColor(ImGuiCol_SliderGrab, col_accent);
            ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, col_accent_hi);
            ImGui::SliderFloat("##fov_val", &s.fov_value, s.fov_min, s.fov_max, "%.0f");
            ImGui::PopStyleColor(2);
            ImGui::Spacing();

            // Preset buttons
            ImGui::TextColored(col_text_dim, "Presets:");
            ImGui::SameLine();
            if (ImGui::SmallButton("1200")) s.fov_value = 1200.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("1800")) s.fov_value = 1800.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("2400")) s.fov_value = 2400.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("3000")) s.fov_value = 3000.f;
            ImGui::SameLine();
            if (ImGui::SmallButton("5000")) s.fov_value = 5000.f;
            ImGui::Spacing(); ImGui::Spacing();

            // Status
            ImGui::Separator();
            ImGui::Spacing();
            ImGui::TextColored(col_green, "Camera Distance: %.0f", s.fov_value);
            ImGui::TextColored(col_text_dim, "Default: 1200 | Offset: 0x270");
        } else {
            ImGui::TextColored(col_text_dim, "[OFF] Enable to change camera distance");
            ImGui::Spacing();
            ImGui::TextColored(col_text_dim, "CDOTA_Camera offsets:");
            ImGui::BulletText("m_Distance: 0x270");
            ImGui::BulletText("m_LookAtPos: 0x278");
            ImGui::BulletText("m_MaxPitch: 0x264");
            ImGui::BulletText("m_MinPitch: 0x268");
        }
    } else {
        // --- Offsets Info sub-tab ---
        ImGui::TextColored(col_accent, "Source2 Dumper Offsets");
        ImGui::TextColored(col_text_dim, "Generated: 2026-03-30");
        ImGui::Spacing(); ImGui::Separator(); ImGui::Spacing();

        ImGui::TextColored(col_accent, "client.dll");
        ImGui::BulletText("dwEntityList:       0x%X", 0x66DDF48);
        ImGui::BulletText("dwGameEntitySystem: 0x%X", 0x66DDF48);
        ImGui::BulletText("dwGlobalVars:       0x%X", 0x5C9B2D8);
        ImGui::BulletText("dwLocalPlayerPawn:  0x%X", 0x5CA6508);
        ImGui::BulletText("dwPrediction:       0x%X", 0x5CA6430);
        ImGui::BulletText("dwSensitivity:      0x%X", 0x63845E8);
        ImGui::BulletText("dwViewMatrix:       0x%X", 0x638C210);
        ImGui::BulletText("dwViewRender:       0x%X", 0x638B8F8);
        ImGui::Spacing();

        ImGui::TextColored(col_accent, "engine2.dll");
        ImGui::BulletText("dwBuildNumber:       0x%X", 0x610CA4);
        ImGui::BulletText("dwNetworkGameClient: 0x%X", 0x90E500);
        ImGui::BulletText("dwWindowWidth:       0x%X", 0x9128D8);
        ImGui::BulletText("dwWindowHeight:      0x%X", 0x9128DC);
        ImGui::Spacing();

        ImGui::TextColored(col_accent, "inputsystem.dll");
        ImGui::BulletText("dwInputSystem: 0x%X", 0x42B50);
        ImGui::Spacing();

        ImGui::TextColored(col_accent, "soundsystem.dll");
        ImGui::BulletText("dwSoundSystem: 0x%X", 0x513380);
    }
}

// ================================================================
//  Settings Tab
// ================================================================
static void RenderSettings() {
    using namespace menu;
    auto& s = state;

    const char* sub_labels[] = { "GENERAL", "CONFIG" };
    float sub_w = ImGui::GetContentRegionAvail().x * 0.5f;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    for (int i = 0; i < 2; i++) {
        if (i > 0) ImGui::SameLine();
        bool sel = ((int)s.current_sub == i);
        ImGui::PushStyleColor(ImGuiCol_Button,       sel ? col_accent : col_panel);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col_accent_hi);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  col_accent_dk);
        if (ImGui::Button(sub_labels[i], ImVec2(sub_w, 30)))
            s.current_sub = (SubTab)i;
        ImGui::PopStyleColor(3);
    }
    ImGui::PopStyleVar();
    ImGui::Spacing(); ImGui::Spacing();

    if (s.current_sub == SubTab::Sub1) {
        ImGui::TextColored(col_accent, "Menu Settings");
        ImGui::Spacing();

        ImGui::TextColored(col_text_dim, "Opacity");
        ImGui::PushStyleColor(ImGuiCol_SliderGrab, col_accent);
        ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, col_accent_hi);

        float pct = s.menu_opacity * 100.f;
        if (ImGui::SliderFloat("##opacity", &pct, 10.f, 100.f, "%.0f%%"))
            s.menu_opacity = pct / 100.f;
        ImGui::SameLine();
        ImGui::ColorButton("##opc", col_accent, ImGuiColorEditFlags_NoTooltip, ImVec2(16, 16));
        ImGui::PopStyleColor(2);
        ImGui::Spacing();

        ToggleSwitch("Save config on close", &s.save_on_close);
        ImGui::Spacing();

        const char* themes[] = { "Red (Default)", "Blue", "Green", "Purple" };
        ImGui::TextColored(col_text_dim, "Theme");
        ImGui::PushItemWidth(200);
        ImGui::Combo("##theme", &s.theme_index, themes, IM_ARRAYSIZE(themes));
        ImGui::PopItemWidth();
        ImGui::Spacing(); ImGui::Spacing();

        ImGui::TextColored(col_accent, "Hotkeys");
        ImGui::Spacing();
        ImGui::BulletText("INSERT  - Toggle menu");
        ImGui::BulletText("END     - Unload DLL");
    } else {
        ImGui::TextColored(col_accent, "Config Manager");
        ImGui::Spacing();

        ImGui::TextColored(col_text_dim, "Config name:");
        ImGui::PushItemWidth(200);
        ImGui::InputText("##cfgname", s.config_name, sizeof(s.config_name));
        ImGui::PopItemWidth();
        ImGui::Spacing();

        bool save = false;

        ImGui::PushStyleColor(ImGuiCol_Button, col_accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col_accent_hi);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, col_accent_dk);
        if (ImGui::Button("Save", ImVec2(90, 28))) save = true;
        ImGui::SameLine();
        if (ImGui::Button("Load", ImVec2(90, 28))) LoadConfig(s.config_name);
        ImGui::SameLine();
        if (ImGui::Button("Reset", ImVec2(90, 28))) s = MenuState{};
        ImGui::PopStyleColor(3);

        if (save) SaveConfig(s.config_name);
    }
}

// ================================================================
//  Style
// ================================================================
void menu::ApplyStyle() {
    auto& style = ImGui::GetStyle();
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.GrabRounding      = 3.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 6.0f;
    style.WindowBorderSize  = 1.0f;
    style.FramePadding      = ImVec2(6, 4);
    style.ItemSpacing       = ImVec2(8, 6);
    style.ScrollbarSize     = 12.0f;

    auto& c = style.Colors;
    c[ImGuiCol_WindowBg]        = col_bg;
    c[ImGuiCol_ChildBg]         = col_panel;
    c[ImGuiCol_Border]          = col_border;
    c[ImGuiCol_FrameBg]         = ImVec4(0.18f, 0.17f, 0.17f, 1.0f);
    c[ImGuiCol_FrameBgHovered]  = ImVec4(0.24f, 0.22f, 0.22f, 1.0f);
    c[ImGuiCol_FrameBgActive]   = ImVec4(0.30f, 0.28f, 0.28f, 1.0f);
    c[ImGuiCol_TitleBg]         = col_sidebar;
    c[ImGuiCol_TitleBgActive]   = col_sidebar;
    c[ImGuiCol_Button]          = ImVec4(0.20f, 0.19f, 0.19f, 1.0f);
    c[ImGuiCol_ButtonHovered]   = ImVec4(0.28f, 0.26f, 0.26f, 1.0f);
    c[ImGuiCol_ButtonActive]    = col_accent;
    c[ImGuiCol_SliderGrab]      = col_accent;
    c[ImGuiCol_SliderGrabActive]= col_accent_hi;
    c[ImGuiCol_CheckMark]       = col_accent;
    c[ImGuiCol_Header]          = ImVec4(0.20f, 0.19f, 0.19f, 1.0f);
    c[ImGuiCol_HeaderHovered]   = ImVec4(0.26f, 0.24f, 0.24f, 1.0f);
    c[ImGuiCol_HeaderActive]    = col_accent;
    c[ImGuiCol_Text]            = col_text;
    c[ImGuiCol_TextDisabled]    = col_text_dim;
    c[ImGuiCol_Separator]       = col_border;
    c[ImGuiCol_ScrollbarBg]     = col_panel;
    c[ImGuiCol_ScrollbarGrab]   = ImVec4(0.30f, 0.28f, 0.28f, 1.0f);
    c[ImGuiCol_PopupBg]         = ImVec4(0.12f, 0.11f, 0.11f, 0.96f);
}

void menu::Toggle() {
    state.show = !state.show;
}

// ================================================================
//  Main Render
// ================================================================
void menu::Render() {
    if (!state.show) return;

    static bool config_loaded = false;
    if (!config_loaded) {
        LoadConfig(state.config_name);
        config_loaded = true;
    }

    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, state.menu_opacity);

    const float icon_bar_w = 52.0f;

    ImGui::SetNextWindowSize(ImVec2(600, 420), ImGuiCond_FirstUseEver);
    ImGui::PushStyleColor(ImGuiCol_TitleBg, col_sidebar);
    ImGui::PushStyleColor(ImGuiCol_TitleBgActive, col_sidebar);
    ImGui::Begin("GAMBODJAN##main", &state.show,
        ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoScrollbar |
        ImGuiWindowFlags_NoScrollWithMouse);
    ImGui::PopStyleColor(2);
    {
        // --- Icon sidebar ---
        ImGui::PushStyleColor(ImGuiCol_ChildBg, col_sidebar);
        ImGui::BeginChild("##iconbar", ImVec2(icon_bar_w, 0), false);
        {
            ImGui::Spacing(); ImGui::Spacing();

            float bsz = 38.0f;
            auto icon = [&](const char* lbl, Tab tab) {
                ImGui::SetCursorPosX((icon_bar_w - bsz) * 0.5f);
                if (IconButton(lbl, state.current_tab == tab, bsz)) {
                    state.current_tab = tab;
                    state.current_sub = SubTab::Sub1;
                }
                ImGui::Spacing();
            };

            icon("F",  Tab::FOV);
            icon("S",  Tab::Settings);
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();

        // --- Content area ---
        ImGui::SameLine();
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14, 10));
        ImGui::BeginChild("##content", ImVec2(0, 0), false);
        {
            switch (state.current_tab) {
                case Tab::FOV:       RenderFOV();      break;
                case Tab::Settings:  RenderSettings();  break;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();

    ImGui::PopStyleVar(); // Alpha
}
