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
#include <bit>
#include <stdexcept>

namespace dc::cheats
{
    namespace
    {
        std::array<std::atomic<bool>, 5> options{};
        std::atomic<int> currentAttack{-1}, pendingAttack{-1};
        std::atomic<int> absRate{1},moveRate{100},currentGilda{-1},pendingGilda{-1};
        std::atomic<int> currentSize{-1},minimumSize{1},pendingSize{-1};
        std::array<std::atomic<uint64_t>,3> currentItems{},pendingItems{};
        std::atomic<uint32_t> timeGeneration{0};
        uint32_t observedGeneration=~0u;
        float heldTime=0;
        bool haveHeldTime=false;
        bozo::port::InstructionHandler priorInstruction=nullptr;
        RecompiledFunction moveCheck=nullptr,sky=nullptr,light=nullptr;
        constexpr uint32_t kMoveCheck=0x14A680u,kSky=0x187F90u,kLight=0x188D50u,kAbsGain=0x1DF2A0u;
        RecompiledFunction addLife = nullptr, setLife = nullptr, addDrink = nullptr, step = nullptr, wear = nullptr;
        constexpr uint32_t kAddLife = 0x001BE710u, kSetLife = 0x001BE900u, kAddDrink = 0x001BE510u,
                           kStep = 0x001BEA50u, kWear = 0x001B5D90u;
        template<class T> T read(const uint8_t *ram, uint32_t at)
        { T value; std::memcpy(&value, ram + at, sizeof(value)); return value; }
        template<class T> void write(uint8_t *ram, uint32_t at, T value)
        { std::memcpy(ram + at, &value, sizeof(value)); }
        bool validStatus(uint32_t status) { return status != 0 && status <= bozo::kFlatScratchpadOffset - 0x8B14u; }
        uint32_t playerStatus(const uint8_t *ram,const R5900Context *ctx)
        {
            // CSaveData::GetDngStatus returns this embedded record in both town
            // and dungeon. DUN's cached gp-0x6388 pointer is not set in town.
            const int64_t ownerAt=bozo::GuestMemory::flatOffset(getRegU32(ctx,28)-0x72E4u,4);
            if(ownerAt>=0 && ownerAt<bozo::kFlatScratchpadOffset){
                const uint32_t owner=read<uint32_t>(ram,uint32_t(ownerAt));
                if(owner && owner<=bozo::kFlatScratchpadOffset-0xDF20u)return owner+0x540Cu;
            }
            const int64_t at=bozo::GuestMemory::flatOffset(getRegU32(ctx,28)-0x6388u,4);
            if (at<0 || at>=bozo::kFlatScratchpadOffset) return 0;
            const uint32_t status=read<uint32_t>(ram,static_cast<uint32_t>(at));
            return validStatus(status) ? status : 0;
        }
        uint32_t equippedWeapon(const uint8_t *ram, uint32_t status)
        {
            if (!validStatus(status)) return 0;
            const int character = read<int8_t>(ram,status+4);
            if (character<0 || character>=6) return 0;
            const int slot = read<int8_t>(ram,status+0x4340+character);
            if (slot<0 || slot>=11) return 0;
            const uint32_t record=status+0x450C+character*0xAA8+slot*0xF8;
            return read<int16_t>(ram,record)>0 ? record : 0;
        }
        void weaponAttack(uint8_t *ram,uint32_t status);
        // GetItem and GetNowItemNum count active quantities as bag occupancy.
        // The backing array has 103 entries; ordinary capacity is at most 100.
        int occupied(const uint8_t *ram,uint32_t s,int excludedSlot=-1)
        {
            int n=0;
            for(int i=0;i<103;++i){const int id=read<int16_t>(ram,s+0x436eu+i*2);if(id>=132 && id<257)++n;}
            for(int i=0;i<3;++i)if(i!=excludedSlot && read<int16_t>(ram,s+0x4362+i*2)!=-1)
                n+=std::max(0,int(read<int16_t>(ram,s+0x4368+i*2)));
            return n;
        }
        int minimumBag(const uint8_t *ram,uint32_t s)
        {
            int n=std::max(1,occupied(ram,s));
            for(int i=0;i<103;++i)if(read<int16_t>(ram,s+0x436eu+i*2)!=-1)n=std::max(n,i+1);
            return n;
        }
        Consumable item(const uint8_t *ram,uint32_t s,int slot)
        {
            if(!validStatus(s) || slot<0 || slot>=3)return {};
            const int id=read<int16_t>(ram,s+0x4362+slot*2),quantity=read<int16_t>(ram,s+0x4368+slot*2);
            const int capacity=read<int8_t>(ram,s+0x4360);
            if(id<132 || id>=257 || quantity<=0 || capacity<1 || capacity>100)return {};
            return {id,quantity,std::clamp(capacity-occupied(ram,s,slot),0,99)};
        }
        void publishPlayer(const uint8_t *ram,uint32_t s)
        {
            const auto w=equippedWeapon(ram,s);
            currentAttack.store(w?read<int16_t>(ram,w+4):-1,std::memory_order_relaxed);
            currentGilda.store(s?read<uint16_t>(ram,s+0x4346):-1,std::memory_order_relaxed);
            currentSize.store(s?read<int8_t>(ram,s+0x4360):-1,std::memory_order_relaxed);
            minimumSize.store(s?minimumBag(ram,s):1,std::memory_order_relaxed);
            for(int i=0;i<3;++i){const auto a=item(ram,s,i);currentItems[i].store(
                a.id<0?0:uint64_t(a.id)|(uint64_t(a.quantity)<<16)|(uint64_t(a.maximum)<<32),std::memory_order_relaxed);}
        }
        void editPlayer(uint8_t *ram,uint32_t s)
        {
            const int size=pendingSize.exchange(-1,std::memory_order_relaxed);
            const int gold=pendingGilda.exchange(-1,std::memory_order_relaxed);
            if(s && size>=minimumBag(ram,s))write<int8_t>(ram,s+0x4360,static_cast<int8_t>(size));
            if(s && gold>=0)write<uint16_t>(ram,s+0x4346,static_cast<uint16_t>(gold));
            for(int i=0;i<3;++i){const auto request=pendingItems[i].exchange(0,std::memory_order_relaxed);const auto a=item(ram,s,i);
                if(request && a.id==int(request&65535) && a.maximum>0)
                    write<int16_t>(ram,s+0x4368+i*2,static_cast<int16_t>(std::clamp(int(request>>16),1,a.maximum)));}
            weaponAttack(ram,s);
            publishPlayer(ram,s);
        }
        void moveHook(uint8_t *ram,R5900Context *ctx,Runtime *rt)
        {
            const uint32_t caller=getRegU32(ctx,31);
            const bool town=caller==0x16A9C0u;
            const bool dungeon=(caller==0x1DB39F4u || caller==0x1DB3EA0u || caller==0x1DB5C10u) && rt->functions.generated(0x1DB7450u);
            if(town || dungeon){
                editPlayer(ram,playerStatus(ram,ctx));
                const int rate=movementPercent();
                const int64_t p=bozo::GuestMemory::flatOffset(getRegU32(ctx,5),12);
                if(rate!=100 && p>0 && p<=bozo::kFlatScratchpadOffset-12){
                    // MoveCheck takes a displacement, not a destination. Keep
                    // gravity/steps on the original Y path and run collision.
                    const float x=read<float>(ram,uint32_t(p)),z=read<float>(ram,uint32_t(p)+8);
                    if(std::isfinite(x) && std::isfinite(z)){
                        write<float>(ram,uint32_t(p),x*(rate/100.f));write<float>(ram,uint32_t(p)+8,z*(rate/100.f));}
                }
            }
            moveCheck(ram,ctx,rt);
        }
        void timeInput(R5900Context *ctx)
        {
            const auto generation=timeGeneration.load(std::memory_order_relaxed);
            if(generation!=observedGeneration){observedGeneration=generation;haveHeldTime=false;}
            if(!enabled(Cheat::FreezeTimeOfDay))return;
            const float time=std::bit_cast<float>(ctx->fu[12]);
            if(!haveHeldTime && std::isfinite(time) && time>=0 && time<=12){heldTime=time;haveHeldTime=true;}
            if(haveHeldTime)ctx->fu[12]=std::bit_cast<uint32_t>(heldTime);
        }
        void skyHook(uint8_t *r,R5900Context *c,Runtime *rt){timeInput(c);sky(r,c,rt);}
        void lightHook(uint8_t *r,R5900Context *c,Runtime *rt){timeInput(c);light(r,c,rt);}
        bool instruction(uint8_t *r,R5900Context *c,Runtime *rt,uint32_t pc)
        {
            if(pc==kAbsGain){
                // This is the positive reward branch, after bonus modifiers and
                // the reverse-ABS floor branch. The original cap/full message
                // and level-up logic still execute, without short overflow.
                const auto w=equippedWeapon(r,playerStatus(r,c));
                const int reward=getRegS32(c,7),rate=absMultiplier();
                if(rate!=1 && reward>0 && w && w==getRegU32(c,19))
                    c->r[7].ud[0]=uint64_t(std::min(int64_t(reward)*rate,int64_t(32767)));
                return false; // execute the original lh v1,0x14(s3)
            }
            return priorInstruction && priorInstruction(r,c,rt,pc);
        }
        void weaponAttack(uint8_t *ram,uint32_t status)
        {
            const int requested=pendingAttack.exchange(-1,std::memory_order_relaxed);
            const uint32_t weapon=equippedWeapon(ram,status);
            if (weapon && requested>=0) write<int16_t>(ram,weapon+4,static_cast<int16_t>(requested));
            currentAttack.store(weapon ? read<int16_t>(ram,weapon+4) : -1,std::memory_order_relaxed);
        }
        void stamina(uint8_t *ram,uint32_t status,uint32_t caller,Runtime &runtime)
        {
            // This call site is DUN motionDrive's status tick. The selected
            // function table must also expose DUN motionDrive, so TITLE's
            // overlapping storage is never treated as the combat gauge.
            if (!enabled(Cheat::InfiniteStamina) || !validStatus(status) || caller!=0x01DB829Cu ||
                !runtime.functions.generated(0x01DB7450u)) return;
            // DUN MainDraw displays this 0..100 Speed/combo gauge. motionDrive
            // clamps its natural refill to 100 and clears its exhausted state.
            write<float>(ram,0x01DC44C8u,100.0f);
            write<int32_t>(ram,0x01DC4524u,0);
        }
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
            const uint32_t player=playerStatus(ram,ctx);
            // The weapon menu ticks a preview copy of CUserStatus. Queue edits
            // until the real player is ticking again, rather than consuming
            // the request on a temporary preview/inventory allocation.
            if (!player || player==status) editPlayer(ram,player);
            else
            {
                const uint32_t weapon=equippedWeapon(ram,player);
                currentAttack.store(weapon ? read<int16_t>(ram,weapon+4) : -1,std::memory_order_relaxed);
            }
            stamina(ram,status,caller,*runtime);
            maintain(ram, status);
            step(ram, ctx, runtime);
            if (!ctx->leaving && ctx->pc == caller)
            {
                maintain(ram, status);
                stamina(ram,status,caller,*runtime);
                const uint32_t weapon=equippedWeapon(ram,playerStatus(ram,ctx));
                currentAttack.store(weapon ? read<int16_t>(ram,weapon+4) : -1,std::memory_order_relaxed);
            }
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
    void setEnabled(Cheat cheat, bool on) {
        if(options[static_cast<size_t>(cheat)].exchange(on,std::memory_order_relaxed)!=on && cheat==Cheat::FreezeTimeOfDay)
            timeGeneration.fetch_add(1,std::memory_order_relaxed);
    }
    int equippedWeaponAttack() { return currentAttack.load(std::memory_order_relaxed); }
    void requestWeaponAttack(int value) { pendingAttack.store(std::clamp(value,0,999),std::memory_order_relaxed); }
    int absMultiplier(){return absRate.load(std::memory_order_relaxed);}
    void setAbsMultiplier(int v){absRate.store(std::clamp(v,1,16),std::memory_order_relaxed);}
    int movementPercent(){return moveRate.load(std::memory_order_relaxed);}
    void setMovementPercent(int v){moveRate.store(std::clamp(v,50,200),std::memory_order_relaxed);}
    int gilda(){return currentGilda.load(std::memory_order_relaxed);}
    void requestGilda(int v){pendingGilda.store(std::clamp(v,0,65535),std::memory_order_relaxed);}
    int inventorySize(){return currentSize.load(std::memory_order_relaxed);}
    int minimumInventorySize(){return minimumSize.load(std::memory_order_relaxed);}
    void requestInventorySize(int v){pendingSize.store(std::clamp(v,1,100),std::memory_order_relaxed);}
    Consumable activeConsumable(int slot){
        if(slot<0 || slot>=3)return {};const auto v=currentItems[slot].load(std::memory_order_relaxed);
        return v?Consumable{int(v&65535),int((v>>16)&65535),int(v>>32)}:Consumable{};
    }
    void requestConsumableQuantity(int slot,int quantity){
        const auto a=activeConsumable(slot);if(a.id<0 || a.maximum<1)return;
        pendingItems[slot].store(uint64_t(a.id)|(uint64_t(std::clamp(quantity,1,a.maximum))<<16),std::memory_order_relaxed);
    }
    void refreshPlayer(const uint8_t *r,const R5900Context *c){publishPlayer(r,playerStatus(r,c));}
    void applyPlayerEdits(uint8_t *r,const R5900Context *c){editPlayer(r,playerStatus(r,c));}
    void install(Runtime &runtime, bozo::port::Registry &registry)
    {
        addLife = runtime.functions.generated(kAddLife); setLife = runtime.functions.generated(kSetLife);
        addDrink = runtime.functions.generated(kAddDrink); step = runtime.functions.generated(kStep);
        wear = runtime.functions.generated(kWear);
        if (!addLife || !setLife || !addDrink || !step || !wear) throw std::runtime_error("cheats: resource routines unavailable");
        registry.registerFunction(kAddLife, addLifeHook); registry.registerFunction(kSetLife, setLifeHook);
        registry.registerFunction(kAddDrink, addDrinkHook); registry.registerFunction(kStep, stepHook);
        registry.registerFunction(kWear, wearHook);
        moveCheck=runtime.functions.generated(kMoveCheck);
        sky=registry.find(kSky);if(!sky)sky=runtime.functions.generated(kSky);
        light=registry.find(kLight);if(!light)light=runtime.functions.generated(kLight);
        if(moveCheck)registry.registerFunction(kMoveCheck,moveHook);
        if(sky)registry.registerFunction(kSky,skyHook);
        if(light)registry.registerFunction(kLight,lightHook);
        priorInstruction=registry.instructionHandler();registry.nativeGraphics(instruction);
    }
}
