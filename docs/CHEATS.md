# Cheats and in-game options

Open the game's main menu, choose **Options**, then **Cheats**. The entry page also offers
**Game Options** (the original settings) and **Quit Game**. Circle goes back; Circle on the entry page
closes Options through the original fade/exit lifecycle. Quit Game defaults to Cancel and requires
an explicit confirmation. It uses normal application shutdown and does not autosave.

The same cheat controls remain available in **F2 → Cheats**. Both interfaces share session settings.

| Control | Effect and limits |
|---|---|
| Infinite health | Keeps all party HP full and cancels queued loss |
| Weapon durability | Refills equipped WHP; prevents wear, breakage and auto-repair powder consumption |
| Infinite thirst | Refills party thirst, cancels queued depletion and resets dehydration damage timers |
| Combo stamina | Holds the bottom-left dungeon attack/combo gauge at 100 and clears exhaustion; does not grant Stamina Drink/Dran's Feather effects |
| Equipped weapon ATK | One edit to the active character's equipped weapon, 0–999 |
| Weapon ABS multiplier | 1–16× positive combat ABS rewards, after original bonus modifiers; retains the normal maximum and full-ABS notification |
| Gilda | One edit, 0–65,535, matching the game's unsigned money field |
| Consumable quantity | One edit to an existing active HUD slot, 1–99, further limited by available bag capacity |
| Movement speed | 50–200% horizontal player displacement before collision; leaves gravity unchanged |
| Freeze time of day | Holds one shared time input for outdoor sky and lighting; game/story timers and menu fades continue |
| Inventory size | One edit, 1–100 slots; refuses shrinkage that would hide an occupied entry or exceed capacity with active quantities |

On the in-game list, Up/Down selects and scrolls through all 16 rows. Cross toggles on/off rows;
Left/Right sets them off/on. ATK, quantities and capacity use Left/Right ±1, L1/R1 ±10, L2/R2 ±100.
Gilda uses ±1/100/1000; movement uses ±5%. Select the active slot (1–3, left to right on the HUD)
before changing its quantity. Empty slots cannot create items. The row hint describes each control.

Select the corresponding **Apply** row for ATK, Gilda, quantity or inventory size. Edits are queued
for the game thread and applied when Options closes or gameplay resumes. Increasing inventory size
may allow a larger quantity afterward; close and reopen Options to refresh the displayed limit.
Quantity requests retain the expected item ID, so changing the selected item before application
cannot silently edit a different item. Resizing never deletes or moves items.

Toggles start off, ABS starts at 1× and movement starts at 100% on each launch. These settings are not
stored in saves. Restored resources and one-time inventory edits **are** included if the game saves
afterward. Disabling a resource cheat restores normal depletion from its current value.

## Implementation and bounded validation

Only the game thread writes player state. The native UI publishes atomic settings and queues edits.
Original resource boundaries handle health/thirst/WHP before death or breakage checks. Stamina requires
the DUN call site and an identity-matched dungeon module. Player snapshots resolve the save's embedded
status in town as well as the dungeon, and avoid weapon-menu preview copies.

The ABS hook is an inline native boundary in the monster's positive reward branch, after bonus
modifiers and after the reverse-ABS branch. It scales the reward before the original addition/cap;
it does not lower the level-up threshold or multiply reverse-ABS penalties. Movement changes only
identified player MoveCheck callers in town and the active DUN module; collision still runs.
The time hold wraps sky and lighting inputs rather than stopping a global clock.

The controller page owns a converted copy of the common game font, with transparent glyph gutters,
and draws native UI geometry. It extends no guest menu arrays and allocates no guest text buffers.
The original Options load/fade/exit handlers and original settings remain available. Only the outer
Options help draw is suppressed on custom pages to avoid overlapping their footer.

`dc_native_cheats` checks disabled CPU/player-state equivalence against real compiled resource
routines, death/break prevention, preview isolation, six-character weapon selection, one-shot edit
bounds and unchanged neighboring bytes. New cases execute the compiled monster reward tail with
normal/multiplied/capped ABS and a reverse-ABS penalty; check bag occupancy, safe resizing and stale
item requests; and observe the inputs reaching collision and both lighting renderers.
`dc_native_ingame_cheats` exercises controller navigation, scrolling, every cheat, original Options
transitions and the quit confirmation's default/cancel/one-shot behaviour. Native-window captures
check layout and original settings access. Full retail combat and all movement/lighting situations
remain part of manual gameplay testing.
