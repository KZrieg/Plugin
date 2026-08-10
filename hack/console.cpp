// Console.cpp
#include "Console.h"
#include <Windows.h>
#include <vector>
#include <string>
#include <mutex>
#include <cstdarg>
#include <cstdio>

static std::vector<std::string> g_logs;
static std::mutex g_mutex;

void InitConsole() {
    static bool done = false;
    if (done) return;
    done = true;

    AllocConsole();
    SetConsoleTitleW(L"Console");
    SetConsoleOutputCP(CP_UTF8);
    FILE* f;
    freopen_s(&f, "CONOUT$", "w", stdout);
    freopen_s(&f, "CONOUT$", "w", stderr);
}

void Log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[2048];
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    {
        std::lock_guard<std::mutex> lock(g_mutex);
        g_logs.push_back(buf);
    }
    printf("%s\n", buf);
}

void DrawConsole(bool* p_open) {
    if (!ImGui::Begin("Console", p_open)) {
        ImGui::End();
        return;
    }

    ImGui::BeginChild("log", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        for (const auto& line : g_logs)
            ImGui::TextUnformatted(line.c_str());
    }
    ImGui::EndChild();
    ImGui::End();
}