# Cheats

Open **F2 → Cheats** to toggle:

- **Infinite health:** keeps party HP at each character's maximum and cancels queued HP loss.
- **Infinite weapon durability:** keeps equipped weapons at full WHP and bypasses wear before breakage or automatic repair-powder consumption.
- **Infinite thirst:** keeps thirst gauges full, cancels queued depletion and resets the dehydration damage timer.

The toggles are independent and start disabled on every launch. Disabling one restores normal depletion from the current resource values. The options are not stored in game saves; restored resource values can be saved normally.

The game owns all player-state writes. The settings window only updates atomic option flags. Native wrappers delegate to the original compiled routines while disabled. Enabled wrappers act at `CUserStatus::AddNowLife`, `SetNextLife`, `AddDrink`, `Step`, and `BattleSubWeaponDmg`, so lethal hits and weapon break logic are handled before their destructive effects rather than repaired at frame end. Status ticks maintain all six party members and follow each character's selected weapon slot. The cheats do not directly edit enemies, base attack stats, experience, item counts or weapon attachments.

`dc_native_cheats` executes the original compiled resource routines with invented player/inventory data. It compares disabled hooks against the original CPU and player-state results, then checks lethal and animated HP changes, pending drains, independent thirst/dehydration behavior, weapon break prevention, character/weapon switching, empty weapon slots and resuming depletion after disabling. Native-window captures verify the three visible checkboxes and enabled states. Retail combat beyond these targeted checks remains part of manual testing.
