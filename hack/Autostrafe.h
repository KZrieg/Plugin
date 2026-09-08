// hack/Autostrafe.h
#pragma once
#include <atomic>
#include "cmd_utils.h"

enum class AutostrafeMode {
    Normal,
    Subtick
};

namespace Autostrafe {
    bool IsEnabled();
    void SetEnabled(bool enabled);
    AutostrafeMode GetMode();
    void SetMode(AutostrafeMode mode);
}

void DoAutostrafeCmd(CUserCmd* cmd);