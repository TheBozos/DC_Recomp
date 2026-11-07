// Session-only cheat options. The UI touches atomic options; only the game
// thread accesses guest state, at the actual resource-update boundaries.
#include "dc_cheats.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>

namespace dc::cheats
{
    namespace
    {
        std::array<std::atomic<bool>, 3> options{};
        RecompiledFunction addLife = nullptr, setLife = nullptr, addDrink = nullptr, step = nullptr, wear = nullptr;
        constexpr uint32_t kAddLife = 0x001BE710u, kSetLife = 0x001BE900u, kAddDrink = 0x001BE510u,
                           kStep = 0x001BEA50u, kWear = 0x001B5D90u;
        template<class T> T read(const uint8_t *ram, uint32_t at)
        { T value; std::memcpy(&value, ram + at, sizeof(value)); return value; }
        template<class T> void write(uint8_t *ram, uint32_t at, T value)
        { std::memcpy(ram + at, &value, sizeof(value)); }
        bool validStatus(uint32_t status) { return status != 0 && status <= bozo::kFlatScratchpadOffset - 0x8B14u; }
        bool health(uint8_t *ram, uint32_t status, uint32_t character)
        {
            if (!validStatus(status) || character >= 6) return false;
            const int16_t maximum = read<int16_t>(ram, status + 6 + character * 2);
            if (maximum <= 0) return false;
            write(ram, status + 0x12 + character * 2, maximum);
            write(ram, status + 0x8A5C + character * 2, maximum);
            write<int16_t>(ram, status + 0x8A68 + character * 2, 0); // cancel queued HP loss
            return true;
        }
        bool thirst(uint8_t *ram, uint32_t status, uint32_t character)
        {
            if (!validStatus(status) || character >= 6) return false;
            const float maximum = read<float>(ram, status + 0x42EC + character * 4);
            if (!std::isfinite(maximum) || maximum <= 0) return false;
            write(ram, status + 0x4304 + character * 4, maximum);
            write<int16_t>(ram, status + 0x8AD6 + character * 2, static_cast<int16_t>(std::min(maximum, 32767.0f)));
            write<int16_t>(ram, status + 0x8AE2 + character * 2, 0); // cancel queued drink loss
            write<float>(ram, status + 0x8AF0 + character * 4, 0); // dehydration damage timer
            return true;
        }
        void weapons(uint8_t *ram, uint32_t status)
        {
            if (!validStatus(status)) return;
            // BattleSubWeaponDmg's equipped record: six character inventories,
            // each with eleven 248-byte records and its own selected slot.
            for (uint32_t character = 0; character < 6; ++character)
            {
                const int slot = read<int8_t>(ram, status + 0x4340 + character);
                if (slot < 0 || slot >= 11) continue;
                const uint32_t record = status + 0x450C + character * 0xAA8 + static_cast<uint32_t>(slot) * 0xF8;
                if (read<int16_t>(ram, record) <= 0) continue;
                const int16_t maximum = read<int16_t>(ram, record + 0xC);
                if (maximum > 0) write<float>(ram, record + 0x10, static_cast<float>(maximum));
            }
        }
        void maintain(uint8_t *ram, uint32_t status)
        {
            if (enabled(Cheat::InfiniteHealth))
                for (uint32_t character = 0; character < 6; ++character) health(ram, status, character);
            if (enabled(Cheat::InfiniteThirst))
                for (uint32_t character = 0; character < 6; ++character) thirst(ram, status, character);
            if (enabled(Cheat::InfiniteWeaponDurability)) weapons(ram, status);
        }
        void returnVoid(R5900Context *ctx) { ctx->pc = getRegU32(ctx, 31); }
        void addLifeHook(uint8_t *ram, R5900Context *ctx, Runtime *runtime)
        {
            if (enabled(Cheat::InfiniteHealth) && health(ram, getRegU32(ctx, 4), getRegU32(ctx, 5))) returnVoid(ctx);
            else addLife(ram, ctx, runtime);
        }
        void setLifeHook(uint8_t *ram, R5900Context *ctx, Runtime *runtime)
        {
            if (enabled(Cheat::InfiniteHealth) && health(ram, getRegU32(ctx, 4), getRegU32(ctx, 5))) returnVoid(ctx);
            else setLife(ram, ctx, runtime);
        }
        void addDrinkHook(uint8_t *ram, R5900Context *ctx, Runtime *runtime)
        {
            if (enabled(Cheat::InfiniteThirst) && thirst(ram, getRegU32(ctx, 4), getRegU32(ctx, 5))) returnVoid(ctx);
            else addDrink(ram, ctx, runtime);
        }
        void stepHook(uint8_t *ram, R5900Context *ctx, Runtime *runtime)
        {
            const uint32_t status = getRegU32(ctx, 4), caller = getRegU32(ctx, 31);
            maintain(ram, status);
            step(ram, ctx, runtime);
            if (!ctx->leaving && ctx->pc == caller) maintain(ram, status);
        }
        void wearHook(uint8_t *ram, R5900Context *ctx, Runtime *runtime)
        {
            if (!enabled(Cheat::InfiniteWeaponDurability)) { wear(ram, ctx, runtime); return; }
            const uint32_t pointer = getRegU32(ctx, 28) - 0x6388u;
            const int64_t offset = bozo::GuestMemory::flatOffset(pointer, 4);
            if (offset >= 0 && offset < bozo::kFlatScratchpadOffset)
                weapons(ram, read<uint32_t>(ram, static_cast<uint32_t>(offset)));
            // No wear means no breakage, loss of attachments, auto-repair powder
            // consumption, or break/equip result returned to SwordDmgCheck1.
            setReturnS32(ctx, 0);
            returnVoid(ctx);
        }
    }
    bool enabled(Cheat cheat) { return options[static_cast<size_t>(cheat)].load(std::memory_order_relaxed); }
    void setEnabled(Cheat cheat, bool on) { options[static_cast<size_t>(cheat)].store(on, std::memory_order_relaxed); }
    void install(Runtime &runtime, bozo::port::Registry &registry)
    {
        addLife = runtime.functions.generated(kAddLife); setLife = runtime.functions.generated(kSetLife);
        addDrink = runtime.functions.generated(kAddDrink); step = runtime.functions.generated(kStep);
        wear = runtime.functions.generated(kWear);
        if (!addLife || !setLife || !addDrink || !step || !wear) throw std::runtime_error("cheats: resource routines unavailable");
        registry.registerFunction(kAddLife, addLifeHook); registry.registerFunction(kSetLife, setLifeHook);
        registry.registerFunction(kAddDrink, addDrinkHook); registry.registerFunction(kStep, stepHook);
        registry.registerFunction(kWear, wearHook);
    }
}
