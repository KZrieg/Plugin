// hack/Bhop.cpp
#include "Bhop.h"
#include "Console.h"
#include <atomic>

namespace {
    std::atomic<bool> g_enabled{ true };
    bool g_prevOnGround = false;
    int g_consecutiveJumps = 0;
    int g_missedJumps = 0;
    constexpr float LANDING_JUMP_WHEN = 0.05f;
}

bool Bhop::IsEnabled() {
    return g_enabled.load();
}

void Bhop::SetEnabled(bool enabled) {
    g_enabled.store(enabled);
    if (!enabled) {
        g_prevOnGround = false;
        g_consecutiveJumps = 0;
    }
}

void Bhop::ProcessCommand(CUserCmd* cmd) {
    if (!g_enabled.load() || !cmd) return;

    uintptr_t pawn = CmdUtils::GetLocalPlayerPawn();
    if (!pawn) return;

    if (!CmdUtils::IsButtonPressed(cmd, IN_JUMP)) {
        g_prevOnGround = false;
        g_consecutiveJumps = 0;
        return;
    }

    uint32_t moveType = CmdUtils::GetMoveType(pawn);
    if (moveType == 2 || moveType == 5) return;

    bool onGround = CmdUtils::IsOnGround(pawn);

    if (!g_prevOnGround && onGround) {
        CBaseUserCmdPB* base = CmdUtils::GetCmdBase(cmd);
        if (!base) {
            g_missedJumps++;
            g_prevOnGround = onGround;
            return;
        }

        CmdUtils::ClearSubtickButton(base, IN_JUMP);
        CmdUtils::ClearButton(cmd, IN_JUMP);

        CSubtickMoveStep* step = CmdUtils::AddSubtickStep(base);
        if (step) {
            step->set_button(IN_JUMP);
            step->set_pressed(true);
            step->set_when(LANDING_JUMP_WHEN);
            step->set_analog_forward_delta(0.0f);
            step->set_analog_left_delta(0.0f);
            step->set_yaw_delta(0.0f);
            step->set_pitch_delta(0.0f);
            g_consecutiveJumps++;
        }
        else {
            g_missedJumps++;
        }
    }

    g_prevOnGround = onGround;
}