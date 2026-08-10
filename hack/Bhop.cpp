// Bhop.cpp
#include "Bhop.h"
#include "offsets.hpp"
#include "client_dll.hpp"
#include "proto.hpp"           // 你的 protobuf 封装
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
// 注意：pCsgoUserCmdPb 的偏移需要根据你的游戏版本调整，常见为 0x58
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
    void* pCsgoUserCmdPb;        // 0x58  ← 指向 proto::csgo_usercmd_pb
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

// ---------- Subtick 跳跃注入 ----------
static void AddSubtickJump(proto::csgo_usercmd_pb* pb) {
    if (!pb) return;

    proto::base_usercmd_pb* base = pb->mutable_base();
    if (!base) return;

    auto subtick_moves = base->mutable_subtick_moves();
    if (!subtick_moves) return;

    // 分配一个新的 Subtick 步骤
    proto::subtick_move_step* step = subtick_moves->add();  // 需要你的 repeated_ptr_field 有 add() 方法
    if (!step) return;

    step->set_button(IN_JUMP);
    step->set_pressed(true);
    step->set_when(0.0f);   // 立即在当前 tick 内执行
    step->set_analog_forward_delta(0.0f);
    step->set_analog_left_delta(0.0f);
    step->set_pitch_delta(0.0f);
    step->set_yaw_delta(0.0f);
}

// ---------- 主逻辑 ----------
void DoBhopCmd(CUserCmd* cmd) {
    if (!bhopEnabled.load() || !cmd) return;

    uintptr_t pawn = GetLocalPlayerPawn();
    if (!pawn) return;

    uint32_t moveType = GetMoveType(pawn);
    if (moveType == 2 || moveType == 5) return;  // 梯子 / 穿墙

    // 检测跳跃键是否按住（按钮位可能是 buttons 或 nButtons）
    bool jumpPressed = (cmd->buttons & IN_JUMP) || (cmd->nButtons & IN_JUMP);
    if (!jumpPressed) {
        static bool prevOnGround = false;
        prevOnGround = false;
        return;
    }

    uint32_t flags = GetFlags(pawn);
    bool onGround = (flags & FL_ONGROUND) != 0;

    static bool prevOnGround = false;  // 前一帧地面状态

    // 落地瞬间：从空中变为地面
    if (!prevOnGround && onGround) {
        // 尝试注入 Subtick 跳跃
        proto::csgo_usercmd_pb* pb = static_cast<proto::csgo_usercmd_pb*>(cmd->pCsgoUserCmdPb);
        if (pb) {
            AddSubtickJump(pb);
            // 清除普通跳跃位，避免干扰
            cmd->buttons &= ~IN_JUMP;
            cmd->nButtons &= ~IN_JUMP;
        }
        else {
            // 如果 pb 为空（偏移错误或未启用），回退到普通跳跃
            cmd->buttons |= IN_JUMP;
            cmd->nButtons |= IN_JUMP;
        }
    }
    else {
        // 在空中或已在地面，清除跳跃位（防止持续按住）
        cmd->buttons &= ~IN_JUMP;
        cmd->nButtons &= ~IN_JUMP;
    }

    prevOnGround = onGround;
}