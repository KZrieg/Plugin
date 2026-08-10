#include "ui.h"
#include "imgui.h"
#include "Console.h"
#include "Bhop.h"
#include "Autostrafe.h"

void RenderUI(bool* p_open)
{
    ImGui::Begin("Plugin", p_open);

    // Bhop
    bool bhop = bhopEnabled.load();
    if (ImGui::Checkbox("Bhop", &bhop)) {
        bhopEnabled.store(bhop);
    }
    ImGui::SameLine();
    ImGui::TextColored(bhop ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
        bhop ? "ON" : "OFF");

    // Autostrafe
    bool autostrafe = autostrafeEnabled.load();
    if (ImGui::Checkbox("Autostrafe", &autostrafe)) {
        autostrafeEnabled.store(autostrafe);
    }
    ImGui::SameLine();
    ImGui::TextColored(autostrafe ? ImVec4(0, 1, 0, 1) : ImVec4(1, 0, 0, 1),
        autostrafe ? "ON" : "OFF");

    // 模式选择（仅在 Autostrafe 启用时显示，也可始终显示）
    if (autostrafe) {
        ImGui::Indent();
        const char* modes[] = { "Normal", "Subtick" };
        static int currentMode = 0; // 默认 Normal
        // 加载当前模式
        currentMode = (autostrafeMode.load() == AutostrafeMode::Normal) ? 0 : 1;
        if (ImGui::Combo("Mode", &currentMode, modes, IM_ARRAYSIZE(modes))) {
            autostrafeMode.store(currentMode == 0 ? AutostrafeMode::Normal : AutostrafeMode::Subtick);
        }
        ImGui::Unindent();
    }

    ImGui::End();

    // 控制台窗口
    DrawConsole(nullptr);
}