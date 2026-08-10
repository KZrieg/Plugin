// Autostrafe.h
#pragma once
#include <atomic>

struct CUserCmd;

enum class AutostrafeMode {
    Normal,
    Subtick
};

extern std::atomic<bool> autostrafeEnabled;
extern std::atomic<AutostrafeMode> autostrafeMode;

void DoAutostrafeCmd(CUserCmd* cmd);
void DoAutostrafe();