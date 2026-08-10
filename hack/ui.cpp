// ui.cpp
#include "ui.h"
#include "imgui.h"
#include "Console.h"
#include "Bhop.h"
#include "Autostrafe.h"

void RenderUI(bool* p_open) {
    ImGui::Begin("Plugin", p_open);

    bool bhop = bhopEnabled.load();
    if (ImGui::Checkbox("Bhop", &bhop)) {
        bhopEnabled.store(bhop);
    }
    ImGui::SameLine();
    ImGui::TextColored(bhop ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
        bhop ? "ON" : "OFF");

    bool autostrafe = autostrafeEnabled.load();
    if (ImGui::Checkbox("Autostrafe", &autostrafe)) {
        autostrafeEnabled.store(autostrafe);
    }
    ImGui::SameLine();
    ImGui::TextColored(autostrafe ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
        autostrafe ? "ON" : "OFF");

    if (autostrafe) {
        ImGui::Indent();
        const char* modes[] = { "Normal", "Subtick" };
        static int currentMode = 0;
        currentMode = (autostrafeMode.load() == AutostrafeMode::Normal) ? 0 : 1;
        if (ImGui::Combo("Mode", &currentMode, modes, IM_ARRAYSIZE(modes))) {
            autostrafeMode.store(currentMode == 0 ? AutostrafeMode::Normal : AutostrafeMode::Subtick);
        }
        ImGui::Unindent();
    }

    ImGui::End();

    DrawConsole(nullptr);
}