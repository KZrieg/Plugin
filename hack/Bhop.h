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
    int command_number;          // 0x00
    int tick_count;              // 0x04
    QAngle viewangles;           // 0x08
    Vector aimdirection;         // 0x14
    float forwardmove;           // 0x20
    float sidemove;              // 0x24
    float upmove;                // 0x28
    int buttons;                 // 0x2C
    uint64_t nButtons;           // 0x30
    uint64_t nValueScroll;       // 0x38
    uint64_t nValueChanged;      // 0x40
    void* pBaseCmd;              // 0x48
    void* pCsgoUserCmdPb;        // 0x58
};

namespace Bhop {
    bool IsEnabled();
    void SetEnabled(bool enabled);
    void ProcessCommand(CUserCmd* cmd);
}