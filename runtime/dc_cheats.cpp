// Session-only cheat options. The UI touches atomic options; only the game
// thread accesses guest state, at the actual resource-update boundaries.
#include "dc_cheats.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <array>
#include <atomic>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <bit>
#include <mutex>
#include <string>
#include <vector>
#include <stdexcept>

namespace dc::cheats
{
    namespace
    {
        std::array<std::atomic<bool>, 11> options{};
        std::atomic<int> damageRate{1};
        std::atomic<int> fastRate{300};
        std::atomic<bool> fastHeld{false};
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
        // Fishing (town only): every fish decision is rand_check(p) (0x240050, one rand() per call); a hooked fish
        // escapes only when its action timer (+0x58) runs out in state 8 (+0x50). DC1 has no line break.
        RecompiledFunction randCheck=nullptr,fishStep=nullptr;
        constexpr uint32_t kRandCheck=0x240050u,kFishStep=0x240480u;
        constexpr uint32_t kLeaveFood=0x2403FCu,kLeaveHook=0x24044Cu; // rand_check's return addresses in LeaveFood/LeaveHook
        // Dungeon floor maps: the two CDungeonMap objects of DUN's .bss (main floor, sub-map), each with a 20 x 20
        // explored grid (+0x8710, ints, index x + z * 20), 16 room-visited flags (+0x8) and the cell table (+0x9C50,
        // 16-byte records, part -1: no floor). FlushCheckMask clears them as a floor is built; checkMask marks them as
        // the player walks; only DrawMiniMap reads them (not saved).
        RecompiledFunction checkMask=nullptr,flushMask=nullptr;
        constexpr uint32_t kCheckMask=0x1C39C0u,kFlushMask=0x1C3C00u,kDunResident=0x1DB7450u;
        constexpr uint32_t kMapMain=0x01DC4BE0u,kMapSub=0x01DD56F0u;
        // Calling compiled game routines from a hook: GetWeaponMaxExp, CheckItemGet, CheckWeaponRot, GetItem.
        RecompiledFunction maxExp=nullptr,checkItemGet=nullptr,checkWeaponRot=nullptr,getItem=nullptr;
        constexpr uint32_t kMaxExp=0x232080u,kCheckItemGet=0x1BDD40u,kCheckWeaponRot=0x1BDF90u,kGetItem=0x1BE060u;
        constexpr uint32_t kReturnSentinel=0x00080000u; // not code: a routine's jr ra lands here
        std::atomic<bool> pendingLevel{false};
        std::atomic<uint32_t> pendingSpawn{0}; // id | count << 16
        std::mutex spawnMutex;
        std::vector<SpawnItem> spawnList;
        bool spawnListBuilt=false;
        // Combat: every player attack (melee, shots, magic, bombs, thrown items) reaches a monster's HP at one
        // instruction of CMonstorUnit::CheckDmg, 0x1DC3F4 subu v0,v0,s4 (v0 HP, s4 the final damage, s5 the unit).
        // The player is hit only where DUN's BtCheckDamageProc asks CheckHitUser (0x1B5920; returns to 0x1DBB0E0).
        constexpr uint32_t kMonsterHp=0x1DC3F4u,kCheckHitUser=0x1B5920u,kHitUserReturn=0x1DBB0E0u;
        RecompiledFunction checkHitUser=nullptr;
        // Key finder: SetKey gives a floor's key to one live monster (unit = gp-0x6320, 16 slots of 400 bytes: state
        // +0x1E3D0 (-1 none), HP +0x1E3F4, item +0x1E470 (s16, -1 none)). DUN MainDraw calls DrawMapSymbol (0x1D7AA0,
        // returning to 0x1DB0348) to put monster icons on the map; the marker is drawn after it.
        constexpr uint32_t kMapSymbols=0x1D7AA0u,kMapSymbolsReturn=0x1DB0348u,kColourRect=0x15E120u;
        RecompiledFunction mapSymbols=nullptr,colourRect=nullptr;
        uint32_t markerBlink=0;
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
        void levelUp(uint8_t *ram,uint32_t s,const R5900Context *ctx,Runtime *rt);
        void spawn(uint8_t *ram,uint32_t s,const R5900Context *ctx,Runtime *rt);
        void buildSpawnList(const uint8_t *ram);
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
        void editPlayer(uint8_t *ram,uint32_t s,const R5900Context *ctx=nullptr,Runtime *rt=nullptr)
        {
            const int size=pendingSize.exchange(-1,std::memory_order_relaxed);
            const int gold=pendingGilda.exchange(-1,std::memory_order_relaxed);
            if(s && size>=minimumBag(ram,s))write<int8_t>(ram,s+0x4360,static_cast<int8_t>(size));
            if(s && gold>=0)write<uint16_t>(ram,s+0x4346,static_cast<uint16_t>(gold));
            for(int i=0;i<3;++i){const auto request=pendingItems[i].exchange(0,std::memory_order_relaxed);const auto a=item(ram,s,i);
                if(request && a.id==int(request&65535) && a.maximum>0)
                    write<int16_t>(ram,s+0x4368+i*2,static_cast<int16_t>(std::clamp(int(request>>16),1,a.maximum)));}
            weaponAttack(ram,s);
            if(s){levelUp(ram,s,ctx,rt);spawn(ram,s,ctx,rt);buildSpawnList(ram);}
            publishPlayer(ram,s);
        }
        // Runs a compiled routine on a copy of the hook's context (its own frame below the current stack, its return to
        // a sentinel). -1 when it did not return there (a routine that blocks or switches threads is never called so).
        bool callGameDone(RecompiledFunction routine,uint8_t *ram,const R5900Context *ctx,Runtime *rt,
                          std::initializer_list<uint32_t> args,int32_t *result=nullptr)
        {
            if(!routine || !ctx || !rt)return false;
            R5900Context call=*ctx;
            int reg=4;
            for(const uint32_t a:args)call.r[reg++].ud[0]=uint64_t(int64_t(int32_t(a)));
            call.r[31].ud[0]=kReturnSentinel;
            call.leaving=false;
            routine(ram,&call,rt);
            if(call.pc!=kReturnSentinel || call.leaving)return false;
            if(result)*result=getRegS32(&call,2);
            return true;
        }
        int32_t callGame(RecompiledFunction routine,uint8_t *ram,const R5900Context *ctx,Runtime *rt,
                         std::initializer_list<uint32_t> args)
        {
            int32_t result=-1;
            return callGameDone(routine,ram,ctx,rt,args,&result)?result:-1;
        }
        // DC_PROBE_CHEATS=1: the one-shot edits that call game routines, as they are applied.
        bool probe(){static const bool on=std::getenv("DC_PROBE_CHEATS")!=nullptr;return on;}
        bool defaultWeapon(int id){return id==0x101 || id==0x12B || id==0x13A || id==0x14B || id==0x15B || id==0x16B;}
        void levelUp(uint8_t *ram,uint32_t s,const R5900Context *ctx,Runtime *rt)
        {
            if(!rt || !pendingLevel.exchange(false,std::memory_order_relaxed))return;
            const uint32_t w=equippedWeapon(ram,s);
            if(!w)return;
            const int id=read<int16_t>(ram,w),level=read<int16_t>(ram,w+2);
            // The game's own refusals: default weapons earn no ABS, the Serpent Sword waits for a story flag, 99 is
            // the last level.
            if(defaultWeapon(id) || id==0x10C || level>=99)return;
            const int32_t needed=callGame(maxExp,ram,ctx,rt,{w});
            const int before=read<int16_t>(ram,w+0x14);
            if(needed>0 && needed<=999 && before<needed)write<int16_t>(ram,w+0x14,int16_t(needed));
            if(probe())std::fprintf(stderr,"[dc:cheats] level-up: weapon 0x%x level %d ABS %d -> %d (needed %d)\n",id,level,before,
                                    read<int16_t>(ram,w+0x14),needed);
        }
        // The game's text: 16-bit codes (0xFD21.. 'A'.., 0xFD3B.. 'a'.., 0xFD6F.. '0'.., 0xFF02 space, 0xFF01 end).
        std::string gameText(const uint8_t *ram,int key)
        {
            const uint32_t buffer=read<uint32_t>(ram,0x2A2528u)&0x1FFFFFFu; // SystemMes, resident all session
            if(!buffer || buffer>0x1F00000u)return {};
            const int count=read<int16_t>(ram,buffer);
            if(count<=0 || count>4000)return {};
            for(int i=0;i<count;++i)
            {
                if(read<int16_t>(ram,buffer+4+4*uint32_t(i))!=key)continue;
                uint32_t at=buffer+2*uint32_t(count+read<int16_t>(ram,buffer+6+4*uint32_t(i))+1);
                std::string text;
                for(int n=0;n<40 && at<0x1FFFFFEu;++n,at+=2)
                {
                    const uint16_t c=read<uint16_t>(ram,at);
                    if(c==0xFF01)break;
                    if(c>=0xFD21 && c<=0xFD3A)text+=char('A'+c-0xFD21);
                    else if(c>=0xFD3B && c<=0xFD54)text+=char('a'+c-0xFD3B);
                    else if(c>=0xFD6F && c<=0xFD78)text+=char('0'+c-0xFD6F);
                    else if(c==0xFF02)text+=' ';
                    else if(c==0xFD55)text+='\'';
                    else if(c==0xFD5D)text+='-';
                    else if(c==0xFD6D)text+='.';
                    else if(c==0xFD5B)text+='&';
                    else if(c==0xFD61)text+='(';
                    else if(c==0xFD62)text+=')';
                    else return {}; // an argument or a mark: not a plain item name
                }
                return text;
            }
            return {};
        }
        // What the spawner offers: no event/key/story item (they satisfy doors and NPCs out of order), no default or
        // story weapon, no unused ID.
        bool spawnable(int id)
        {
            if(id>=0x84 && id<=0x100)
                return (id<=0x8D) || (id>=0x91 && id<=0x9B) || (id>=0x9F && id<=0xB7) || (id>=0xBA && id<=0xC1) ||
                       id==0xC5 || id==0xC7 || (id>=0xF5 && id<=0xF7);
            if(id>=0x51 && id<=0x83)
                return (id<=0x55) || (id>=0x5B && id<=0x6B) || (id>=0x6F && id<=0x78);
            if(id>=0x102 && id<=0x177)
            {
                constexpr int excluded[]={0x10B,0x10C,0x115,0x116,0x117,0x118,0x129,0x12A,0x12B,0x13A,0x146,0x14A,0x14B,
                                          0x156,0x15A,0x15B,0x166,0x169,0x16A,0x16B};
                return std::find(std::begin(excluded),std::end(excluded),id)==std::end(excluded);
            }
            return false;
        }
        void buildSpawnList(const uint8_t *ram)
        {
            if(spawnListBuilt)return;
            std::vector<SpawnItem> list;
            for(int id=0x51;id<=0x177;++id)
            {
                if(!spawnable(id))continue;
                // System message id + 100: a bag item's or an attachment's number is its id; a weapon's is ComItemInfo's
                // (8-byte entries indexed id - 0x51, the number at +6; 0 = unused).
                const int number=id<=0x100 ? id : read<int16_t>(ram,0x27DE50u+uint32_t(id-0x51)*8+6);
                if(number<=0)continue;
                const int key=number+100;
                const std::string name=gameText(ram,key);
                if(name.empty())continue;
                SpawnItem item;item.id=id;std::snprintf(item.name,sizeof(item.name),"%s",name.c_str());
                list.push_back(item);
            }
            if(list.empty())return; // the messages are not loaded yet
            std::lock_guard lock(spawnMutex);
            spawnList=std::move(list);spawnListBuilt=true;
        }
        void spawn(uint8_t *ram,uint32_t s,const R5900Context *ctx,Runtime *rt)
        {
            if(!rt)return;
            const uint32_t request=pendingSpawn.exchange(0,std::memory_order_relaxed);
            const int id=int(request&0xFFFF),count=std::clamp(int(request>>16),1,10);
            if(!request || !validStatus(s) || !spawnable(id))return;
            int given=0;
            for(int n=0;n<count;++n,++given)
            {
                if(id>=0x101)
                {
                    // A weapon goes to its owner, who must have joined (+5: party size), with room left in slots 0..9.
                    const int owner=read<int8_t>(ram,0x275A7Eu+uint32_t(id)*0x4C);
                    if(owner<0 || owner>=read<int8_t>(ram,s+5) || callGame(checkWeaponRot,ram,ctx,rt,{s,uint32_t(id)})>=9)break;
                }
                else if(callGame(checkItemGet,ram,ctx,rt,{s,uint32_t(id)})!=0)break; // bag or attachments full
                // a2 = 1: a stat sphere (0x5B..0x5E) gives its +1 bonus, as when one is found.
                if(callGame(getItem,ram,ctx,rt,{s,uint32_t(id),1u})<0)break;
            }
            if(probe())std::fprintf(stderr,"[dc:cheats] spawn: item 0x%x, %d of %d given\n",id,given,count);
        }
        void moveHook(uint8_t *ram,R5900Context *ctx,Runtime *rt)
        {
            const uint32_t caller=getRegU32(ctx,31);
            const bool town=caller==0x16A9C0u;
            const bool dungeon=(caller==0x1DB39F4u || caller==0x1DB3EA0u || caller==0x1DB5C10u) && rt->functions.generated(0x1DB7450u);
            if(town || dungeon){
                editPlayer(ram,playerStatus(ram,ctx),ctx,rt);
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
        void randCheckHook(uint8_t *r,R5900Context *c,Runtime *rt)
        {
            // A fish near the bait never decides to leave it: p = 0 for the two Leave rolls only. The call still draws
            // its rand(), so the shared sequence advances as it would; the Find/Eat/Bite odds (bait affinity) stay.
            const uint32_t from=getRegU32(c,31);
            if(enabled(Cheat::FishingHelper) && (from==kLeaveFood || from==kLeaveHook))c->fu[12]=0;
            randCheck(r,c,rt);
        }
        void fishStepHook(uint8_t *r,R5900Context *c,Runtime *rt)
        {
            // A hooked fish (state 8) stays hooked until the button is pressed: its timer never runs out.
            const int64_t fish=bozo::GuestMemory::flatOffset(getRegU32(c,4),0x60);
            if(enabled(Cheat::FishingHelper) && fish>=0 && fish<=bozo::kFlatScratchpadOffset-0x60 &&
               read<int32_t>(r,uint32_t(fish)+0x50)==8 && read<int32_t>(r,uint32_t(fish)+0x58)<1)
                write<int32_t>(r,uint32_t(fish)+0x58,1);
            fishStep(r,c,rt);
        }
        void revealFloor(uint8_t *r,uint32_t map)
        {
            if(map!=kMapMain && map!=kMapSub)return;
            // Every cell with floor, except column x = 19: SetUnderLoad fills it, but no floor reaches it.
            for(uint32_t i=0;i<400;++i)
                if(i%20!=19 && read<int32_t>(r,map+0x9C50+i*16)!=-1)write<int32_t>(r,map+0x8710+i*4,1);
            const int rooms=std::clamp(read<int32_t>(r,map+0xB650),0,16);
            for(int i=0;i<rooms;++i)
                if(read<int32_t>(r,map+0xB558+i*16)!=0)write<int32_t>(r,map+0x8+i*4,1);
        }
        void flushMaskHook(uint8_t *r,R5900Context *c,Runtime *rt)
        {
            // On arrival: after the floor build clears the grid (the three DUN floor-build call sites only).
            const uint32_t from=getRegU32(c,31),map=getRegU32(c,4);
            flushMask(r,c,rt);
            if(enabled(Cheat::RevealFloorMap) && !c->leaving && c->pc==from &&
               (from==0x1DB8A1Cu || from==0x1DB913Cu || from==0x1DB91FCu) && rt->functions.generated(kDunResident))
                revealFloor(r,map);
        }
        void checkMaskHook(uint8_t *r,R5900Context *c,Runtime *rt)
        {
            // Switched on mid-floor: at MoveChara's next walking update (it only ever adds explored cells).
            if(enabled(Cheat::RevealFloorMap) && getRegU32(c,31)==0x1DB159Cu && rt->functions.generated(kDunResident))
                revealFloor(r,getRegU32(c,4));
            checkMask(r,c,rt);
        }
        void checkHitUserHook(uint8_t *r,R5900Context *c,Runtime *rt)
        {
            // No damage taken: the hit test answers "no hit" (-1) to the player's hit routine, before its damage,
            // knock-down, status and theft effects.
            if(enabled(Cheat::NoDamageTaken) && getRegU32(c,31)==kHitUserReturn && rt->functions.generated(kDunResident))
            {setReturnS32(c,-1);c->pc=getRegU32(c,31);return;}
            checkHitUser(r,c,rt);
        }
        bool gateKey(int item){return item==0xC3 || item==0xC4 || item==0xC6 || (item>=0xC9 && item<=0xCE);}
        void mapSymbolsHook(uint8_t *r,R5900Context *c,Runtime *rt)
        {
            const uint32_t from=getRegU32(c,31);
            mapSymbols(r,c,rt);
            if(!enabled(Cheat::KeyFinder) || from!=kMapSymbolsReturn || c->pc!=from || !rt->functions.generated(kDunResident) ||
               !colourRect)return;
            const uint32_t gp=getRegU32(c,28),unit=read<uint32_t>(r,(gp-0x6320u)&0x1FFFFFFu)&0x1FFFFFFu;
            const uint32_t map=read<uint32_t>(r,(gp-0x6338u)&0x1FFFFFFu)&0x1FFFFFFu;
            if(!unit || unit>0x1F00000u)return;
            const int wanted=map && map<0x1F00000u ? read<int32_t>(r,map+0x464) : -1;
            int holder=-1;
            for(int i=0;i<16;++i){
                const uint32_t slot=unit+uint32_t(i)*400;
                const int item=read<int16_t>(r,slot+0x1E470);
                if(read<int32_t>(r,slot+0x1E3D0)==-1 || read<int32_t>(r,slot+0x1E3F4)<=0 || !gateKey(item))continue;
                if(holder<0 || item==wanted)holder=i; // dungeon 1 places three keys: the floor's own wins
            }
            if(holder<0)return;
            // Scratch guest memory below the stack, as the routine's own frame would use: position, rectangles.
            const uint32_t scratch=(getRegU32(c,29)-0x200u)&~15u;
            const uint32_t character=unit+uint32_t(holder)*0x3510+0x1FCD0u;
            // Its position through its own virtual GetPosition (vtable at +0xA0, slot +0xA0), as DrawMapSymbol asks.
            const uint32_t table=read<uint32_t>(r,(character+0xA0)&0x1FFFFFFu)&0x1FFFFFFu;
            const uint32_t getPosition=table?read<uint32_t>(r,(table+0xA0)&0x1FFFFFFu):0u;
            RecompiledFunction locate=rt->functions.override(getPosition);if(!locate)locate=rt->functions.generated(getPosition);
            if(!locate || !callGameDone(locate,r,c,rt,{character,scratch}))return;
            const float scale=read<float>(r,(gp-0x7F80u)&0x1FFFFFFu),x=read<float>(r,scratch),z=read<float>(r,scratch+8);
            if(!std::isfinite(x) || !std::isfinite(z) || !std::isfinite(scale))return;
            // A solid gold square, 10 x 10 around the holder's 8 x 8 icon position, blinking: drawn by the port's native
            // colour-rectangle routine (set2DSpriteC4), like the game's own map rectangles.
            const int32_t rect[4]={int32_t(scale*x)+384-1,int32_t(scale*z)+68-1,10,10};
            // spRGBA: the bytes R, G, B, A (255, 200, 32), alpha 0x80 = 1.0 then 0: blinking.
            const uint32_t gold=((++markerBlink/15)%2?0x80u:0x00u)<<24|0x0020C8FFu; // on and off: unlike the static exit tile
            std::memcpy(r+scratch+0x20,rect,16);
            for(uint32_t k=0;k<4;++k)write<uint32_t>(r,scratch+0x30+k*4,gold);
            R5900Context draw=*c;
            draw.r[4].ud[0]=0;draw.r[5].ud[0]=scratch+0x20;
            for(int k=0;k<4;++k)draw.r[6+k].ud[0]=scratch+0x30+uint32_t(k)*4;
            draw.r[31].ud[0]=kReturnSentinel;draw.pc=kColourRect;
            colourRect(r,&draw,rt);
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
            if(pc==kMonsterHp){
                // Scales the final damage just before the original subtraction; immunities (0 damage) stay. Capped at
                // the HP left (or the original damage, if higher), so the damage number stays the game's own range.
                const int64_t damage=getRegS32(c,20),hp=getRegS32(c,2);
                if(damage>0){
                    int64_t scaled=damage;
                    if(enabled(Cheat::OneHitKills))scaled=std::max(damage,hp);
                    else if(damageMultiplier()>1)scaled=std::min(damage*damageMultiplier(),std::max(damage,hp));
                    c->r[20].ud[0]=uint64_t(scaled);
                }
                return false; // execute the original subu v0,v0,s4
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
            if (!player || player==status) editPlayer(ram,player,ctx,runtime);
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
    int fastForwardPercent(){return fastRate.load(std::memory_order_relaxed);}
    void setFastForwardPercent(int v){fastRate.store(std::clamp(v,150,400),std::memory_order_relaxed);}
    void setFastForwardHeld(bool held){fastHeld.store(held,std::memory_order_relaxed);}
    int gameSpeedPercent(){return fastHeld.load(std::memory_order_relaxed) || enabled(Cheat::FastForwardLock) ? fastForwardPercent() : 100;}
    int damageMultiplier(){return damageRate.load(std::memory_order_relaxed);}
    void setDamageMultiplier(int v){damageRate.store(std::clamp(v,1,16),std::memory_order_relaxed);}
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
    void requestWeaponLevelUp(){pendingLevel.store(true,std::memory_order_relaxed);}
    int spawnItemCount(){std::lock_guard lock(spawnMutex);return int(spawnList.size());}
    SpawnItem spawnItem(int index){std::lock_guard lock(spawnMutex);return index>=0 && index<int(spawnList.size())?spawnList[size_t(index)]:SpawnItem{};}
    void requestSpawn(int id,int count){if(spawnable(id))pendingSpawn.store(uint32_t(id)|uint32_t(std::clamp(count,1,10))<<16,std::memory_order_relaxed);}
    void refreshPlayer(const uint8_t *r,const R5900Context *c){publishPlayer(r,playerStatus(r,c));buildSpawnList(r);}
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
        randCheck=runtime.functions.generated(kRandCheck);fishStep=runtime.functions.generated(kFishStep);
        if(randCheck)registry.registerFunction(kRandCheck,randCheckHook);
        if(fishStep)registry.registerFunction(kFishStep,fishStepHook);
        checkMask=runtime.functions.generated(kCheckMask);flushMask=runtime.functions.generated(kFlushMask);
        if(checkMask)registry.registerFunction(kCheckMask,checkMaskHook);
        if(flushMask)registry.registerFunction(kFlushMask,flushMaskHook);
        maxExp=runtime.functions.generated(kMaxExp);checkItemGet=runtime.functions.generated(kCheckItemGet);
        checkWeaponRot=runtime.functions.generated(kCheckWeaponRot);getItem=runtime.functions.generated(kGetItem);
        checkHitUser=runtime.functions.generated(kCheckHitUser);
        if(checkHitUser)registry.registerFunction(kCheckHitUser,checkHitUserHook);
        mapSymbols=registry.find(kMapSymbols);if(!mapSymbols)mapSymbols=runtime.functions.generated(kMapSymbols);
        colourRect=registry.find(kColourRect); // the port's native rectangle drawing (dc_native), installed first
        if(mapSymbols)registry.registerFunction(kMapSymbols,mapSymbolsHook);
        priorInstruction=registry.instructionHandler();registry.nativeGraphics(instruction);
    }
}
