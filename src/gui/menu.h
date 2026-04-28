#pragma once

#include "../core/lib/imgui/imgui.h"
#define IMGUI_DEFINE_MATH_OPERATORS
#include "../core/lib/imgui/imgui_internal.h"

namespace menu {

    enum class Tab {
        FOV,
        Settings
    };

    enum class SubTab {
        Sub1,
        Sub2
    };

    struct MenuState {
        bool   show = false;
        Tab    current_tab = Tab::FOV;
        SubTab current_sub = SubTab::Sub1;

        // === Camera Distance (Dota 2 FOV) ===
        bool fov_enabled = false;
        float fov_value = 1200.f;
        float fov_min = 1000.f;
        float fov_max = 5000.f;

        // === Camera & World ===
        bool mouse_distance = true;
        int  camera_step = 50;
        bool fog_enabled = false;

        // === ESP ===
        bool esp_health = false;
        bool esp_mana = false;
        bool esp_abilities = false;

        // === Auto Accept ===
        bool auto_accept = true;
        bool accept_last_moment = false;
        int  auto_accept_delay = 0;

        // === Settings ===
        int  menu_key = 0x2D; // VK_INSERT
        float menu_opacity = 0.95f;
        bool save_on_close = true;
        char config_name[64] = "default";
        int  theme_index = 0;

        // === Runtime state ===
        bool game_found = false;
        char status_text[256] = "Not connected";
    };

    inline MenuState state;

    void Toggle();
    void Render();
    void ApplyStyle();
    void SaveConfig(const char* filename);
    void LoadConfig(const char* filename);

}
