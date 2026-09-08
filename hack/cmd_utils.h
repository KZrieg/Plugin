// hack/cmd_utils.h
#pragma once
#include <Windows.h>
#include <cmath>
#include <cstdint>
#include <algorithm>
#include "offsets.hpp"
#include "client_dll.hpp"
#include "usercmd.pb.h"

using namespace cs2_dumper::offsets::client_dll;
using namespace cs2_dumper::schemas::client_dll;

struct Vector { float x, y, z; };

struct QAngle {
    float pitch;
    float yaw;
    float roll;
};

struct CUserCmd {
    int         command_number;
    int         tick_count;
    QAngle      viewangles;
    Vector      aimdirection;
    float       forwardmove;
    float       sidemove;
    float       upmove;
    int         buttons;
    uint64_t    nButtons;
    uint64_t    nValueScroll;
    uint64_t    nValueChanged;
    void* pBaseCmd;
    void* pCsgoUserCmdPb;
};

constexpr uint64_t IN_JUMP = 1ULL << 6;
constexpr uint64_t IN_DUCK = 1ULL << 7;
constexpr uint64_t IN_FORWARD = 1ULL << 3;
constexpr uint64_t IN_BACK = 1ULL << 4;
constexpr uint64_t IN_MOVELEFT = 1ULL << 8;
constexpr uint64_t IN_MOVERIGHT = 1ULL << 9;
constexpr uint64_t IN_SPRINT = 1ULL << 10;

namespace CmdUtils {

    template<typename T>
    inline bool SafeRead(uintptr_t addr, T& out) {
        if (!addr) return false;
        __try { out = *reinterpret_cast<T*>(addr); return true; }
        __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
    }

    inline uintptr_t GetClientBase() {
        static uintptr_t base = 0;
        if (!base) base = (uintptr_t)GetModuleHandleW(L"client.dll");
        return base;
    }

    inline uintptr_t GetLocalPlayerPawn() {
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
        if (!SafeRead(entityList + index * 0x10, pawn)) return 0;
        return pawn;
    }

    inline bool IsOnGround(uintptr_t pawn) {
        if (!pawn) return false;
        uint32_t flags = 0;
        SafeRead(pawn + C_BaseEntity::m_fFlags, flags);
        return (flags & 1) != 0;
    }

    inline uint32_t GetMoveType(uintptr_t pawn) {
        if (!pawn) return 0;
        uint8_t v = 0;
        SafeRead(pawn + C_BaseEntity::m_nActualMoveType, v);
        return v;
    }

    inline Vector GetVelocity(uintptr_t pawn) {
        if (!pawn) return { 0, 0, 0 };
        Vector v{};
        SafeRead(pawn + C_BaseEntity::m_vecVelocity, v);
        return v;
    }

    inline QAngle GetEyeAngles(uintptr_t pawn) {
        if (!pawn) return { 0, 0, 0 };
        QAngle v{};
        SafeRead(pawn + C_CSPlayerPawn::m_angEyeAngles, v);
        return v;
    }

    inline CBaseUserCmdPB* GetCmdBase(CUserCmd* cmd) {
        if (!cmd || !cmd->pCsgoUserCmdPb) return nullptr;
        __try {
            auto* pb = static_cast<CUserCmdBasePB*>(cmd->pCsgoUserCmdPb);
            return pb->mutable_base();
        }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    inline CInButtonStatePB* GetButtonsPB(CBaseUserCmdPB* base) {
        if (!base) return nullptr;
        __try { return base->mutable_buttons_pb(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    inline bool IsButtonPressed(CUserCmd* cmd, uint64_t button) {
        CBaseUserCmdPB* base = GetCmdBase(cmd);
        if (!base) return false;
        CInButtonStatePB* btn = GetButtonsPB(base);
        if (!btn) return false;
        return (btn->buttonstate1() & button) != 0;
    }

    inline void ClearButton(CUserCmd* cmd, uint64_t button) {
        CBaseUserCmdPB* base = GetCmdBase(cmd);
        if (!base) return;
        CInButtonStatePB* btn = GetButtonsPB(base);
        if (!btn) return;
        btn->set_buttonstate1(btn->buttonstate1() & ~button);
    }

    inline float GetMaxSubtickWhen(CBaseUserCmdPB* base) {
        if (!base) return 0.0f;
        float max_when = 0.0f;
        for (int i = 0; i < base->subtick_moves_size(); ++i) {
            auto* step = base->mutable_subtick_moves(i);
            if (step) max_when = std::fmaxf(max_when, step->when());
        }
        return max_when;
    }

    inline void ClearSubtickButton(CBaseUserCmdPB* base, uint64_t button) {
        if (!base) return;
        auto* field = base->mutable_subtick_moves();
        for (int i = field->size() - 1; i >= 0; --i) {
            if (field->Get(i).button() == button) {
                field->DeleteSubrange(i, 1);
            }
        }
    }

    inline CSubtickMoveStep* AddSubtickStep(CBaseUserCmdPB* base) {
        if (!base) return nullptr;
        __try { return base->add_subtick_moves(); }
        __except (EXCEPTION_EXECUTE_HANDLER) { return nullptr; }
    }

    constexpr float RAD2DEG = 57.29577951308232f;
    constexpr float DEG2RAD = 0.0174532925199433f;

    inline void NormalizeAngle(float& angle) {
        angle = std::fmodf(angle + 180.0f, 360.0f);
        if (angle < 0.0f) angle += 360.0f;
        angle -= 180.0f;
    }

    inline float AngleDiff(float a, float b) {
        return std::fmodf(a - b + 540.0f, 360.0f) - 180.0f;
    }

}