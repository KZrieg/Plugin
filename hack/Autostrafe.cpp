#include "Autostrafe.h"
#include "Console.h"
#include "offsets.hpp"
#include "client_dll.hpp"
#include <Windows.h>
#include <Psapi.h>
#include <atomic>
#include <cmath>

std::atomic<bool> autostrafeEnabled{ false };
std::atomic<AutostrafeMode> autostrafeMode{ AutostrafeMode::Normal };

// ---------- 基础结构 ----------
struct Vector { float x, y, z; };
struct QAngle { float x, y, z; };

// ---------- CUserCmd 结构（与 Bhop 保持一致） ----------
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

static bool IsOnGround(uintptr_t pawn) {
    if (!pawn) return false;
    return (*(uint32_t*)(pawn + 0x3F4) & 1) != 0;
}

static Vector GetVelocity(uintptr_t pawn) {
    if (!pawn) return { 0.0f, 0.0f, 0.0f };
    return *(Vector*)(pawn + 0x3F8);
}

static QAngle GetEyeAngles(uintptr_t pawn) {
    if (!pawn) return { 0.0f, 0.0f, 0.0f };
    constexpr std::ptrdiff_t EYE_ANGLES_OFFSET = 0x3340;
    return *(QAngle*)(pawn + EYE_ANGLES_OFFSET);
}

// ---------- 自动转向核心逻辑 ----------
void DoAutostrafeCmd(CUserCmd* cmd) {
    if (!autostrafeEnabled.load()) return;
    if (!cmd) return;

    uintptr_t pawn = GetLocalPlayerPawn();
    if (!pawn) {
        static bool logged = false;
        if (!logged) { Log("[Autostrafe] Pawn null."); logged = true; }
        return;
    }

    if (IsOnGround(pawn)) return;

    Vector vel = GetVelocity(pawn);
    float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
    if (speed < 3.0f) return;

    QAngle eye = GetEyeAngles(pawn);
    float yaw = eye.y;
    float velYaw = atan2f(vel.y, vel.x) * 57.2957795f;

    const float airAccel = 30.0f;
    float ideal = asinf(fminf(airAccel / speed, 1.0f)) * 57.2957795f;
    if (ideal > 45.0f) ideal = 45.0f;

    float diff = fmodf(yaw - velYaw + 540.0f, 360.0f) - 180.0f;
    float sign = (diff > 0.0f) ? 1.0f : -1.0f;

    float sideMove = 450.0f * sign;
    float forwardMove = 0.0f;

    cmd->sidemove = sideMove;
    cmd->forwardmove = forwardMove;

    if (autostrafeMode.load() == AutostrafeMode::Normal) {
        float targetYaw = velYaw - ideal * sign;
        float delta = fmodf(targetYaw - yaw + 540.0f, 360.0f) - 180.0f;
        const float correction = 0.92f;
        cmd->viewangles.y = yaw + delta * correction;
    }
}

void DoAutostrafe() {
    // 线程轮询版本（已弃用）
}