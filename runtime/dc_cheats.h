#pragma once

#include <cstdint>

struct Runtime;
struct R5900Context;
namespace bozo::port { class Registry; }

namespace dc::cheats
{
    enum class Cheat { InfiniteHealth, InfiniteWeaponDurability, InfiniteThirst, InfiniteStamina, FreezeTimeOfDay, FastForwardLock, FishingHelper, RevealFloorMap, OneHitKills, NoDamageTaken, KeyFinder };
    bool enabled(Cheat cheat);
    void setEnabled(Cheat cheat, bool on);
    // Fast-forward: while held (the window's ` key or a gamepad's touchpad / Share button) or locked on, the frame
    // cap is raised to this percentage of the normal rate and the sound is muted. Game logic runs per frame.
    int fastForwardPercent();
    void setFastForwardPercent(int value); // 150..400
    void setFastForwardHeld(bool held);    // window input, sampled once a frame
    int gameSpeedPercent();                // 100, or fastForwardPercent() while held or locked
    int equippedWeaponAttack(); // game-thread snapshot; -1 when no weapon is equipped
    void requestWeaponAttack(int value); // one edit, applied on the game thread
    int absMultiplier();
    void setAbsMultiplier(int value); // 1..16, positive combat rewards only
    int damageMultiplier();
    void setDamageMultiplier(int value); // 1..16, damage the party deals to monsters
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
    // Weapon level-up: fills the equipped weapon's ABS to the game's own threshold (GetWeaponMaxExp); the player then
    // levels it up in the weapon menu as usual. Refused for default weapons, the Serpent Sword and level 99.
    void requestWeaponLevelUp();
    // Item spawner: the bag items, attachments and weapons the game's GetItem may give without touching story
    // progress (event/key items, default and story weapons, unused IDs excluded), with their names read from the
    // game's resident system messages. Empty until the game has loaded them.
    struct SpawnItem { int id = 0; char name[32] = {}; };
    int spawnItemCount();
    SpawnItem spawnItem(int index);
    void requestSpawn(int id, int count); // count 1..10; stops at the first refusal (bag/attachment/weapon full)
    // Authoritative player state only. Called on the game thread at Options
    // entry/exit and gameplay updates, never against inventory preview copies.
    void refreshPlayer(const uint8_t *ram,const R5900Context *ctx);
    void applyPlayerEdits(uint8_t *ram,const R5900Context *ctx);
    void install(Runtime &runtime, bozo::port::Registry &registry);
    void registerMenu();
}
