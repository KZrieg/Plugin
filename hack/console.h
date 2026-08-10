#pragma once
#include "imgui.h"

void InitConsole();
void Log(const char* fmt, ...);
void DrawConsole(bool* p_open = nullptr);