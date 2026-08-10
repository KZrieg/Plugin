#pragma once
#include <atomic>

struct CUserCmd;  // 前向声明

extern std::atomic<bool> bhopEnabled;

// 在 CreateMove 中调用的版本
void DoBhopCmd(CUserCmd* cmd);

// 线程轮询版本（已弃用，保留声明）
void DoBhop();