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
| Level up equipped weapon | Fills the equipped weapon's ABS to the game's own threshold; choose **Level up** in the weapon menu as usual. Refused for default weapons, the Serpent Sword and level 99 |
| Reveal floor map | Marks every floor cell of the current dungeon floor explored, on arrival and when switched on; unopened chests, Atla and trap circles show too |
| Fish always bite | A fish near the bait never leaves it, and a hooked fish stays hooked until you reel in; fish kinds, sizes and points are unchanged |
| Spawn item / Spawn count / Give item | Gives up to 10 of a bag item, attachment or weapon through the game's own give-item routine, while there is room. Event, key and story items, default and story weapons are not offered |
| Fast-forward speed / always on | 150–400% (default 300%). Hold **`** (the key below Esc) or a gamepad's **touchpad** / **Share** button, or lock it on. Sound is muted while fast |
| Damage multiplier | 1–16× damage you deal to monsters, capped at the monster's remaining HP; immunities stay |
| One-hit kills | Any hit that deals damage defeats the monster; immunities stay |
| No damage taken | Monster attacks never land: no damage, knock-down, status or theft. Poison already active, traps and thirst still apply |
| Key finder | The monster carrying the current floor's key blinks as a gold square on the dungeon map |

On the in-game list, Up/Down selects and scrolls through all 28 rows. Cross toggles on/off rows;
Left/Right sets them off/on. ATK, quantities and capacity use Left/Right ±1, L1/R1 ±10, L2/R2 ±100.
Gilda uses ±1/100/1000; movement uses ±5%. Select the active slot (1–3, left to right on the HUD)
before changing its quantity. Empty slots cannot create items. The row hint describes each control.

Select the corresponding **Apply** row for ATK, Gilda, quantity or inventory size. Edits are queued
for the game thread and applied when Options closes or gameplay resumes. Increasing inventory size
may allow a larger quantity afterward; close and reopen Options to refresh the displayed limit.
Quantity requests retain the expected item ID, so changing the selected item before application
cannot silently edit a different item. Resizing never deletes or moves items.

Toggles start off, ABS and damage start at 1×, movement at 100% and fast-forward at 300% on each launch. These settings are not
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

### Level-up, map, fishing, spawner, fast-forward, combat and key finder

Hook points, all in the main program (the DUN overlay is reached through the main routines it calls, with
return-address checks and the identity-matched DUN module):

- **Level-up**: a one-shot edit at the player's status tick. ABS (`WEAPON_HAVE+0x14`) is set to the compiled
  `GetWeaponMaxExp` (0x232080) result; the weapon menu's own flow then applies stat gains, attachment absorption,
  synthsphere counting, `level+1` and the ABS reset. A kill at full ABS never levels a weapon in the original either.
- **Map**: `FlushCheckMask` (0x1C3C00, after the floor build clears it) and `checkMask` (0x1C39C0, walking updates)
  write the 20×20 explored grid (`CDungeonMap+0x8710`) and room flags for cells that have floor, on the two DUN map
  objects only (0x1DC4BE0, 0x1DD56F0); column 19, which no floor reaches, is left alone. The grid is not saved.
- **Fishing**: `rand_check` (0x240050) gets probability 0 for the LeaveFood/LeaveHook rolls only, still drawing its
  `rand()`; `CFish::Step` (0x240480) holds a hooked fish's timer (state 8, `+0x58`) at 1. DC1 has no line break.
- **Spawner**: the compiled `CheckItemGet` (0x1BDD40), `CheckWeaponRot` (0x1BDF90) and `GetItem` (0x1BE060) run on a
  copy of the hook's context with a sentinel return. Names come from the resident system messages (`SystemMes`,
  key = number + 100; a weapon's number from `ComItemInfo`, indexed id − 0x51). The allow-list excludes the 53
  event-flagged bag items (every floor gate key among them), unused IDs, the Synthsphere and default/story weapons.
- **Fast-forward**: the frame pacer's cap is raised (the game's clock advances per frame); audio is not queued while
  fast. `PS2X_FRAME_CAP=0` runs are left unlimited.
- **Damage dealt**: an instruction hook at 0x1DC3F4 (`subu v0,v0,s4` in `CMonstorUnit::CheckDmg`, a native site)
  scales the final damage before the original subtraction. Every player attack reaches monster HP there.
- **Damage taken**: `CheckHitUser` (0x1B5920) answers "no hit" (-1) to DUN `BtCheckDamageProc` (return 0x1DBB0E0).
- **Key finder**: after `CMonstorUnit::DrawMapSymbol` (0x1D7AA0, from DUN `MainDraw`), the live monster holding a
  gate key (`unit+i*400+0x1E470`; the floor's own key, `NowDngMap+0x464`, first) is located through its own virtual
  `GetPosition` and marked with the port's native `set2DSpriteC4` rectangle at the map's transform. No packet is built.

`dc_native_cheat_extras` checks each hook's decisions against recording stubs. A scripted dungeon run checked the
page, map reveal, the key marker, level-up (Dagger ABS 27 → 32), spawning (two Fire), fast-forward (60 → 180 frames
per second at 300%) and full HP with no damage taken. `DC_PROBE_CHEATS=1` logs applied level-ups and spawns.
Fishing and the damage multiplier still need play-testing in a fishing spot and in combat.
