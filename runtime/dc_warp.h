#pragma once
// Travel and progress cheats: warps to any town, area or dungeon floor, a teleport to the floor's monsters, every
// georama part, every world-map place, and the story chapters. Requests come from any thread (the menus, the
// --cheat-script file); the game thread carries them out through the game's own routines once it is idle (a town's
// free walk, a dungeon's free play), one at a time.
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <cstdint>
#include <string>

namespace dc::warp
{
    // The places a warp reaches: the towns and areas the disc has a map for (gedit/eNN, sNN).
    struct Place
    {
        int map;
        const char *name;
    };
    int placeCount();
    Place place(int index);
    // The dungeons (map 200 + index) and their floor counts.
    struct Dungeon
    {
        const char *name;
        int floors;
    };
    int dungeonCount();
    Dungeon dungeon(int index);
    // The story chapters a chapter select reaches (1 the start of the game ... 7 after the Dark Genie).
    int chapterCount();
    const char *chapterName(int chapter);
    const char *chapterShort(int chapter); // "3 Queens": for narrow menus

    void requestPlace(int map);                // a town or area by map number, its default entrance
    void requestDungeon(int dungeon, int floor); // floor 0-based; arrives through the game's own floor build
    void requestMonster();                     // in a dungeon: next to the next live monster of the floor
    void requestGeorama();                     // every georama part of every town, as if every Atla were opened
    void requestWorld();                       // every town and dungeon shown on the world map, the World Map item
    void requestChapter(int chapter);          // the story state a playthrough has at that chapter's start
    // What happened last ("Warped to ...", "Waiting for free walk", ...), for the menus.
    std::string status();
    bool busy();

    // --cheat-script FILE: lines "<frame> <command> [args]" queued as the game reaches each frame (see dc_warp.cpp).
    bool loadScript(const std::string &path);
    void onFrame(uint64_t frame);

    void install(Runtime &runtime, bozo::port::Registry &registry);
}
