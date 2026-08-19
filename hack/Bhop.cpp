// Bhop.cpp
#include "Bhop.h"
#include "offsets.hpp"
#include "client_dll.hpp"         
#include "Console.h"
#include <Windows.h>
#include <Psapi.h>
#include <atomic>
#include <cstdint>

using namespace cs2_dumper::offsets::client_dll;
using namespace cs2_dumper::schemas::client_dll;

// ---------- 基础结构体 ----------
struct Vector {
    float x, y, z;
};

struct QAngle {
    float x, y, z;
};

// ---------- CUserCmd 结构 ----------
// 注意：已移除 pCsgoUserCmdPb 字段
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
    // void* pCsgoUserCmdPb;     // 已移除
};

// ---------- 常量 ----------
static constexpr uint32_t IN_JUMP = 1ULL << 6;
static constexpr uint32_t FL_ONGROUND = 1 << 0;

std::atomic<bool> bhopEnabled{ false };

// ---------- 辅助函数 ----------
static uintptr_t GetClientBase() {
    static uintptr_t base = 0;
    if (!base) {
        HMODULE mod = GetModuleHandleA("client.dll");
        if (mod) {
            MODULEINFO info;
            GetModuleInformation(GetCurrentProcess(), mod, &info, sizeof(info));
            base = (uintptr_t)mod;
        }
    }
    return base;
}

static uintptr_t GetEntityFromHandle(uint32_t handle) {
    if (!handle) return 0;
    uintptr_t base = GetClientBase();
    if (!base) return 0;
    uintptr_t entityList = base + dwEntityList;
    uint32_t index = handle & 0x7FFF;
    return *(uintptr_t*)(entityList + index * 0x10);
}

static uintptr_t GetLocalPlayerPawn() {
    uintptr_t base = GetClientBase();
    if (!base) return 0;

    uintptr_t direct = *(uintptr_t*)(base + dwLocalPlayerPawn);
    if (direct) return direct;

    uintptr_t controller = *(uintptr_t*)(base + dwLocalPlayerController);
    if (!controller) return 0;

    uint32_t pawnHandle = *(uint32_t*)(controller + CCSPlayerController::m_hPlayerPawn);
    return GetEntityFromHandle(pawnHandle);
}

static uint32_t GetFlags(uintptr_t pawn) {
    if (!pawn) return 0;
    return *(uint32_t*)(pawn + C_BaseEntity::m_fFlags);
}

static uint32_t GetMoveType(uintptr_t pawn) {
    if (!pawn) return 0;
    return *(uint32_t*)(pawn + C_BaseEntity::m_MoveType);
}

// ---------- 主逻辑（普通跳跃，无 Protobuf） ----------
void DoBhopCmd(CUserCmd* cmd) {
    if (!bhopEnabled.load() || !cmd) return;

    uintptr_t pawn = GetLocalPlayerPawn();
    if (!pawn) return;

    uint32_t moveType = GetMoveType(pawn);
    if (moveType == 2 || moveType == 5) return;  // 梯子 / 穿墙

    // 检测跳跃键是否按住
    bool jumpPressed = (cmd->buttons & IN_JUMP) || (cmd->nButtons & IN_JUMP);
    if (!jumpPressed) {
        // 如果未按住跳跃，重置地面状态
        static bool prevOnGround = false;
        prevOnGround = false;
        return;
    }

    uint32_t flags = GetFlags(pawn);
    bool onGround = (flags & FL_ONGROUND) != 0;

    static bool prevOnGround = false;

    // 落地瞬间：从空中变为地面 → 执行跳跃
    if (!prevOnGround && onGround) {
        cmd->buttons |= IN_JUMP;
        cmd->nButtons |= IN_JUMP;
    }
    else {
        // 在空中或已在地面，清除跳跃位（防止持续按住）
        cmd->buttons &= ~IN_JUMP;
        cmd->nButtons &= ~IN_JUMP;
    }

    prevOnGround = onGround;
}