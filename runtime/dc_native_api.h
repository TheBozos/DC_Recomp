#pragma once
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <bit>
#include <string>
namespace dc::native
{
    void finishSceneForMenu();
    void setAssetDirectory(const std::string &);
    // Widescreen (16:9 Hor+, the HUD at the edges): taken at the next frame's start. Any thread.
    void setWidescreen(bool on);
}
