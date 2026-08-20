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
// Autostrafe.h
namespace Autostrafe {
    bool IsEnabled();
    void SetEnabled(bool enabled);
    AutostrafeMode GetMode();
    void SetMode(AutostrafeMode mode);
}

void DoAutostrafeCmd(CUserCmd* cmd);
void DoAutostrafe();