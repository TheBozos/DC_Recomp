#pragma once
struct Runtime;
namespace bozo::port { class Registry; }
namespace dc::cheats {
    // The original Options lifecycle stays in charge of loading, fading and
    // returning to gameplay. Native pages share the existing cheat controls.
    void installIngameMenu(Runtime &, bozo::port::Registry &, void (*prepareScene)() = nullptr);
    bool takeIngameQuitRequest(); // consumed at the application's frame boundary
}
