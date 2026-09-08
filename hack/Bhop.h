// hack/Bhop.h
#pragma once
#include "cmd_utils.h"

namespace Bhop {
    bool IsEnabled();
    void SetEnabled(bool enabled);
    void ProcessCommand(CUserCmd* cmd);
}