// Bhop.h
#pragma once
#include <atomic>

struct CUserCmd;  // 前向声明

extern std::atomic<bool> bhopEnabled;

void DoBhopCmd(CUserCmd* cmd);