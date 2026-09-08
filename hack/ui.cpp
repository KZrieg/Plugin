#include "ui.h"
#include "vacexploit.h"
#include "imgui.h"

void RenderUI()
{
    ImGui::SetNextWindowPos(ImVec2(100, 100), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(200, 80), ImGuiCond_FirstUseEver);

    ImGui::Begin("Plugin", nullptr, ImGuiWindowFlags_NoResize);

    bool enabled = g_vac_exploit_enabled;
    if (ImGui::Checkbox("VAC Exploit", &enabled)) {
        g_vac_exploit_enabled = enabled;
        if (!enabled) {
            g_vac_burst_remaining = 0;
        }
    }

    ImGui::End();
}