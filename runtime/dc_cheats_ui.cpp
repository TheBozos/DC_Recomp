#include "dc_cheats.h"
#include "bozo/app/settings_tabs.h"
#include "imgui.h"

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
            ImGui::Separator();
            ImGui::TextWrapped("Cheats start disabled each time you launch the game.");
        });
    }
}
