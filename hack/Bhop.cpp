#include "Bhop.h"
#include "Console.h"
#include "offsets.hpp"
#include "client_dll.hpp"
#include <Windows.h>
#include <Psapi.h>
#include <atomic>
#include <cstdint>

std::atomic<bool> bhopEnabled{ false };

// ---------- 常量 ----------
#define IN_JUMP        (1ULL << 6)
#define FL_ONGROUND    (1 << 0)

// ---------- 基础结构 ----------
struct Vector { float x, y, z; };
struct QAngle { float x, y, z; };

// ---------- CUserCmd 结构（根据常见偏移定义） ----------
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
};

// ---------- 辅助函数 ----------
static uintptr_t GetModuleBase(const wchar_t* name) {
    HMODULE hMod = GetModuleHandleW(name);
    if (!hMod) return 0;
    MODULEINFO info;
    if (!GetModuleInformation(GetCurrentProcess(), hMod, &info, sizeof(info))) return 0;
    return (uintptr_t)hMod;
}

static uintptr_t clientBase = 0;
static uintptr_t GetClientBase() {
    if (!clientBase) clientBase = GetModuleBase(L"client.dll");
    return clientBase;
}

using namespace cs2_dumper::offsets::client_dll;

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
    constexpr std::ptrdiff_t m_hPlayerPawn = 0x914;
    uint32_t pawnHandle = *(uint32_t*)(controller + m_hPlayerPawn);
    if (!pawnHandle) return 0;
    return GetEntityFromHandle(pawnHandle);
}

static uint32_t GetMoveType(uintptr_t pawn) {
    if (!pawn) return 0;
    return *(uint32_t*)(pawn + 0x525);
}

static uint32_t GetFlags(uintptr_t pawn) {
    if (!pawn) return 0;
    return *(uint32_t*)(pawn + 0x3F4);
}

// ---------- 连跳核心逻辑 ----------
void DoBhopCmd(CUserCmd* cmd) {
    if (!bhopEnabled.load()) return;
    if (!cmd) return;

    uintptr_t pawn = GetLocalPlayerPawn();
    if (!pawn) {
        static bool loggedOnce = false;
        if (!loggedOnce) { Log("[Bhop] Pawn null."); loggedOnce = true; }
        return;
    }

    uint32_t moveType = GetMoveType(pawn);
    if (moveType == 2 || moveType == 5) // LADDER / NOCLIP
        return;

    if (!(cmd->nButtons & IN_JUMP) && !(cmd->nValueScroll & IN_JUMP))
        return;

    cmd->nButtons &= ~IN_JUMP;
    cmd->nValueChanged &= ~IN_JUMP;
    cmd->nValueScroll &= ~IN_JUMP;

    uint32_t flags = GetFlags(pawn);
    if (!(flags & FL_ONGROUND))
        return;

    cmd->nButtons |= IN_JUMP;
    cmd->nValueScroll |= IN_JUMP;
}

void DoBhop() {
    // 线程轮询版本（已弃用）
}