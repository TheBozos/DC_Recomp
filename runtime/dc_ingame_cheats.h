#pragma once
#include "bozo/app/video_settings.h"
struct Runtime;
namespace bozo::port { class Registry; }
namespace dc::cheats {
    // The original Options lifecycle stays in charge of loading, fading and
    // returning to gameplay. Native pages share the existing cheat controls.
    void installIngameMenu(Runtime &, bozo::port::Registry &, void (*prepareScene)() = nullptr);
    bool takeIngameQuitRequest(); // consumed at the application's frame boundary
    // The windowed build, before the game thread starts: the window's settings for the Graphics page (without them,
    // as headless, the page says they are unavailable).
    void setGraphicsAccess(bozo::app::video::Shared (*shared)(), void (*post)(const bozo::app::video::Settings &, bool));
}
