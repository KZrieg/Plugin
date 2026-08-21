// Bhop.h
#pragma once
#include <cstdint>
#include <atomic>

struct QAngle {
    float pitch, yaw, roll;
};

struct Vector {
    float x, y, z;
};

struct CUserCmd {
    int command_number;
    int tick_count;
    QAngle viewangles;
    Vector aimdirection;
    float forwardmove;
    float sidemove;
    float upmove;
    int buttons;
    uint64_t nButtons;
    uint64_t nValueScroll;
    uint64_t nValueChanged;
    void* pBaseCmd;
    void* pCsgoUserCmdPb;
};

namespace Bhop {
    bool IsEnabled();
    void SetEnabled(bool enabled);
    void ProcessCommand(CUserCmd* cmd);
}