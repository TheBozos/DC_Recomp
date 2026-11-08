#pragma once

#include <cstdint>

struct Runtime;
struct R5900Context;
namespace bozo::port { class Registry; }

namespace dc::cheats
{
    enum class Cheat { InfiniteHealth, InfiniteWeaponDurability, InfiniteThirst, InfiniteStamina, FreezeTimeOfDay };
    bool enabled(Cheat cheat);
    void setEnabled(Cheat cheat, bool on);
    int equippedWeaponAttack(); // game-thread snapshot; -1 when no weapon is equipped
    void requestWeaponAttack(int value); // one edit, applied on the game thread
    int absMultiplier();
    void setAbsMultiplier(int value); // 1..16, positive combat rewards only
    int movementPercent();
    void setMovementPercent(int value); // 50..200, horizontal movement before collision
    int gilda();
    void requestGilda(int value); // 0..65535
    int inventorySize();
    int minimumInventorySize();
    void requestInventorySize(int value); // 1..100; never hide or discard existing items
    struct Consumable { int id=-1, quantity=0, maximum=0; };
    Consumable activeConsumable(int slot);
    void requestConsumableQuantity(int slot,int quantity);
    // Authoritative player state only. Called on the game thread at Options
    // entry/exit and gameplay updates, never against inventory preview copies.
    void refreshPlayer(const uint8_t *ram,const R5900Context *ctx);
    void applyPlayerEdits(uint8_t *ram,const R5900Context *ctx);
    void install(Runtime &runtime, bozo::port::Registry &registry);
    void registerMenu();
}
