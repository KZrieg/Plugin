#pragma once

#include <string_view>

namespace Signatures {

    // CreateMove (client.dll)
    // 模式: 48 8D 05 ? ? ? ? 48 89 0D ? ? ? ?
    // 偏移 +28 得到函数入口
    inline constexpr std::string_view CreateMove = "48 8D 05 ? ? ? ? 48 89 0D ? ? ? ?";

    // FrameStageNotify (client.dll)
    inline constexpr std::string_view FrameStageNotify = "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC ? 48 8B F9 33 ED";

    // 可根据需要添加更多
}