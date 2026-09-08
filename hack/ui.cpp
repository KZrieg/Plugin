// hack/ui.cpp
#include "ui.h"
#include "imgui.h"
#include "Console.h"
#include "Bhop.h"
#include "Autostrafe.h"

void RenderUI(bool* p_open) {
    ImGui::Begin("Plugin", p_open);

    bool bhop = Bhop::IsEnabled();
    if (ImGui::Checkbox("Bhop", &bhop)) {
        Bhop::SetEnabled(bhop);
    }

    bool autostrafe = Autostrafe::IsEnabled();
    if (ImGui::Checkbox("Autostrafe", &autostrafe)) {
        Autostrafe::SetEnabled(autostrafe);
    }

    if (autostrafe) {
        const char* modes[] = { "Normal", "Subtick" };
        int currentMode = (Autostrafe::GetMode() == AutostrafeMode::Normal) ? 0 : 1;
        if (ImGui::Combo("Mode", &currentMode, modes, IM_ARRAYSIZE(modes))) {
            Autostrafe::SetMode(currentMode == 0 ? AutostrafeMode::Normal : AutostrafeMode::Subtick);
        }
    }

    ImGui::End();
    DrawConsole(nullptr);
}