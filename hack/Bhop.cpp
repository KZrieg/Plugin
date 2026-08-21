// Bhop.cpp
#include "Bhop.h"
#include "usercmd.pb.h"
#include "Console.h"
#include "offsets.hpp"
#include "client_dll.hpp"
#include <atomic>
#include <Windows.h>

using namespace cs2_dumper::offsets::client_dll;
using namespace cs2_dumper::schemas::client_dll;

namespace {
    std::atomic<bool> g_enabled{ true };
    constexpr uint64_t IN_JUMP = 1ULL << 6;
    constexpr std::ptrdiff_t ENTITY_LIST_ENTRY_SIZE = 0x10;

    uintptr_t GetClientBase() {
        static uintptr_t base = 0;
        if (!base) base = (uintptr_t)GetModuleHandleW(L"client.dll");
        return base;
    }

    template<typename T>
    bool SafeRead(uintptr_t addr, T& out) {
        if (!addr) return false;
        __try { out = *reinterpret_cast<T*>(addr); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    // ===== 通过控制器获取 Pawn（这个方法之前成功过） =====
    uintptr_t GetLocalPlayerPawn() {
        uintptr_t client = GetClientBase();
        if (!client) return 0;

        uintptr_t controller = 0;
        if (!SafeRead(client + dwLocalPlayerController, controller)) return 0;
        if (!controller) return 0;

        uint32_t handle = 0;
        if (!SafeRead(controller + CBasePlayerController::m_hPawn, handle)) return 0;
        if (!handle || handle == 0xFFFFFFFF) return 0;

        uintptr_t entityList = 0;
        if (!SafeRead(client + dwEntityList, entityList)) return 0;
        if (!entityList) return 0;

        int index = handle & 0x7FFF;
        uintptr_t pawn = 0;
        if (!SafeRead(entityList + index * ENTITY_LIST_ENTRY_SIZE, pawn)) return 0;
        return pawn;
    }

    uint32_t GetMoveType(uintptr_t pawn) {
        if (!pawn) return 0;
        uint8_t v = 0;
        SafeRead(pawn + C_BaseEntity::m_nActualMoveType, v);
        return v;
    }

    uint32_t GetFlags(uintptr_t pawn) {
        if (!pawn) return 0;
        uint32_t v = 0;
        SafeRead(pawn + C_BaseEntity::m_fFlags, v);
        return v;
    }

    // ===== 从 Protobuf 读取按钮（CUserCmdBasePB，不是CSGOUserCmdPB） =====
    bool IsJumpPressed(CUserCmd* cmd) {
        if (!cmd || !cmd->pCsgoUserCmdPb) return false;
        __try {
            CUserCmdBasePB* pb = static_cast<CUserCmdBasePB*>(cmd->pCsgoUserCmdPb);
            CBaseUserCmdPB* base = pb->mutable_base();
            if (!base) return false;
            CInButtonStatePB* btn = base->mutable_buttons_pb();
            if (!btn) return false;
            return (btn->buttonstate1() & IN_JUMP) != 0;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            return false;
        }
    }

    void ClearJumpBit(CUserCmd* cmd) {
        if (!cmd || !cmd->pCsgoUserCmdPb) return;
        __try {
            CUserCmdBasePB* pb = static_cast<CUserCmdBasePB*>(cmd->pCsgoUserCmdPb);
            CBaseUserCmdPB* base = pb->mutable_base();
            if (!base) return;
            CInButtonStatePB* btn = base->mutable_buttons_pb();
            if (!btn) return;
            btn->set_buttonstate1(btn->buttonstate1() & ~IN_JUMP);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }

    void AddSubtickJump(CUserCmd* cmd) {
        if (!cmd || !cmd->pCsgoUserCmdPb) return;
        __try {
            CUserCmdBasePB* pb = static_cast<CUserCmdBasePB*>(cmd->pCsgoUserCmdPb);
            CBaseUserCmdPB* base = pb->mutable_base();
            if (!base) return;
            CSubtickMoveStep* step = base->add_subtick_moves();
            if (!step) return;
            step->set_button(IN_JUMP);
            step->set_pressed(true);
            step->set_when(0.0f);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
}

bool Bhop::IsEnabled() { return g_enabled.load(); }
void Bhop::SetEnabled(bool enabled) { g_enabled.store(enabled); }

void Bhop::ProcessCommand(CUserCmd* cmd) {
    __try {
        if (!g_enabled.load() || !cmd) return;

        uintptr_t pawn = GetLocalPlayerPawn();
        if (!pawn) return;

        if (!IsJumpPressed(cmd)) {
            static bool prevOnGround = false;
            prevOnGround = false;
            return;
        }

        uint32_t moveType = GetMoveType(pawn);
        if (moveType == 2 || moveType == 5) return;

        uint32_t flags = GetFlags(pawn);
        bool onGround = (flags & 1) != 0;
        static bool prevOnGround = false;

        if (!prevOnGround && onGround) {
            AddSubtickJump(cmd);
            ClearJumpBit(cmd);
        }

        prevOnGround = onGround;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {}
}