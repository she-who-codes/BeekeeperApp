
#pragma once

#include "imgui.h"

namespace Fonts
{
    extern ImFont* Small;
    extern ImFont* Body;
    extern ImFont* Heading;
    extern ImFont* Bold;

    bool Load(float dpiScale = 1.0f);
}
