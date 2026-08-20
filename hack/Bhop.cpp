// Bhop.cpp
#include "Bhop.h"
#include "usercmd.pb.h"
#include "Console.h"
#include "offsets.hpp"
#include "client_dll.hpp"
#include <atomic>
#include <Windows.h>

#define BHOP_LOG(fmt, ...) Log("[Bhop] " fmt, ##__VA_ARGS__)

using namespace cs2_dumper::offsets::client_dll;
using namespace cs2_dumper::schemas::client_dll;

namespace {
    std::atomic<bool> g_enabled{ true };
    constexpr uint64_t IN_JUMP = 1ULL << 6;

    uintptr_t GetClientBase() {
        static uintptr_t base = 0;
        if (!base) {
            base = (uintptr_t)GetModuleHandleW(L"client.dll");
            BHOP_LOG("client.dll base: %p", base);
        }
        return base;
    }

    template<typename T>
    T ReadMemory(uintptr_t address) {
        if (!address) return T{};
        return *reinterpret_cast<T*>(address);
    }

    // 从命令中获取 Pawn（优先）
    uintptr_t GetPawnFromCommand(CUserCmd* cmd) {
        if (!cmd || !cmd->pCsgoUserCmdPb) return 0;
        CUserCmdBasePB* pb = static_cast<CUserCmdBasePB*>(cmd->pCsgoUserCmdPb);
        CBaseUserCmdPB* base = pb->mutable_base();
        if (!base || !base->has_pawn_entity_handle()) return 0;

        uint32_t handle = base->pawn_entity_handle();
        if (handle == 0x00FFFFFF) return 0;

        uintptr_t entityList = ReadMemory<uintptr_t>(GetClientBase() + dwEntityList);
        if (!entityList) return 0;
        int index = handle & 0x7FFF;
        return ReadMemory<uintptr_t>(entityList + index * 0x10);
    }

    // 备选：直接读取本地 Pawn
    uintptr_t GetLocalPlayerPawn_Direct() {
        return ReadMemory<uintptr_t>(GetClientBase() + dwLocalPlayerPawn);
    }

    // ===== 关键修改：使用 m_nActualMoveType（偏移 0x526）而不是 m_MoveType（0x525） =====
    uint32_t GetMoveType(uintptr_t pawn) {
        if (!pawn) return 0;
        // m_nActualMoveType 在 C_BaseEntity 中的偏移为 0x526
        return ReadMemory<uint8_t>(pawn + C_BaseEntity::m_nActualMoveType);
    }

    // 获取地面标志
    uint32_t GetFlags(uintptr_t pawn) {
        if (!pawn) return 0;
        return ReadMemory<uint32_t>(pawn + C_BaseEntity::m_fFlags);
    }

    // Subtick 跳跃注入（落地瞬间）
    void AddSubtickJump(CUserCmdBasePB* pb) {
        if (!pb) return;
        CBaseUserCmdPB* base = pb->mutable_base();
        if (!base) return;
        CSubtickMoveStep* step = base->add_subtick_moves();
        if (!step) return;
        step->set_button(IN_JUMP);
        step->set_pressed(true);
        step->set_when(0.0f);
        BHOP_LOG("Subtick step added");
    }
}

bool Bhop::IsEnabled() { return g_enabled.load(); }
void Bhop::SetEnabled(bool enabled) { g_enabled.store(enabled); BHOP_LOG("SetEnabled: %s", enabled ? "ON" : "OFF"); }

void Bhop::ProcessCommand(CUserCmd* cmd) {
    if (!IsEnabled() || !cmd) return;

    // 1. 获取本地 Pawn
    uintptr_t pawn = GetPawnFromCommand(cmd);
    if (!pawn) {
        pawn = GetLocalPlayerPawn_Direct();
        if (!pawn) {
            BHOP_LOG("No pawn available");
            return;
        }
    }

    // 2. 检查移动类型（现在使用正确的偏移）
    uint32_t moveType = GetMoveType(pawn);
    uint32_t flags = GetFlags(pawn);
    bool jumpPressed = (cmd->buttons & IN_JUMP) || (cmd->nButtons & IN_JUMP);

    BHOP_LOG("Pawn: %p | MoveType: %u | Flags: 0x%X | Jump: %d", pawn, moveType, flags, jumpPressed);

    // 3. 过滤梯子和穿墙
    if (moveType == 2 || moveType == 5) {
        BHOP_LOG("Skipping (ladder/noclip)");
        return;
    }

    // 4. 如果未按下跳跃，重置地面状态
    if (!jumpPressed) {
        static bool prevOnGround = false;
        prevOnGround = false;
        return;
    }

    // 5. 地面检测
    bool onGround = (flags & 1) != 0;
    static bool prevOnGround = false;

    // 6. 落地瞬间逻辑
    if (!prevOnGround && onGround) {
        BHOP_LOG("Landing detected!");
        CUserCmdBasePB* pb = static_cast<CUserCmdBasePB*>(cmd->pCsgoUserCmdPb);
        if (pb) {
            AddSubtickJump(pb);
            cmd->buttons &= ~IN_JUMP;
            cmd->nButtons &= ~IN_JUMP;
            BHOP_LOG("Subtick injected");
        }
        else {
            BHOP_LOG("pb null, fallback to legacy");
            cmd->buttons |= IN_JUMP;
            cmd->nButtons |= IN_JUMP;
        }
    }
    else {
        // 非落地瞬间：清除跳跃位（防止按住连续跳）
        cmd->buttons &= ~IN_JUMP;
        cmd->nButtons &= ~IN_JUMP;
    }

    prevOnGround = onGround;
}