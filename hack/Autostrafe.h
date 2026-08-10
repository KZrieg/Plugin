#pragma once
#include <atomic>

struct CUserCmd;

enum class AutostrafeMode {
    Normal,      // YAW 补偿
    Subtick      // 无 YAW 补偿
};

extern std::atomic<bool> autostrafeEnabled;
extern std::atomic<AutostrafeMode> autostrafeMode;

void DoAutostrafeCmd(CUserCmd* cmd);

// 线程轮询版本（保留，用于兼容）
void DoAutostrafe();