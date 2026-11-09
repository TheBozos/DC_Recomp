#include "dc_cheats.h"
#include "bozo/app/settings_tabs.h"
#include "imgui.h"
#include <algorithm>

namespace dc::cheats
{
    void registerMenu()
    {
        bozo::app::ui::registerTab("Cheats", [] {
            auto toggle = [](const char *label, Cheat cheat, const char *description) {
                bool on = enabled(cheat);
                if (ImGui::Checkbox(label, &on)) setEnabled(cheat, on);
                ImGui::TextDisabled("%s", description);
                ImGui::Spacing();
            };
            toggle("Infinite health", Cheat::InfiniteHealth, "Keeps your party's HP full.");
            toggle("Infinite weapon durability", Cheat::InfiniteWeaponDurability, "Keeps equipped weapons at full WHP.");
            toggle("Infinite thirst", Cheat::InfiniteThirst, "Keeps your party's thirst gauges full.");
            toggle("Infinite stamina", Cheat::InfiniteStamina, "Keeps the bottom-left attack/combo gauge full.");
            ImGui::Separator();
            const int current = equippedWeaponAttack();
            if (current >= 0) ImGui::Text("Equipped weapon ATK: %d", current);
            else ImGui::TextDisabled("No equipped weapon available.");
            static int attack = 100;
            ImGui::SetNextItemWidth(220);
            ImGui::InputInt("Weapon ATK (0-999)", &attack);
            attack = std::clamp(attack,0,999);
            ImGui::BeginDisabled(current < 0);
            if (ImGui::Button("Apply to equipped weapon")) requestWeaponAttack(attack);
            ImGui::EndDisabled();
            ImGui::TextWrapped("Applies when gameplay resumes. The edited ATK is included when you save your game.");
            ImGui::Separator();
            int abs=absMultiplier();if(ImGui::SliderInt("Weapon ABS multiplier",&abs,1,16,"%dx"))setAbsMultiplier(abs);
            int damage=damageMultiplier();if(ImGui::SliderInt("Damage multiplier",&damage,1,16,"%dx"))setDamageMultiplier(damage);
            toggle("One-hit kills", Cheat::OneHitKills, "Any hit that deals damage defeats the monster; immunities still apply.");
            toggle("No damage taken", Cheat::NoDamageTaken, "Monster attacks never land: no damage, knock-down or theft.");
            toggle("Key finder", Cheat::KeyFinder, "The monster carrying the floor's key blinks gold on the map.");
            int speed=movementPercent();if(ImGui::SliderInt("Movement speed",&speed,50,200,"%d%%"))setMovementPercent(speed);
            toggle("Freeze time of day",Cheat::FreezeTimeOfDay,"Holds sky and lighting; game timers continue.");
            static int gold=1000;
            ImGui::Text("Current Gilda: %d",gilda());ImGui::InputInt("Gilda (0-65535)",&gold,100,1000);gold=std::clamp(gold,0,65535);
            ImGui::BeginDisabled(gilda()<0);if(ImGui::Button("Apply Gilda"))requestGilda(gold);ImGui::EndDisabled();
            static int slot=0,quantity=1;
            ImGui::SliderInt("Active consumable slot",&slot,0,2,"%d (0=left)");
            const auto item=activeConsumable(slot);
            if(item.id>=0)ImGui::Text("Item ID %d: %d held, maximum %d",item.id,item.quantity,item.maximum);
            else ImGui::TextDisabled("Equip a consumable in this HUD slot first.");
            ImGui::InputInt("Quantity",&quantity);quantity=std::clamp(quantity,1,std::max(1,item.maximum));
            ImGui::BeginDisabled(item.id<0 || item.maximum<1);if(ImGui::Button("Apply quantity"))requestConsumableQuantity(slot,quantity);ImGui::EndDisabled();
            static int capacity=50;
            ImGui::Text("Inventory: %d slots; minimum occupied size: %d",inventorySize(),minimumInventorySize());
            ImGui::InputInt("Inventory size (1-100)",&capacity);capacity=std::clamp(capacity,std::min(100,minimumInventorySize()),100);
            ImGui::BeginDisabled(inventorySize()<0);if(ImGui::Button("Apply inventory size"))requestInventorySize(capacity);ImGui::EndDisabled();
            ImGui::TextWrapped("Inventory edits apply when gameplay resumes and are included in game saves. Existing items are never discarded by resizing.");
            ImGui::Separator();
            ImGui::BeginDisabled(current < 0);
            if (ImGui::Button("Level up equipped weapon")) requestWeaponLevelUp();
            ImGui::EndDisabled();
            ImGui::TextDisabled("Fills its ABS; choose Level up in the weapon menu. Not for default weapons.");
            toggle("Reveal floor map", Cheat::RevealFloorMap, "Shows the whole dungeon floor on the map, chests and Atla included.");
            toggle("Fish always bite", Cheat::FishingHelper, "Fish near the bait take it and stay hooked until you reel in.");
            static int spawn = 0, spawnCount = 1;
            const int items = spawnItemCount();
            if (items)
            {
                spawn = std::clamp(spawn, 0, items - 1);
                if (ImGui::BeginCombo("Spawn item", spawnItem(spawn).name))
                {
                    for (int i = 0; i < items; ++i)
                        if (ImGui::Selectable(spawnItem(i).name, i == spawn)) spawn = i;
                    ImGui::EndCombo();
                }
                ImGui::SliderInt("Spawn count", &spawnCount, 1, 10);
                if (ImGui::Button("Give item")) requestSpawn(spawnItem(spawn).id, spawnCount);
                ImGui::TextDisabled("Given when gameplay resumes, while there is room. Story and key items are excluded.");
            }
            else ImGui::TextDisabled("Item names appear once the game has loaded its messages.");
            ImGui::Separator();
            int fast = fastForwardPercent();
            if (ImGui::SliderInt("Fast-forward speed", &fast, 150, 400, "%d%%")) setFastForwardPercent(fast / 50 * 50);
            toggle("Fast-forward always on", Cheat::FastForwardLock, "Or hold ` (below Esc) or the gamepad's touchpad. Sound is muted.");
            ImGui::Separator();
            ImGui::TextWrapped("Cheats reset each launch: toggles off, ABS 1x, movement 100%, fast-forward 300%.");
        });
    }
}
