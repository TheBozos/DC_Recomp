// Travel and progress cheats (dc_warp.h). Everything the game thread does here goes through the game's own state
// and routines, at the point main() reads the pad (CGamePad::UpDate, 0x12AD70: after the frame's town or dungeon
// loop, before the next), and only while the game is idle:
//  - a town's free walk: main mode 2 (EditLoop), its GameMode 1, no fade, no exit pending;
//  - a dungeon's free play: main mode 3 (GameLoop), BtGameModeFlag 1, gameTask 0, no event, no fade, not the back
//    floor, no forced floor.
// Map numbers: < 200 a town or area (gedit/e01.. map 0.., s01.. map 11..), 200 + d dungeon d. MapJump(map, -1)
// (0x141D30) names the next map; the town leaves through EdExitLoop (0x178110) after an EdFadeOut, the dungeon
// through its own exit task (gameTask 0xAF: a fade, then MapJump to its town and existFlag 2; the next map is then
// replaced by the warp's). A dungeon is entered at a floor through the entrance menu's own preset (InitDunEnterMenu,
// 0x226110, a2 >= 0: it picks that floor by itself); another floor of the same dungeon is built by the stairs' path
// (floor - 1 as the floor, system event 100 as the next event, gameTask 0x190). See build notes for the evidence.
#include "dc_warp.h"
#include "dc_cheats.h"
#include "dc_pad.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <initializer_list>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace dc::warp
{
    namespace
    {
        // ---- the game's state ----
        constexpr uint32_t kSaveData = 0x01CD4140u;            // CSaveData
        constexpr uint32_t kDngStatusInSave = kSaveData + 0x540Cu; // CDngStatusData (also in town)
        constexpr uint32_t kMainMode = 0x002A2534u;            // main()'s mode: 2 town (EditLoop), 3 dungeon (GameLoop)
        constexpr uint32_t kMapNo = 0x002A2518u, kNextMapNo = 0x002A1E90u;
        constexpr uint32_t kTownGameMode = 0x002A1F50u;        // EditLoop's GameMode: 1 free walk, 0xC in an interior
        constexpr uint32_t kInnerGameMode = 0x002A2A84u;       // EditInLoop's GameMode (interiors): 0 free walk, 1 talk, 4 event
        constexpr uint32_t kEventResult = 0x01D3D618u;         // EdEventInfo+0x448: an event's result, 8 a map jump
        constexpr uint32_t kNowEditMap = 0x002A2810u;          // the loaded town (0..5)
        constexpr uint32_t kFadeInOut = 0x002A29F8u, kFadeEnd = 0x002A29FCu, kExitLoop = 0x002A287Cu;
        constexpr uint32_t kUserStatus = 0x002A3468u;          // DUN: CDngStatusData *
        constexpr uint32_t kBtGameMode = 0x002A355Cu, kGameTask = 0x002A35ECu, kExistFlag = 0x002A35F0u;
        constexpr uint32_t kBtEventMode = 0x002A3608u, kBtUra = 0x002A34B4u, kBtMapJumpFloor = 0x002A2BE4u;
        constexpr uint32_t kBtActBusy = 0x01DC4490u, kLockOn = 0x002A3588u;
        constexpr uint32_t kEventNext = 0x01D5676Cu, kEventReload = 0x01D56774u; // BtEventInfo +0x2C, +0x34
        constexpr uint32_t kMonsterUnit = 0x002A34D0u;         // CMonstorUnit * (16 slots)
        constexpr uint32_t kPlayer = 0x01EA1D20u, kCamera = 0x01DC45E0u;
        constexpr uint32_t kDunResident = 0x01DB7450u;
        constexpr uint32_t kGameClear = 0x002A2510u;
        constexpr uint32_t kEditPartsLive = 0x01D19BF0u;       // CEditPartsInfo of the loaded town
        constexpr uint32_t kAtlaRows = 0x0025DF30u;            // AtraAppearData: 6 pointers to {id, floor, count} rows
        constexpr uint32_t kEditPartsData = 0x002540D0u;       // [6 towns][24 parts] x 0xBC
        // ---- the game's routines ----
        constexpr uint32_t kPadUpdate = 0x0012AD70u, kEnterMenu = 0x00226110u, kEnterMenuReturn = 0x01DB4CA0u;
        constexpr uint32_t kMapJump = 0x00141D30u, kExitLoopFn = 0x00178110u, kFadeOut = 0x00189790u;
        constexpr uint32_t kSetPosition = 0x00138FB0u, kSetFollow = 0x00124AE0u, kCameraStep = 0x001247D0u;
        constexpr uint32_t kSetGameFlag = 0x00158280u, kSetIntFlag = 0x00158360u, kVisitMap = 0x00158800u;
        constexpr uint32_t kQuestDungeon = 0x00158870u, kPartsGet = 0x00158660u, kChipGet = 0x00158760u;
        constexpr uint32_t kPartsLoad = 0x0019A3F0u, kEdGetItem = 0x001730A0u, kEquipDefault = 0x0020BFB0u;
        constexpr uint32_t kReturnSentinel = 0x00080000u; // not code: a routine's jr ra lands here

        const Place kPlaces[] = {
            {0, "Norune Village"}, {1, "Matataki Village"}, {2, "Queens"}, {3, "Muska Lacka"}, {4, "Sun Giant georama"},
            {11, "Area s01"}, {13, "Area s03"}, {14, "Brownboo Village"}, {19, "Area s09"}, {21, "Area s11"}, {22, "Area s12"},
            {23, "Yellow Drops"}, {24, "Area s14"}, {25, "Area s15"}, {26, "Area s16"}, {27, "Area s17"}, {33, "Area s23"},
            {34, "Area s24"}, {35, "Area s25"}, {36, "Area s26"}, {37, "Area s27"}, {38, "Area s28"}, {39, "Area s29"},
            {40, "Dark Heaven Castle"}, {41, "Area s31"}, {42, "Sun & Moon Temple approach"}, {43, "Area s33"},
            {44, "Area s34"}, {45, "Area s35"}, {46, "Area s36"}, {47, "Area s37"}, {48, "Area s38"}, {49, "Area s39"},
            {50, "Area s40"}, {51, "Area s41, the ending"}, {60, "Demon Shaft entrance"}, {61, "Area s51"}, {87, "Area s77"},
            {88, "Area s78"}, {89, "Area s79"}, {96, "Area s86"}, {97, "Area s87"}, {98, "Area s88"}, {100, "Area s90"},
            {101, "Area s91"}, {102, "Area s92"}, {103, "Area s93"}, {104, "Area s94"}, {105, "Area s95"}, {106, "Area s96"},
            {107, "Area s97"}, {108, "Area s98"}, {109, "Area s99"}};
        const Dungeon kDungeons[] = {{"Divine Beast Cave", 15}, {"Wise Owl Forest", 17}, {"Shipwreck", 18},
                                     {"Sun & Moon Temple", 18}, {"Moon Sea", 15}, {"Gallery of Time", 25},
                                     {"Demon Shaft", 100}};
        const char *const kChapters[] = {"", "1: Norune (the start)", "2: Matataki, Wise Owl Forest", "3: Queens, Shipwreck",
                                         "4: Muska Lacka, Sun & Moon Temple", "5: Yellow Drops, Moon Sea",
                                         "6: Dark Heaven Castle, Gallery of Time", "7: After the Dark Genie (post-game)"};

        // ---- requests ----
        enum class Kind { Place, Dungeon, Monster, Georama, World, Chapter, Cheat, Flag, IntFlag, Where, Talk, Wait, Peek };
        struct Command
        {
            Kind kind;
            int a = 0, b = 0;
            std::string text;
        };
        std::mutex g_mutex;
        std::deque<Command> g_queue;
        std::string g_status = "Ready";
        std::atomic<bool> g_busy{false};
        struct Scheduled
        {
            uint64_t frame;
            Command command;
        };
        std::vector<Scheduled> g_script; // sorted by frame
        size_t g_scriptNext = 0;

        std::atomic<uint64_t> g_frame{0}; // the host's frame (onFrame), for the log
        void say(const std::string &text)
        {
            std::fprintf(stderr, "[dc:warp] frame %llu: %s\n", static_cast<unsigned long long>(g_frame.load()), text.c_str());
            std::lock_guard<std::mutex> lock(g_mutex);
            g_status = text;
        }
        std::deque<Command> g_immediate; // where / cheat: carried out at once, ahead of a warp under way
        void push(Command command)
        {
            std::lock_guard<std::mutex> lock(g_mutex);
            if (command.kind == Kind::Peek || command.kind == Kind::Cheat || command.kind == Kind::Talk)
                g_immediate.push_back(std::move(command));
            else
            {
                g_queue.push_back(std::move(command));
                g_busy = true;
            }
        }

        // ---- guest access ----
        template <typename T> T rd(const uint8_t *ram, uint32_t address)
        {
            T value;
            std::memcpy(&value, ram + (address & 0x1FFFFFFu), sizeof(T));
            return value;
        }
        template <typename T> void wr(uint8_t *ram, uint32_t address, T value)
        {
            std::memcpy(ram + (address & 0x1FFFFFFu), &value, sizeof(T));
        }
        // A compiled game routine on a copy of the hook's context: its frame below the current stack, its return to a
        // sentinel; false when it did not come back there (never for the routines called here).
        bool call(Runtime *rt, uint8_t *ram, const R5900Context *ctx, uint32_t address, std::initializer_list<uint32_t> ints,
                  std::initializer_list<float> floats = {}, int32_t *result = nullptr)
        {
            RecompiledFunction routine = rt->functions.generated(address);
            if (!routine)
                return false;
            R5900Context c = *ctx;
            int reg = 4;
            for (const uint32_t a : ints)
                c.r[reg++].ud[0] = uint64_t(int64_t(int32_t(a)));
            int f = 12;
            for (const float v : floats)
                c.f[f++] = v;
            c.r[31].ud[0] = kReturnSentinel;
            c.leaving = false;
            routine(ram, &c, rt);
            if (c.pc != kReturnSentinel || c.leaving)
                return false;
            if (result)
                *result = static_cast<int32_t>(c.r[2].uw[0]);
            return true;
        }

        int mainMode(const uint8_t *ram) { return rd<int32_t>(ram, kMainMode); }
        bool townIdle(const uint8_t *ram)
        {
            return mainMode(ram) == 2 && rd<int32_t>(ram, kTownGameMode) == 1 && rd<int32_t>(ram, kFadeInOut) == 0 &&
                   rd<int32_t>(ram, kExitLoop) == 0;
        }
        // An interior's free walk (EditLoop runs EditInLoop: its GameMode 0xC, the interior's own GameMode 0).
        bool interiorIdle(const uint8_t *ram)
        {
            return mainMode(ram) == 2 && rd<int32_t>(ram, kTownGameMode) == 0xC && rd<int32_t>(ram, kInnerGameMode) == 0 &&
                   rd<int32_t>(ram, kFadeInOut) == 0;
        }
        uint32_t userStatus(const uint8_t *ram) { return rd<uint32_t>(ram, kUserStatus) & 0x1FFFFFFu; }
        bool dungeonIdle(const uint8_t *ram, Runtime *rt)
        {
            const uint32_t s = userStatus(ram);
            return mainMode(ram) == 3 && rt->functions.generated(kDunResident) && s != 0u && s < 0x2000000u &&
                   rd<int32_t>(ram, kBtGameMode) == 1 && rd<int32_t>(ram, kGameTask) == 0 && rd<int32_t>(ram, kBtEventMode) == 0 &&
                   rd<int32_t>(ram, kExistFlag) == 0 && rd<int32_t>(ram, kFadeInOut) == 0 && rd<int32_t>(ram, kBtUra) == 0 &&
                   rd<int32_t>(ram, kBtMapJumpFloor) == -1 && rd<int32_t>(ram, kBtActBusy) == 0;
        }
        int currentDungeon(const uint8_t *ram) { return rd<int8_t>(ram, userStatus(ram)); }
        int currentFloor(const uint8_t *ram) { return rd<int8_t>(ram, userStatus(ram) + 2u); }

        std::string where(const uint8_t *ram)
        {
            char line[256];
            const int mode = mainMode(ram);
            const int d = mode == 3 ? currentDungeon(ram) : -1, f = mode == 3 ? currentFloor(ram) : -1;
            std::snprintf(line, sizeof line, "where: mode %d map %d next %d town-mode %d/%d dungeon %d floor %d chapter %d party %d",
                          mode, rd<int32_t>(ram, kMapNo), rd<int32_t>(ram, kNextMapNo), rd<int32_t>(ram, kTownGameMode),
                          rd<int32_t>(ram, kInnerGameMode), d, f,
                          rd<int32_t>(ram, kSaveData + 0x10668u), rd<int8_t>(ram, kDngStatusInSave + 5u));
            std::string out = line;
            if (mode == 3)
            {
                std::snprintf(line, sizeof line, " | bt-mode %d task 0x%x event %d exist %d fade %d ura %d jump-floor %d act %d",
                              rd<int32_t>(ram, kBtGameMode), rd<int32_t>(ram, kGameTask), rd<int32_t>(ram, kBtEventMode),
                              rd<int32_t>(ram, kExistFlag), rd<int32_t>(ram, kFadeInOut), rd<int32_t>(ram, kBtUra),
                              rd<int32_t>(ram, kBtMapJumpFloor), rd<int32_t>(ram, kBtActBusy));
                out += line;
            }
            out += " deepest";
            for (int i = 0; i < 7; ++i)
                out += " " + std::to_string(rd<int8_t>(ram, kDngStatusInSave + 0x42BFu + static_cast<uint32_t>(i)));
            return out;
        }

        // ---- the executor ----
        bool g_active = false;
        Command g_current;
        int g_phase = 0;
        uint32_t g_waited = 0;
        int g_presetFloor = -1;      // the entrance menu's preset for the warp under way
        bool g_interiorExit = false; // EditInLoop's next call answers 99 (an interior left for the map named)
        int g_nextMonster = 0;
        // talk 1 (scripts): Cross tapped every 16 frames while a town talk or event (GameMode 2 / 0xE) or a dungeon
        // event (gameTask 0x191) waits, so a scripted run reads through the story; never in free walk or a menu.
        bool g_talk = false;
        uint32_t g_talkClock = 0;
        bool talking(const uint8_t *ram)
        {
            const int mode = mainMode(ram);
            if (mode == 2)
            {
                const int town = rd<int32_t>(ram, kTownGameMode), inner = rd<int32_t>(ram, kInnerGameMode);
                return town == 2 || town == 0xE || (town == 0xC && (inner == 1 || inner == 4));
            }
            return mode == 3 && rd<int32_t>(ram, kGameTask) == 0x191 && rd<int32_t>(ram, kBtGameMode) == 1;
        }

        void finish(const std::string &text)
        {
            say(text);
            g_active = false;
            std::lock_guard<std::mutex> lock(g_mutex);
            g_busy = !g_queue.empty();
        }

        // A warp's target, a town (map < 200) or a dungeon (200 + d, floor b).
        bool targetIsDungeon() { return g_current.kind == Kind::Dungeon; }
        int targetMap() { return targetIsDungeon() ? 200 + g_current.a : g_current.a; }
        std::string targetName()
        {
            if (targetIsDungeon())
                return std::string(kDungeons[g_current.a].name) + " floor " + std::to_string(g_current.b + 1);
            for (const Place &p : kPlaces)
                if (p.map == g_current.a)
                    return p.name;
            return "map " + std::to_string(g_current.a);
        }

        // The allies a dungeon's story floors need: their scenes load each ally's weapon, and an ally not yet in the
        // party has none (the game then stops on its own assertion, "commenu/weapon/"). Divine Beast Cave from floor
        // 11 (Xiao), then one more ally a dungeon; the Demon Shaft none. (Checked: Wise Owl Forest floor 4 stops with
        // Toan alone and plays with three; the Demon Shaft's 100 floors play with Toan alone.)
        int partyNeeded(int dungeon, int floor)
        {
            if (dungeon == 0)
                return floor >= 10 ? 2 : 1;
            return dungeon == 6 ? 1 : dungeon + 1;
        }

        void stepWarp(uint8_t *ram, const R5900Context *ctx, Runtime *rt)
        {
            const bool dungeonTarget = targetIsDungeon();
            if (dungeonTarget && g_phase == 0)
                if (const int needed = partyNeeded(g_current.a, g_current.b); rd<int8_t>(ram, kDngStatusInSave + 5u) < needed)
                    return finish(std::string("Not yet: ") + targetName() + " needs " + std::to_string(needed) +
                                  " party members. Advance the story to chapter " + std::to_string(needed) + " first");
            switch (g_phase)
            {
            case 0: // wait for an idle game, then fade out (or ask the dungeon to)
                if (townIdle(ram))
                {
                    call(rt, ram, ctx, kFadeOut, {30u}, {0.0f, 0.0f, 0.0f});
                    g_phase = 1;
                }
                else if (interiorIdle(ram))
                {
                    call(rt, ram, ctx, kFadeOut, {30u}, {0.0f, 0.0f, 0.0f});
                    g_phase = 30;
                }
                else if (g_waited > 1800u && mainMode(ram) == 2 &&
                         (rd<int32_t>(ram, kTownGameMode) == 0xE ||
                          (rd<int32_t>(ram, kTownGameMode) == 0xC && rd<int32_t>(ram, kInnerGameMode) == 4)))
                {
                    // A town or interior event that has run for 30 s (a later chapter's scene, out of its story): left
                    // as its own script leaves, the next map named and the event's result "map jump".
                    g_presetFloor = dungeonTarget ? g_current.b : -1;
                    call(rt, ram, ctx, kMapJump, {static_cast<uint32_t>(targetMap()), 0xFFFFFFFFu});
                    wr<int32_t>(ram, kEventResult, 8);
                    say("Leaving a long event for " + targetName());
                    g_phase = 2;
                }
                else if (dungeonIdle(ram, rt))
                {
                    if (dungeonTarget && currentDungeon(ram) == g_current.a)
                    {
                        call(rt, ram, ctx, kFadeOut, {30u}, {0.0f, 0.0f, 0.0f});
                        g_phase = 10;
                    }
                    else
                    {
                        wr<int32_t>(ram, kGameTask, 0xAF); // the dungeon's exit: a fade, then its town's MapJump
                        g_phase = 20;
                    }
                }
                break;
            case 1: // town: faded out; name the next map and leave
                if (rd<int32_t>(ram, kFadeEnd) == 1 || rd<int32_t>(ram, kFadeInOut) == 0)
                {
                    g_presetFloor = dungeonTarget ? g_current.b : -1;
                    call(rt, ram, ctx, kMapJump, {static_cast<uint32_t>(targetMap()), 0xFFFFFFFFu});
                    call(rt, ram, ctx, kExitLoopFn, {});
                    g_phase = 2;
                }
                break;
            case 30: // interior: faded out; the next map named, and EditInLoop's next call answers 99 (what it returns
                     // after a script's _MAP_JUMP): the town loop then exits to the map named (EditExit, as there)
                if (rd<int32_t>(ram, kFadeEnd) == 1 || rd<int32_t>(ram, kFadeInOut) == 0)
                {
                    g_presetFloor = dungeonTarget ? g_current.b : -1;
                    call(rt, ram, ctx, kMapJump, {static_cast<uint32_t>(targetMap()), 0xFFFFFFFFu});
                    g_interiorExit = true;
                    g_phase = 2;
                }
                break;
            case 10: // dungeon, same dungeon: faded out; the stairs' path builds the floor
                if (rd<int32_t>(ram, kFadeEnd) == 1 || rd<int32_t>(ram, kFadeInOut) == 0)
                {
                    const uint32_t s = userStatus(ram);
                    wr<int8_t>(ram, s + 2u, static_cast<int8_t>(g_current.b - 1)); // event 100 builds floor + 1
                    wr<int32_t>(ram, kEventNext, 100);
                    wr<int32_t>(ram, kEventReload, 0);
                    wr<int32_t>(ram, kGameTask, 0x190);
                    g_phase = 2;
                }
                break;
            case 20: // dungeon exit under way: replace its town by the target once it names it
                if (rd<int32_t>(ram, kExistFlag) == 2)
                {
                    if (!dungeonTarget)
                    {
                        wr<int32_t>(ram, kNextMapNo, targetMap());
                        g_phase = 2;
                    }
                    else
                        g_phase = 0; // to the town first, then into the target dungeon from there
                }
                break;
            case 2: // arrival: the target loaded (its own arrival scene may play on; the next command waits for it)
                if (dungeonTarget ? mainMode(ram) == 3 && rt->functions.generated(kDunResident) && userStatus(ram) != 0u &&
                                        currentDungeon(ram) == g_current.a && currentFloor(ram) == g_current.b &&
                                        rd<int32_t>(ram, kGameTask) != 0x190 && rd<int32_t>(ram, kBtGameMode) == 1
                                  : mainMode(ram) == 2 && rd<int32_t>(ram, kMapNo) == g_current.a && rd<int32_t>(ram, kNextMapNo) < 0 &&
                                        rd<int32_t>(ram, kFadeInOut) == 0)
                    finish("Arrived: " + targetName());
                break;
            }
        }

        // The next live monster of the floor (state 1, HP > 0), cycling; the player put 20 units from it, toward where
        // the player stood, through CCharacter::SetPosition (its attached frames follow), the camera snapped there.
        void teleportToMonster(uint8_t *ram, const R5900Context *ctx, Runtime *rt)
        {
            const uint32_t unit = rd<uint32_t>(ram, kMonsterUnit) & 0x1FFFFFFu;
            if (!unit)
                return finish("No monster unit");
            for (int k = 0; k < 16; ++k)
            {
                const int i = (g_nextMonster + k) % 16;
                const uint32_t rec = unit + 0x1E3D0u + static_cast<uint32_t>(i) * 400u;
                if (rd<int32_t>(ram, rec) != 1 || rd<int32_t>(ram, rec + 0x24u) <= 0)
                    continue;
                const uint32_t pos = unit + 0x1FCE0u + static_cast<uint32_t>(i) * 0x3510u;
                const float mx = rd<float>(ram, pos), my = rd<float>(ram, pos + 4u), mz = rd<float>(ram, pos + 8u);
                const float px = rd<float>(ram, kPlayer + 0x10u), pz = rd<float>(ram, kPlayer + 0x18u);
                float dx = px - mx, dz = pz - mz;
                const float length = std::sqrt(dx * dx + dz * dz);
                if (length > 0.001f)
                    dx /= length, dz /= length;
                else
                    dx = 0.0f, dz = 1.0f;
                const float x = mx + dx * 20.0f, z = mz + dz * 20.0f;
                wr<int32_t>(ram, kLockOn, 0);
                call(rt, ram, ctx, kSetPosition, {kPlayer}, {x, my, z});
                call(rt, ram, ctx, kSetFollow, {kCamera}, {x, my + 13.0f, z});
                call(rt, ram, ctx, kCameraStep, {kCamera, 0xFFFFFFFFu});
                g_nextMonster = i + 1;
                char text[96];
                std::snprintf(text, sizeof text, "Next to monster %d (HP %d) at %.0f %.0f %.0f", i, rd<int32_t>(ram, rec + 0x24u), mx, my, mz);
                return finish(text);
            }
            finish("No live monster on this floor");
        }

        // Every georama part of every town, as the dungeons' Atla tables give them (AtraAppearData), through the
        // game's AtraPartsGet / AtraChipGet; elements already owned (in the inventory or attached) are not given twice.
        // The dungeons' Atla are then marked collected (a never-entered dungeon is marked entered, so none are placed).
        // The Sun Sphere is not in the tables (the Coliseum gives it). The loaded town reloads its live copy.
        void giveGeorama(uint8_t *ram, const R5900Context *ctx, Runtime *rt)
        {
            int parts = 0, chips = 0;
            for (uint32_t t = 0; t < 6u; ++t)
            {
                const uint32_t rows = rd<uint32_t>(ram, kAtlaRows + t * 4u) & 0x1FFFFFFu;
                if (!rows)
                    continue;
                std::vector<int> wanted(60, 0);
                for (uint32_t r = rows; rd<int32_t>(ram, r) != -1; r += 12u)
                    if (const int id = rd<int32_t>(ram, r); id >= 40 && id < 100)
                        wanted[static_cast<size_t>(id - 40)] += std::max(1, rd<int32_t>(ram, r + 8u));
                auto owned = [&](int chip) {
                    int n = 0;
                    const uint32_t inventory = kSaveData + 0x4CCCu + t * 0x100u;
                    for (uint32_t k = 0; k < 128u; ++k)
                        n += rd<int16_t>(ram, inventory + k * 2u) == chip;
                    const uint32_t town = kSaveData + 0x67Cu + t * 3000u;
                    for (uint32_t k = 0; k < 24u; ++k)
                        for (uint32_t j = 0; j < 6u; ++j)
                            if (rd<int32_t>(ram, kEditPartsData + (t * 24u + k) * 0xBCu + (5u + 7u * j) * 4u) == chip &&
                                rd<int16_t>(ram, town + 0x68u + k * 0x18u + 10u + j * 2u) != 0)
                                ++n;
                    return n;
                };
                for (uint32_t r = rows; rd<int32_t>(ram, r) != -1; r += 12u)
                {
                    const int id = rd<int32_t>(ram, r), count = std::max(1, rd<int32_t>(ram, r + 8u));
                    for (int c = 0; c < count; ++c)
                        if (id < 40)
                        {
                            int32_t got = 0;
                            if (call(rt, ram, ctx, kPartsGet, {kSaveData, t, static_cast<uint32_t>(id)}, {}, &got) && got)
                                ++parts;
                        }
                        else if (id < 100 && owned(id - 40) < wanted[static_cast<size_t>(id - 40)])
                        {
                            call(rt, ram, ctx, kChipGet, {kSaveData, t, static_cast<uint32_t>(id - 40)});
                            ++chips;
                        }
                }
                // The dungeon side: nothing left to collect.
                const uint32_t s = kDngStatusInSave;
                if (rd<int8_t>(ram, s + 0x42BFu + t) == -1)
                    wr<int8_t>(ram, s + 0x42BFu + t, 0);
                for (uint32_t f = 0; f < 40u; ++f)
                    for (uint32_t k = 0; k < 8u; ++k)
                    {
                        const uint32_t slot = s + 0x278u + t * 0x500u + f * 0x20u + k * 4u;
                        if (rd<int32_t>(ram, slot) != -1)
                            wr<int32_t>(ram, slot, -3);
                    }
                for (uint32_t i = 0; i < 100u; ++i)
                    wr<int32_t>(ram, s + 0x2078u + t * 0x4B0u + i * 0xCu, -1);
            }
            if (mainMode(ram) == 2)
                if (const int town = rd<int32_t>(ram, kNowEditMap); town >= 0 && town < 6)
                    call(rt, ram, ctx, kPartsLoad, {kEditPartsLive, static_cast<uint32_t>(town), kSaveData, 0u});
            finish("Georama: " + std::to_string(parts) + " parts and " + std::to_string(chips) + " elements given");
        }

        // Every world-map point: the towns' and areas' visit counters, the dungeons' entry counters, the World Map item.
        void openWorld(uint8_t *ram, const R5900Context *ctx, Runtime *rt)
        {
            for (const int map : {0, 1, 14, 2, 3, 42, 23, 4, 40, 60})
                if (rd<int16_t>(ram, kSaveData + 0x12EE8u + static_cast<uint32_t>(map) * 2u) <= 0)
                    call(rt, ram, ctx, kVisitMap, {kSaveData, static_cast<uint32_t>(map), 1u});
            for (uint32_t d = 0; d < 6u; ++d)
                if (rd<int16_t>(ram, kSaveData + 0x12F88u + d * 2u) <= 0)
                    call(rt, ram, ctx, kQuestDungeon, {kSaveData, d, 1u});
            if (rd<int8_t>(ram, kDngStatusInSave + 0x4320u) == 0)
                call(rt, ram, ctx, kEdGetItem, {238u, 1u, 0xFFFFFFFFu});
            finish("World map: every town and dungeon shown");
        }

        // The chapter select: the cumulative state of every chapter before the target, as the transition scripts
        // write it (the chapter counter, game flags, party and ally weapons, the dungeons cleared, the places visited).
        struct Chapter
        {
            int counter, party, allyItem;
            int dungeonDone; // -1 none
            std::vector<int> visits, flags;
        };
        const Chapter kChapterSteps[] = {
            {0, 1, -1, -1, {}, {}},                                                               // 1 (nothing)
            {100, 2, 22, 0, {0}, {0, 1, 18, 24, 25, 32, 37, 40, 49, 50, 100, 101, 102, 103, 104, 105, 106}}, // 2
            {100, 3, 34, 1, {1, 14, 2}, {2, 3, 4, 5, 8, 9, 10, 11, 15, 16, 17, 19, 20, 21, 23, 31, 34, 43, 48, 51, 58, 111}}, // 3
            {300, 4, 47, 2, {3, 42}, {6, 12, 13, 14, 28, 42, 47, 57, 120, 121, 122, 123, 124}},   // 4
            {400, 5, 59, 3, {23, 4}, {26, 27, 38, 44, 52, 53, 54, 55, 130, 131, 132, 133, 134}}, // 5
            {500, 6, 71, 4, {40}, {22, 29, 30, 35, 36, 39, 45, 46, 56, 140}},                     // 6
            {600, 6, -1, 5, {60}, {41, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241}}, // 7
        };
        void applyChapter(uint8_t *ram, const R5900Context *ctx, Runtime *rt, int chapter)
        {
            if (chapter < 1 || chapter > 7)
                return finish("No such chapter");
            for (int c = 2; c <= chapter; ++c)
            {
                const Chapter &step = kChapterSteps[c - 1];
                for (const int f : step.flags)
                    call(rt, ram, ctx, kSetGameFlag, {kSaveData, static_cast<uint32_t>(f), 1u});
                if (rd<int8_t>(ram, kDngStatusInSave + 5u) < step.party)
                {
                    wr<int8_t>(ram, kDngStatusInSave + 5u, static_cast<int8_t>(step.party));
                    if (step.allyItem >= 0)
                    {
                        call(rt, ram, ctx, kEdGetItem, {static_cast<uint32_t>(step.allyItem), 1u, 0xFFFFFFFFu});
                        call(rt, ram, ctx, kEquipDefault, {static_cast<uint32_t>(step.party - 1)});
                    }
                }
                if (step.dungeonDone >= 0)
                {
                    const uint32_t deepest = kDngStatusInSave + 0x42BFu + static_cast<uint32_t>(step.dungeonDone);
                    wr<int8_t>(ram, deepest, static_cast<int8_t>(std::max<int>(rd<int8_t>(ram, deepest), kDungeons[step.dungeonDone].floors - 1)));
                    if (rd<int16_t>(ram, kSaveData + 0x12F88u + static_cast<uint32_t>(step.dungeonDone) * 2u) <= 0)
                        call(rt, ram, ctx, kQuestDungeon, {kSaveData, static_cast<uint32_t>(step.dungeonDone), 1u});
                }
                for (const int map : step.visits)
                    if (rd<int16_t>(ram, kSaveData + 0x12EE8u + static_cast<uint32_t>(map) * 2u) <= 0)
                        call(rt, ram, ctx, kVisitMap, {kSaveData, static_cast<uint32_t>(map), 1u});
                if (c == 2 && rd<int8_t>(ram, kDngStatusInSave + 0x4320u) == 0)
                    call(rt, ram, ctx, kEdGetItem, {238u, 1u, 0xFFFFFFFFu}); // the World Map (Norune, after Dran)
                if (c == 6)
                    call(rt, ram, ctx, kChipGet, {kSaveData, 4u, 15u}); // the Sun Sphere (the Coliseum's)
                if (c == 7)
                {
                    call(rt, ram, ctx, kSetIntFlag, {kSaveData, 10u, 3u});
                    wr<int32_t>(ram, kSaveData + 0x38u, 1); // the game-cleared mark (the Demon Shaft)
                    wr<int32_t>(ram, kGameClear, 1);
                }
            }
            if (rd<int32_t>(ram, kSaveData + 0x10668u) < kChapterSteps[chapter - 1].counter)
                call(rt, ram, ctx, kSetIntFlag, {kSaveData, 0u, static_cast<uint32_t>(kChapterSteps[chapter - 1].counter)});
            finish(std::string("Story: chapter ") + kChapters[chapter]);
        }

        bool setCheat(const std::string &name, bool on)
        {
            using C = dc::cheats::Cheat;
            static const std::pair<const char *, C> kNames[] = {
                {"health", C::InfiniteHealth}, {"durability", C::InfiniteWeaponDurability}, {"thirst", C::InfiniteThirst},
                {"stamina", C::InfiniteStamina}, {"time", C::FreezeTimeOfDay}, {"fast", C::FastForwardLock},
                {"fishing", C::FishingHelper}, {"map", C::RevealFloorMap}, {"onehit", C::OneHitKills},
                {"nodamage", C::NoDamageTaken}, {"key", C::KeyFinder}};
            for (const auto &[n, c] : kNames)
                if (name == n)
                {
                    dc::cheats::setEnabled(c, on);
                    return true;
                }
            return false;
        }

        void stepExecutor(uint8_t *ram, const R5900Context *ctx, Runtime *rt)
        {
            for (;;)
            {
                Command now;
                {
                    std::lock_guard<std::mutex> lock(g_mutex);
                    if (g_immediate.empty())
                        break;
                    now = g_immediate.front();
                    g_immediate.pop_front();
                }
                if (now.kind == Kind::Peek)
                    say(where(ram));
                else if (now.kind == Kind::Talk)
                    say((g_talk = now.a != 0) ? "Dialogue advance on" : "Dialogue advance off");
                else
                    say(setCheat(now.text, now.a != 0) ? "Cheat " + now.text + (now.a ? " on" : " off") : "Unknown cheat " + now.text);
            }
            if (!g_active)
            {
                std::lock_guard<std::mutex> lock(g_mutex);
                if (g_queue.empty())
                    return;
                g_current = g_queue.front();
                g_queue.pop_front();
                g_active = true;
                g_phase = 0;
                g_waited = 0;
            }
            if (g_current.kind == Kind::Wait) // wait <frames>: the next command that much later
            {
                if (++g_waited >= static_cast<uint32_t>(std::max(0, g_current.a)))
                    finish("Waited " + std::to_string(g_current.a) + " frames");
                return;
            }
            // Everything else waits for an idle town or dungeon (at most two minutes of frames).
            if (++g_waited > 7200u)
                return finish("Gave up at step " + std::to_string(g_phase) + ": the game never became idle (" + where(ram) + ")");
            const bool idle = townIdle(ram) || interiorIdle(ram) || dungeonIdle(ram, rt);
            switch (g_current.kind)
            {
            case Kind::Where:
                return finish(where(ram));
            case Kind::Place:
            case Kind::Dungeon:
                return stepWarp(ram, ctx, rt);
            case Kind::Monster:
                if (dungeonIdle(ram, rt))
                    teleportToMonster(ram, ctx, rt);
                else if (mainMode(ram) != 3)
                    finish("Monster teleport: only in a dungeon");
                return;
            case Kind::Georama:
                if (idle)
                    giveGeorama(ram, ctx, rt);
                return;
            case Kind::World:
                if (idle)
                    openWorld(ram, ctx, rt);
                return;
            case Kind::Chapter:
                if (idle)
                    applyChapter(ram, ctx, rt, g_current.a);
                return;
            case Kind::Flag:
                if (idle)
                {
                    call(rt, ram, ctx, kSetGameFlag, {kSaveData, static_cast<uint32_t>(g_current.a), static_cast<uint32_t>(g_current.b)});
                    finish("Game flag " + std::to_string(g_current.a) + " = " + std::to_string(g_current.b));
                }
                return;
            case Kind::IntFlag:
                if (idle)
                {
                    call(rt, ram, ctx, kSetIntFlag, {kSaveData, static_cast<uint32_t>(g_current.a), static_cast<uint32_t>(g_current.b)});
                    finish("Game int flag " + std::to_string(g_current.a) + " = " + std::to_string(g_current.b));
                }
                return;
            default:
                return;
            }
        }

        RecompiledFunction g_padUpdate = nullptr, g_enterMenu = nullptr, g_interiorLoop = nullptr;
        constexpr uint32_t kInteriorLoop = 0x0019C2B0u; // EditInLoop: 99 = leave for NextMapNo
        void interiorLoopHook(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (g_interiorExit)
            {
                g_interiorExit = false;
                ctx->r[2].ud[0] = 99u;
                ctx->pc = ctx->r[31].uw[0];
                return;
            }
            g_interiorLoop(ram, ctx, rt);
        }
        void padUpdateHook(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            stepExecutor(ram, ctx, rt);
            const bool tap = g_talk && talking(ram) && (++g_talkClock % 16u) == 0u;
            dc::pad::press(tap ? 0x40u : 0u); // Cross, for this frame's read only
            g_padUpdate(ram, ctx, rt);
            dc::pad::press(0u);
        }
        // The entrance menu, from the dungeon's arrival event: a warp's floor as its preset (it picks it by itself).
        void enterMenuHook(uint8_t *ram, R5900Context *ctx, Runtime *rt)
        {
            if (g_presetFloor >= 0 && static_cast<uint32_t>(ctx->r[31].uw[0]) == kEnterMenuReturn)
            {
                ctx->r[6].ud[0] = static_cast<uint64_t>(g_presetFloor);
                g_presetFloor = -1;
            }
            g_enterMenu(ram, ctx, rt);
        }

        bool parse(const std::string &line, Command &out)
        {
            std::istringstream in(line);
            std::string word;
            if (!(in >> word))
                return false;
            if (word == "place" && (in >> out.a))
                return out.kind = Kind::Place, true;
            if (word == "dungeon" && (in >> out.a >> out.b) && out.a >= 0 && out.a < 7)
                return out.kind = Kind::Dungeon, out.b = std::clamp(out.b - 1, 0, kDungeons[out.a].floors - 1), true;
            if (word == "monster")
                return out.kind = Kind::Monster, true;
            if (word == "georama")
                return out.kind = Kind::Georama, true;
            if (word == "world")
                return out.kind = Kind::World, true;
            if (word == "chapter" && (in >> out.a))
                return out.kind = Kind::Chapter, true;
            if (word == "flag" && (in >> out.a >> out.b))
                return out.kind = Kind::Flag, true;
            if (word == "intflag" && (in >> out.a >> out.b))
                return out.kind = Kind::IntFlag, true;
            if (word == "where")
                return out.kind = Kind::Where, true;
            if (word == "peek")
                return out.kind = Kind::Peek, true;
            if (word == "talk" && (in >> out.a))
                return out.kind = Kind::Talk, true;
            if (word == "wait" && (in >> out.a))
                return out.kind = Kind::Wait, true;
            if (word == "cheat" && (in >> out.text >> out.a))
                return out.kind = Kind::Cheat, true;
            return false;
        }
    }

    int placeCount() { return static_cast<int>(std::size(kPlaces)); }
    Place place(int index) { return kPlaces[std::clamp(index, 0, placeCount() - 1)]; }
    int dungeonCount() { return static_cast<int>(std::size(kDungeons)); }
    Dungeon dungeon(int index) { return kDungeons[std::clamp(index, 0, dungeonCount() - 1)]; }
    int chapterCount() { return 7; }
    const char *chapterName(int chapter) { return kChapters[std::clamp(chapter, 1, 7)]; }
    const char *chapterShort(int chapter)
    {
        static const char *const kShort[] = {"", "1 Norune", "2 Matataki", "3 Queens", "4 Muska Lacka", "5 Yellow Drops", "6 Dark Heaven", "7 Post-game"};
        return kShort[std::clamp(chapter, 1, 7)];
    }

    void requestPlace(int map) { push({Kind::Place, map}); }
    void requestDungeon(int d, int floor)
    {
        d = std::clamp(d, 0, dungeonCount() - 1);
        push({Kind::Dungeon, d, std::clamp(floor, 0, kDungeons[d].floors - 1)});
    }
    void requestMonster() { push({Kind::Monster}); }
    void requestGeorama() { push({Kind::Georama}); }
    void requestWorld() { push({Kind::World}); }
    void requestChapter(int chapter) { push({Kind::Chapter, chapter}); }
    std::string status()
    {
        std::lock_guard<std::mutex> lock(g_mutex);
        return g_status;
    }
    bool busy() { return g_busy.load(); }

    // The file's lines: "<frame> <command> [args]", '#' starts a comment. Commands: place <map>, dungeon <d> <floor
    // 1-based>, monster, georama, world, chapter <1..7>, flag <i> <0|1>, intflag <i> <value>, cheat <name> <0|1>
    // (health durability thirst stamina time fast fishing map onehit nodamage key), where (the state, logged, in turn),
    // peek (the same at once), talk <0|1>
    // (Cross tapped through dialogue and events), wait <frames> (the next command that much later).
    // Each is queued when the game reaches its frame and carried out when the game is idle, in order.
    bool loadScript(const std::string &path)
    {
        std::ifstream in(path);
        if (!in)
            return false;
        std::string line;
        int number = 0;
        while (std::getline(in, line))
        {
            ++number;
            if (const size_t hash = line.find('#'); hash != std::string::npos)
                line.resize(hash);
            std::istringstream words(line);
            uint64_t frame = 0;
            if (!(words >> frame))
                continue;
            std::string rest;
            std::getline(words, rest);
            Command command;
            if (!parse(rest, command))
            {
                std::fprintf(stderr, "[dc:warp] %s:%d: not a command: %s\n", path.c_str(), number, rest.c_str());
                return false;
            }
            g_script.push_back({frame, command});
        }
        std::stable_sort(g_script.begin(), g_script.end(), [](const Scheduled &a, const Scheduled &b) { return a.frame < b.frame; });
        std::fprintf(stderr, "[dc:warp] %zu scripted commands from %s\n", g_script.size(), path.c_str());
        return true;
    }
    void onFrame(uint64_t frame)
    {
        g_frame.store(frame);
        while (g_scriptNext < g_script.size() && g_script[g_scriptNext].frame <= frame)
        {
            std::fprintf(stderr, "[dc:warp] frame %llu: queued\n", static_cast<unsigned long long>(frame));
            push(g_script[g_scriptNext++].command);
        }
    }

    void install(Runtime &runtime, bozo::port::Registry &registry)
    {
        if ((g_padUpdate = runtime.functions.generated(kPadUpdate)))
            registry.registerFunction(kPadUpdate, &padUpdateHook);
        if ((g_enterMenu = runtime.functions.generated(kEnterMenu)))
            registry.registerFunction(kEnterMenu, &enterMenuHook);
        if ((g_interiorLoop = runtime.functions.generated(kInteriorLoop)))
            registry.registerFunction(kInteriorLoop, &interiorLoopHook);
    }
}
