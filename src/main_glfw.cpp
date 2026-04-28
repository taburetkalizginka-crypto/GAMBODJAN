// GAMBODJAN — Cross-platform Launcher with ImGui menu (GLFW + OpenGL3)
// Works on Linux and Windows (without DirectX dependency)

#include "core/lib/imgui/imgui.h"
#include "core/lib/imgui/imgui_impl_glfw.h"
#include "core/lib/imgui/imgui_impl_opengl3.h"
#include "gui/menu.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <cstring>

static void glfw_error_callback(int error, const char* description) {
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

int main(int, char**) {
    printf("GAMBODJAN CLAN\n");

    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit())
        return 1;

    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);

    GLFWwindow* window = glfwCreateWindow(900, 620, "GAMBODJAN Launcher", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return 1;
    }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    menu::state.show = true;
    menu::ApplyStyle();

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();

        // Toggle menu with INSERT key
        {
            static bool insert_was_pressed = false;
            bool pressed = glfwGetKey(window, GLFW_KEY_INSERT) == GLFW_PRESS;
            if (pressed && !insert_was_pressed)
                menu::Toggle();
            insert_was_pressed = pressed;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        menu::Render();

        // Show a status bar at the bottom
        {
            ImGui::SetNextWindowPos(ImVec2(0, io.DisplaySize.y - 28));
            ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, 28));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10, 5));
            ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.06f, 0.06f, 0.06f, 0.95f));
            ImGui::Begin("##statusbar", nullptr,
                ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
                ImGuiWindowFlags_NoSavedSettings);
            ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
                "GAMBODJAN v1.0 | INSERT = toggle menu | Cross-platform build");
            ImGui::End();
            ImGui::PopStyleColor();
            ImGui::PopStyleVar();
        }

        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.06f, 0.06f, 0.06f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    if (menu::state.save_on_close) {
        menu::SaveConfig(menu::state.config_name);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
