// hook.h
#pragma once

// 初始化所有钩子（在独立线程中调用）
void InitializeHooks();

// 菜单显示状态（外部可访问）
extern bool g_showMenu;