// hack/hook.h
#pragma once

using CreateMoveFn = bool(__fastcall*)(void*, int, void*);

bool InstallCreateMoveHook();
void UninstallCreateMoveHook();