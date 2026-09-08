// hack/Autostrafe.cpp
#include "Autostrafe.h"
#include "Console.h"
#include <cmath>

std::atomic<bool>           autostrafeEnabled{ false };
std::atomic<AutostrafeMode> autostrafeMode{ AutostrafeMode::Normal };

bool Autostrafe::IsEnabled() { return autostrafeEnabled.load(); }
void Autostrafe::SetEnabled(bool e) { autostrafeEnabled.store(e); }
AutostrafeMode Autostrafe::GetMode() { return autostrafeMode.load(); }
void Autostrafe::SetMode(AutostrafeMode m) { autostrafeMode.store(m); }

namespace NormalStrafe {

    constexpr float AIR_ACCEL = 30.0f;
    constexpr float MAX_SIDEMOVE = 450.0f;
    constexpr float MIN_SPEED = 3.0f;
    constexpr float YAW_CORRECTION = 0.92f;

    void run(CUserCmd* cmd) {
        uintptr_t pawn = CmdUtils::GetLocalPlayerPawn();
        if (!pawn) return;
        if (CmdUtils::IsOnGround(pawn)) return;

        Vector vel = CmdUtils::GetVelocity(pawn);
        float speed = sqrtf(vel.x * vel.x + vel.y * vel.y);
        if (speed < MIN_SPEED) return;

        QAngle eye = CmdUtils::GetEyeAngles(pawn);
        float viewYaw = eye.yaw;
        float velYaw = atan2f(vel.y, vel.x) * CmdUtils::RAD2DEG;

        float ideal = asinf(fminf(AIR_ACCEL / speed, 1.0f)) * CmdUtils::RAD2DEG;
        if (ideal > 45.0f) ideal = 45.0f;

        float diff = CmdUtils::AngleDiff(viewYaw, velYaw);
        float sign = (diff > 0.0f) ? 1.0f : -1.0f;

        cmd->sidemove = MAX_SIDEMOVE * sign;
        cmd->forwardmove = 0.0f;

        float targetYaw = velYaw - ideal * sign;
        float delta = CmdUtils::AngleDiff(targetYaw, viewYaw);
        cmd->viewangles.yaw = viewYaw + delta * YAW_CORRECTION;
    }
}

namespace SubtickStrafe {

    constexpr int MAX_SUBTICKS = 16;
    constexpr float MIN_STRAFE_SPEED = 5.0f;
    constexpr float TICK_INTERVAL = 1.0f / 64.0f;
    constexpr float SV_AIRACCELERATE = 30.0f;
    constexpr float SV_MAXSPEED = 260.0f;
    constexpr float SV_AIR_MAX_WISHSPEED = 30.0f;
    constexpr float SURFACE_FRICTION = 1.0f;

    static int s_substepCounter = 0;

    static float ref_ideal_angle(float speed, float dt, float wishspeed,
        float air_accel, float air_max_wishspeed) {
        if (speed < 1.0f) return 15.0f;

        float accel_speed = wishspeed * air_accel * dt;
        float cos_theta;
        if (accel_speed >= air_max_wishspeed) {
            cos_theta = air_max_wishspeed / (2.0f * speed);
        }
        else {
            cos_theta = (air_max_wishspeed - accel_speed) / speed;
        }
        cos_theta = std::clamp(cos_theta, -1.0f, 1.0f);
        return fmaxf(acosf(cos_theta) * CmdUtils::RAD2DEG, 1.0f);
    }

    static float ref_air_strafer(float vel_x, float vel_y, float target_yaw,
        float dt, bool side_switch,
        float wishspeed, float air_accel,
        float air_max_wishspeed) {
        float speed = sqrtf(vel_x * vel_x + vel_y * vel_y);
        float theta = ref_ideal_angle(speed, dt, wishspeed, air_accel, air_max_wishspeed);

        if (speed < 15.0f) return target_yaw;

        float vel_angle = atan2f(vel_y, vel_x) * CmdUtils::RAD2DEG;
        float vel_delta = target_yaw - vel_angle;
        CmdUtils::NormalizeAngle(vel_delta);

        if (fabsf(vel_delta) > 2.0f) {
            float yaw = (vel_delta > 0.0f) ? (vel_angle + theta) : (vel_angle - theta);
            CmdUtils::NormalizeAngle(yaw);
            return yaw;
        }

        float yaw = side_switch ? (vel_angle + theta) : (vel_angle - theta);
        CmdUtils::NormalizeAngle(yaw);
        return yaw;
    }

    static void ref_air_accel_sim(float& vel_x, float& vel_y, float wishdir_yaw,
        float frame_time, float friction,
        float wishspeed, float air_accel,
        float air_max_wishspeed) {
        float yaw_rad = wishdir_yaw * CmdUtils::DEG2RAD;
        float wish_dir_x = cosf(yaw_rad);
        float wish_dir_y = sinf(yaw_rad);

        float capped = fminf(wishspeed, air_max_wishspeed);
        float dot = vel_x * wish_dir_x + vel_y * wish_dir_y;
        float add_speed = capped - dot;
        if (add_speed <= 0.0f) return;

        float accel_speed = wishspeed * air_accel * friction * frame_time;
        float step = fminf(accel_speed, add_speed);
        vel_x += wish_dir_x * step;
        vel_y += wish_dir_y * step;
    }

    static bool apply_yaw_subtick(CBaseUserCmdPB* base, float when, float yaw_delta) {
        CmdUtils::NormalizeAngle(yaw_delta);
        if (fabsf(yaw_delta) <= 0.01f) return false;

        CSubtickMoveStep* step = CmdUtils::AddSubtickStep(base);
        if (!step) return false;

        step->set_when(when);
        step->set_button(0);
        step->set_pressed(false);
        step->set_analog_forward_delta(0.0f);
        step->set_analog_left_delta(0.0f);
        step->set_yaw_delta(yaw_delta);
        step->set_pitch_delta(0.0f);
        return true;
    }

    void run(CUserCmd* cmd) {
        if (CmdUtils::IsButtonPressed(cmd, IN_SPRINT)) return;

        CBaseUserCmdPB* base = CmdUtils::GetCmdBase(cmd);
        if (!base) return;

        uintptr_t pawn = CmdUtils::GetLocalPlayerPawn();
        if (!pawn) return;
        if (CmdUtils::IsOnGround(pawn)) return;

        uint32_t moveType = CmdUtils::GetMoveType(pawn);
        if (moveType == 2 || moveType == 5) return;

        Vector vel = CmdUtils::GetVelocity(pawn);
        float speed_2d = sqrtf(vel.x * vel.x + vel.y * vel.y);
        if (speed_2d < MIN_STRAFE_SPEED) return;

        float command_yaw = cmd->viewangles.yaw;

        float forward = 0.0f, left = 0.0f;
        if (CmdUtils::IsButtonPressed(cmd, IN_FORWARD)) forward = 1.0f;
        else if (CmdUtils::IsButtonPressed(cmd, IN_BACK)) forward = -1.0f;
        if (CmdUtils::IsButtonPressed(cmd, IN_MOVELEFT)) left = -1.0f;
        else if (CmdUtils::IsButtonPressed(cmd, IN_MOVERIGHT)) left = 1.0f;

        if (forward == 0.0f && left == 0.0f) return;

        float base_yaw_offset = atan2f(-left, forward) * CmdUtils::RAD2DEG;
        float target_yaw = command_yaw + base_yaw_offset;
        CmdUtils::NormalizeAngle(target_yaw);

        float start_when = CmdUtils::GetMaxSubtickWhen(base);
        if (start_when >= 0.99f) return;

        float sub_frame = TICK_INTERVAL / MAX_SUBTICKS;
        float when_step = (1.0f - start_when) / (MAX_SUBTICKS + 1);

        float acc_yaw = command_yaw;
        float sim_vx = vel.x;
        float sim_vy = vel.y;
        int injected = 0;

        for (int i = 1; i <= MAX_SUBTICKS; ++i) {
            bool entry_side = ((s_substepCounter + i) % 2) == 0;

            float wishdir_yaw = ref_air_strafer(
                sim_vx, sim_vy, target_yaw, sub_frame, entry_side,
                SV_MAXSPEED, SV_AIRACCELERATE, SV_AIR_MAX_WISHSPEED);

            float target_view_yaw = wishdir_yaw - base_yaw_offset;
            CmdUtils::NormalizeAngle(target_view_yaw);

            float yaw_delta = target_view_yaw - acc_yaw;
            CmdUtils::NormalizeAngle(yaw_delta);

            float when_frac = start_when + i * when_step;

            if (apply_yaw_subtick(base, when_frac, yaw_delta)) {
                acc_yaw = target_view_yaw;
                ++injected;
            }

            ref_air_accel_sim(
                sim_vx, sim_vy, wishdir_yaw, sub_frame, SURFACE_FRICTION,
                SV_MAXSPEED, SV_AIRACCELERATE, SV_AIR_MAX_WISHSPEED);
        }

        if (injected > 0) {
            ++s_substepCounter;
        }
    }
}

void DoAutostrafeCmd(CUserCmd* cmd) {
    if (!autostrafeEnabled.load() || !cmd) return;

    switch (autostrafeMode.load()) {
    case AutostrafeMode::Normal:
        NormalStrafe::run(cmd);
        break;
    case AutostrafeMode::Subtick:
        SubtickStrafe::run(cmd);
        break;
    }
}