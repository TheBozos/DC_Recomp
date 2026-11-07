#pragma once

#include <cstdint>

struct Runtime;
namespace bozo::port { class Registry; }

namespace dc::cheats
{
    enum class Cheat { InfiniteHealth, InfiniteWeaponDurability, InfiniteThirst };
    bool enabled(Cheat cheat);
    void setEnabled(Cheat cheat, bool on);
    void install(Runtime &runtime, bozo::port::Registry &registry);
    void registerMenu();
}
