// Real compiled resource routines with synthetic player/inventory state.
// Disabled hooks must preserve original execution; enabled hooks must prevent
// lethal/depletion effects before the game's break/death checks.
#include "../runtime/dc_cheats.h"
#include "dc_functions.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include "bozo/libs/libraries.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
namespace {
constexpr uint32_t user=0x400000,gp=0x300000,stack=0x700000;
int failures=0;
void check(bool ok,const char *what){if(!ok){++failures;std::printf("FAIL %s\n",what);}}
template<class T> T read(uint8_t *r,uint32_t p){T n;std::memcpy(&n,r+p,sizeof(n));return n;}
template<class T> void write(uint8_t *r,uint32_t p,T n){std::memcpy(r+p,&n,sizeof(n));}
void setup(uint8_t *r)
{
    std::memset(r+user,0,0x8c00);
    for(unsigned i=0;i<6;++i)
    {
        write<int16_t>(r,user+6+i*2,100+i*10);write<int16_t>(r,user+0x12+i*2,30+i);
        write<float>(r,user+0x42ec+i*4,300+i*10);write<float>(r,user+0x4304+i*4,10+i);
        write<int8_t>(r,user+0x4340+i,0);const auto w=user+0x450c+i*0xaa8;
        write<int16_t>(r,w,0x150+i);write<int16_t>(r,w+0xc,48+i);write<float>(r,w+0x10,40+i);
    }
    write<uint32_t>(r,gp-0x6388,user);write<uint32_t>(r,gp-0x62fc,user+0x450c);
    write<float>(r,gp-0x7bec,.01f);write<float>(r,gp-0x7f80,0);
    write<float>(r,gp-0x7d64,0);write<float>(r,gp-0x7d4c,1);write<float>(r,gp-0x7d48,.1f);
}
R5900Context context(uint32_t pc,unsigned character=0,int amount=-50,float animation=0)
{
    R5900Context c;c.pc=pc;c.r[4].uw[0]=user;c.r[5].uw[0]=character;c.r[6].sw[0]=amount;
    c.r[28].uw[0]=gp;c.r[29].uw[0]=stack;c.r[31].uw[0]=0x123450;c.fu[12]=std::bit_cast<uint32_t>(animation);return c;
}
void invoke(Runtime &rt,bozo::port::Registry &r,R5900Context &c)
{const auto link=c.r[31].uw[0];r.find(c.pc)(rt.memory.ram(),&c,&rt);check(c.pc==link && !c.leaving,"resource hook returns");}
bozo::port::Registry *activePorts=nullptr;
struct CheatHost:bozo::port::Host {
    bozo::libs::Libraries libs;
    bozo::libs::Libraries &libraries() override{return libs;}
    void frameEnd(Runtime *,R5900Context *) override{}
    bool callLibrary(Runtime *,R5900Context *,uint32_t) override{return false;}
    bool nativeInstruction(uint8_t *r,R5900Context *c,Runtime *rt,uint32_t pc) override{return activePorts->instructionHandler()(r,c,rt,pc);}
};
float seenTime=0,seenMove[3]={};int movementCalls=0;
void timeBoundary(uint8_t *,R5900Context *c,Runtime *){seenTime=std::bit_cast<float>(c->fu[12]);c->pc=c->r[31].uw[0];}
void movementBoundary(uint8_t *r,R5900Context *c,Runtime *){++movementCalls;std::memcpy(seenMove,r+c->r[5].uw[0],12);c->pc=c->r[31].uw[0];}
void dungeonMarker(uint8_t *,R5900Context *c,Runtime *){c->pc=c->r[31].uw[0];}
}
int main()
{
    using namespace dc::cheats;
    auto rt=std::make_unique<Runtime>();rt->functions.setGenerated(dc_function_table,dc_function_count);
    bozo::port::Registry registry;install(*rt,registry);auto *r=rt->memory.ram();
    for(const auto &entry:registry.functions())rt->functions.setOverride(entry.address,entry.function);
    check(bozo::port::undeclared(registry,dc_external_table,dc_external_count).empty(),"all cheat hooks are declared externals");
    for(auto cheat:{Cheat::InfiniteHealth,Cheat::InfiniteWeaponDurability,Cheat::InfiniteThirst,Cheat::InfiniteStamina})check(!enabled(cheat),"cheats default off");
    for(uint32_t pc:{0x1be710u,0x1be900u,0x1be510u,0x1bea50u,0x1b5d90u})
    {
        setup(r);auto initial=context(pc,0,pc==0x1be900?7:-5);
        if(pc==0x1b5d90){initial.r[4].uw[0]=0;initial.fu[12]=std::bit_cast<uint32_t>(1.0f);}
        auto original=initial;
        rt->functions.generated(pc)(r,&original,rt.get());
        std::vector<uint8_t> expected(r+user,r+user+0x8c00);
        setup(r);auto hooked=initial;
        invoke(*rt,registry,hooked);
        check(!std::memcmp(expected.data(),r+user,expected.size()),"disabled state matches the original compiled routine");
        check(!std::memcmp(&original,&hooked,sizeof(original)),"disabled CPU state matches original");
    }
    setEnabled(Cheat::InfiniteHealth,true);
    for(unsigned character=0;character<6;++character)for(uint32_t pc:{0x1be710u,0x1be900u})for(float animation:{0.0f,10.0f})
    {
        setup(r);write<int16_t>(r,user+0x8a5c+character*2,0);write<int16_t>(r,user+0x8a68+character*2,-100);
        auto c=context(pc,character,-30000,animation);invoke(*rt,registry,c);
        check(read<int16_t>(r,user+0x12+character*2)==100+character*10,"lethal or scheduled HP loss leaves full health");
        check(read<int16_t>(r,user+0x8a68+character*2)==0,"pending HP drain cancelled");
        check(read<float>(r,user+0x4304+character*4)==10+character,"health cheat leaves thirst alone");
    }
    setEnabled(Cheat::InfiniteHealth,false);setup(r);auto c=context(0x1be710,0,-30000);invoke(*rt,registry,c);
    check(read<int16_t>(r,user+0x12)==0,"health loss resumes after disabling");
    setEnabled(Cheat::InfiniteThirst,true);
    for(unsigned character=0;character<6;++character)for(float animation:{0.0f,10.0f})
    {
        setup(r);write<int16_t>(r,user+0x8ae2+character*2,-100);write<float>(r,user+0x8af0+character*4,.99f);
        c=context(0x1be510,character,-30000,animation);invoke(*rt,registry,c);
        check(read<float>(r,user+0x4304+character*4)==300+character*10,"thirst gauge remains full");
        check(read<int16_t>(r,user+0x8ae2+character*2)==0 && read<float>(r,user+0x8af0+character*4)==0,"pending drink loss and dehydration timer cancelled");
        check(read<int16_t>(r,user+0x12+character*2)==30+character,"thirst cheat leaves HP alone");
    }
    setup(r);write<float>(r,user+0x4304,0);write<float>(r,user+0x8af0,.99f);c=context(0x1bea50);c.r[5].uw[0]=0;invoke(*rt,registry,c);
    check(read<float>(r,user+0x4304)==300 && read<int16_t>(r,user+0x12)==30,"status tick prevents dehydration without enabling health cheat");
    setEnabled(Cheat::InfiniteThirst,false);setup(r);c=context(0x1be510,0,-5);invoke(*rt,registry,c);
    check(read<float>(r,user+0x4304)==5,"drink loss resumes after disabling");
    setEnabled(Cheat::InfiniteWeaponDurability,true);
    setup(r);write<float>(r,user+0x451c,.01f);write<int16_t>(r,user+0x450c+0xee,0x100);
    c=context(0x1b5d90);c.r[4].uw[0]=1000;c.fu[12]=std::bit_cast<uint32_t>(10000.0f);invoke(*rt,registry,c);
    check(read<float>(r,user+0x451c)==48,"lethal weapon wear refills WHP before break logic");
    check(read<int16_t>(r,user+0x450c)==0x150 && c.r[2].sw[0]==0,"weapon is retained and reports no break/equip action");
    for(unsigned character=0;character<6;++character)check(read<float>(r,user+0x451c+character*0xaa8)==48+character,"each equipped weapon receives its own maximum");
    setup(r);constexpr uint32_t switched=user+0x450c+3*0xf8;
    write<int8_t>(r,user+0x4340,3);write<int16_t>(r,switched,0x175);write<int16_t>(r,switched+0xc,99);write<float>(r,switched+0x10,2);
    c=context(0x1b5d90);c.r[4].uw[0]=0;invoke(*rt,registry,c);
    check(read<float>(r,switched+0x10)==99 && read<float>(r,user+0x451c)==40,"switching weapons follows the equipped slot and preserves the previous weapon");
    setup(r);write<int8_t>(r,user+0x4340,-1);write<float>(r,user+0x451c,7);c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(read<float>(r,user+0x451c)==7,"empty equipped slot is untouched");
    setEnabled(Cheat::InfiniteWeaponDurability,false);setup(r);c=context(0x1b5d90);c.r[4].uw[0]=0;c.fu[12]=std::bit_cast<uint32_t>(1.0f);invoke(*rt,registry,c);
    check(read<float>(r,user+0x451c)<40,"weapon wear resumes after disabling");
    // An edit targets only the active character's equipped slot, once.
    setup(r);write<int8_t>(r,user+4,2);write<int8_t>(r,user+0x4342,3);
    constexpr uint32_t edited=user+0x450c+2*0xaa8+3*0xf8;
    write<int16_t>(r,edited,0x175);write<int16_t>(r,edited+4,17);
    write<int16_t>(r,edited+6,28);write<int16_t>(r,edited+8,39);write<int16_t>(r,edited+10,50);
    const std::vector<uint8_t> inventory(r+user+0x450c,r+user+0x8500);
    requestWeaponAttack(777);c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(read<int16_t>(r,edited+4)==777 && equippedWeaponAttack()==777,"ATK edit follows the active character and selected weapon slot");
    auto expectedInventory=inventory;
    const int16_t attack=777;std::memcpy(expectedInventory.data()+edited+4-(user+0x450c),&attack,2);
    check(!std::memcmp(r+user+0x450c,expectedInventory.data(),expectedInventory.size()),"ATK edit preserves all other weapon stats, slots and attachments");
    write<int16_t>(r,edited+4,18);c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(read<int16_t>(r,edited+4)==18,"ATK is a one-time edit rather than a permanent per-frame overwrite");
    for(int value:{-123,2000})
    {requestWeaponAttack(value);c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);check(read<int16_t>(r,edited+4)==(value<0?0:999),"ATK input stays within 0..999");}
    setup(r);write<int8_t>(r,user+0x4340,-1);requestWeaponAttack(500);
    c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(equippedWeaponAttack()==-1 && read<int16_t>(r,user+0x4510)==0,"no weapon means no edit");
    write<int8_t>(r,user+0x4340,0);c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(read<int16_t>(r,user+0x4510)==0,"a discarded edit cannot unexpectedly affect a later-equipped weapon");
    setup(r);constexpr uint32_t preview=user+0x10000;
    std::memcpy(r+preview,r+user,0x8c00);requestWeaponAttack(321);
    c=context(0x1bea50);c.r[4].uw[0]=preview;c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(read<int16_t>(r,user+0x4510)==0 && read<int16_t>(r,preview+0x4510)==0,"weapon-menu preview cannot consume or receive the queued ATK edit");
    c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(read<int16_t>(r,user+0x4510)==321,"ATK edit applies to the real player when gameplay resumes");

    constexpr uint32_t gauge=0x1dc44c8,exhausted=0x1dc4524,resident=0x1dabd00;
    setEnabled(Cheat::InfiniteStamina,true);setup(r);write<float>(r,gauge,12);write<int32_t>(r,exhausted,1);
    c=context(0x1bea50);c.r[5].uw[0]=1;c.r[31].uw[0]=0x1db829c;invoke(*rt,registry,c);
    check(read<float>(r,gauge)==12,"no active dungeon module means no overlay writes");
    const std::vector<uint8_t> identity{0x41,0x42,0x43,0x44};std::memcpy(r+resident,identity.data(),identity.size());
    const BozoFunctionEntry dungeon[]={{0x1db7450,dungeonMarker}};
    rt->functions.addModule(resident,0x1f00000,r+resident,identity,dungeon,1,nullptr,0);
    c=context(0x1bea50);c.r[5].uw[0]=1;invoke(*rt,registry,c);
    check(read<float>(r,gauge)==12,"menu status calls cannot write dungeon stamina");
    c=context(0x1bea50);c.r[5].uw[0]=1;c.r[31].uw[0]=0x1db829c;invoke(*rt,registry,c);
    check(read<float>(r,gauge)==100 && read<int32_t>(r,exhausted)==0,"dungeon combo gauge stays full and clears exhaustion");
    r[resident]^=1;write<float>(r,gauge,19);
    c=context(0x1bea50);c.r[5].uw[0]=1;c.r[31].uw[0]=0x1db829c;invoke(*rt,registry,c);
    check(read<float>(r,gauge)==19,"changed overlay identity blocks stamina writes");
    r[resident]^=1;setEnabled(Cheat::InfiniteStamina,false);write<float>(r,gauge,75);
    c=context(0x1bea50);c.r[5].uw[0]=1;c.r[31].uw[0]=0x1db829c;invoke(*rt,registry,c);
    check(read<float>(r,gauge)==75,"disabling stamina preserves normal attack depletion");
    rt->functions.clearModules();
    // Snapshot and one-shot edits also work in town, without ticking Step.
    setup(r);for(int i=0;i<103;++i)write<int16_t>(r,user+0x436eu+i*2,-1);
    for(int i=0;i<3;++i){write<int16_t>(r,user+0x4362+i*2,-1);write<int16_t>(r,user+0x4368+i*2,0);}
    write<int8_t>(r,user+0x4360,50);write<uint16_t>(r,user+0x4346,321);
    write<int16_t>(r,user+0x4362,135);write<int16_t>(r,user+0x4368,4);
    write<int16_t>(r,user+0x4364,136);write<int16_t>(r,user+0x436a,2);
    write<int16_t>(r,user+0x436e,140);write<int16_t>(r,user+0x436eu+9*2,100);
    c=context(0);refreshPlayer(r,&c);
    check(gilda()==321 && inventorySize()==50 && minimumInventorySize()==10,"town snapshots expose Gilda, capacity and occupied bounds");
    check(activeConsumable(0).quantity==4 && activeConsumable(0).maximum==47,"quantity limit accounts for bag consumables and other active quantities");
    check(activeConsumable(2).id<0,"empty active slots cannot spawn an item");
    write<uint32_t>(r,gp-0x6388,0);write<uint32_t>(r,gp-0x72e4,user-0x540c);refreshPlayer(r,&c);
    check(equippedWeaponAttack()==read<int16_t>(r,user+0x4510) && gilda()==321,"town resolves the embedded save status without DUN's cached pointer");
    write<uint32_t>(r,gp-0x6388,user);write<uint32_t>(r,gp-0x72e4,0);
    const std::vector<uint8_t> beforeEdit(r+user,r+user+0x8c00);
    requestGilda(999999);requestInventorySize(5);requestConsumableQuantity(0,100);requestWeaponAttack(123);
    applyPlayerEdits(r,&c);
    check(gilda()==65535 && read<uint16_t>(r,user+0x4346)==65535,"Gilda uses original unsigned 16-bit limit");
    check(inventorySize()==50,"unsafe shrink cannot hide occupied entries");
    check(read<int16_t>(r,user+0x4368)==47 && read<int16_t>(r,user+0x436a)==2,"only selected consumable changes and bag capacity is respected");
    auto expected=beforeEdit;auto expect=[&](uint32_t at,auto value){std::memcpy(expected.data()+at-user,&value,sizeof(value));};
    expect(user+0x4346,uint16_t(65535));expect(user+0x4368,int16_t(47));expect(user+0x4510,int16_t(123));
    check(!std::memcmp(expected.data(),r+user,expected.size()),"edits preserve all unrelated inventory and player bytes");
    requestInventorySize(1000);applyPlayerEdits(r,&c);check(inventorySize()==100,"bag expansion is capped at original 100-entry supported capacity");
    requestConsumableQuantity(0,25);write<int16_t>(r,user+0x4362,137);applyPlayerEdits(r,&c);
    check(read<int16_t>(r,user+0x4368)==47,"changed active item identity rejects stale quantity request");
    requestGilda(-1);applyPlayerEdits(r,&c);check(gilda()==0,"Gilda can be set to zero");
    write<uint16_t>(r,user+0x4346,18);applyPlayerEdits(r,&c);check(gilda()==18,"Gilda is edited once, not held every frame");
    // Run the actual monster reward tail, including the game's original cap.
    // Suppress drops via its normal no-drop state and use the last monster slot.
    auto cheatHost=std::make_unique<CheatHost>();activePorts=&registry;rt->port=cheatHost.get();
    constexpr uint32_t monster=0x800000;
    for(int rate:{1,2,16})for(int reward:{0,5,30000}){
        setup(r);std::memset(r+monster,0,0x56000);write<int16_t>(r,monster+0x1e410,2);
        write<int16_t>(r,user+0x450c+0x14,20);setAbsMultiplier(rate);
        c=context(0x1df2a0);c.r[7].sw[0]=reward;c.r[16].uw[0]=15;c.r[17].uw[0]=0;c.r[18].uw[0]=99;c.r[19].uw[0]=user+0x450c;c.r[21].uw[0]=monster;
        write<uint32_t>(r,stack+0x80,0x123450);
        dc_resume_0x1dd540(r,&c,rt.get());
        check(c.pc==0x123450 && read<int16_t>(r,user+0x450c+0x14)==std::min(99,20+reward*rate),"real monster ABS reward retains original cap with positive multiplier");
    }
    setAbsMultiplier(16);setup(r);write<int16_t>(r,user+0x450c+0x14,20);write<int32_t>(r,user+0x8b10,10);
    c=context(0x1df260);c.r[7].sw[0]=5;c.r[16].uw[0]=15;c.r[17].uw[0]=0;c.r[18].uw[0]=99;c.r[19].uw[0]=user+0x450c;c.r[21].uw[0]=monster;write<uint32_t>(r,stack+0x80,0x123450);
    dc_resume_0x1dd540(r,&c,rt.get());check(read<int16_t>(r,user+0x450c+0x14)==15,"reverse-ABS floor penalty is not multiplied");
    setAbsMultiplier(1);rt->port=nullptr;
    // Observe both time inputs and the displacement entering collision.
    const BozoFunctionEntry extra[]={{0x14a680,movementBoundary},{0x187f90,timeBoundary},{0x188d50,timeBoundary},
        {0x1b5d90,dungeonMarker},{0x1be510,dungeonMarker},{0x1be710,dungeonMarker},{0x1be900,dungeonMarker},{0x1bea50,dungeonMarker}};
    rt->functions.setGenerated(extra,8);bozo::port::Registry controls;install(*rt,controls);
    setMovementPercent(200);setup(r);const float displacement[]={2,-3,4};std::memcpy(r+0x600000,displacement,12);
    c=context(0x14a680);c.r[5].uw[0]=0x600000;c.r[31].uw[0]=0x16a9c0;invoke(*rt,controls,c);
    check(movementCalls==1 && seenMove[0]==4 && seenMove[1]==-3 && seenMove[2]==8,"town speed scales horizontal input before the collision routine, retaining gravity");
    std::memcpy(r+0x600000,displacement,12);c=context(0x14a680);c.r[5].uw[0]=0x600000;invoke(*rt,controls,c);
    check(seenMove[0]==2 && seenMove[2]==4,"unrelated NPC/cutscene collision calls are not accelerated");
    setMovementPercent(100);std::memcpy(r+0x600000,displacement,12);c=context(0x14a680);c.r[5].uw[0]=0x600000;c.r[31].uw[0]=0x16a9c0;invoke(*rt,controls,c);
    check(!std::memcmp(seenMove,displacement,12),"default speed preserves original displacement exactly");
    setEnabled(Cheat::FreezeTimeOfDay,true);
    for(auto [pc,time]:{std::pair{0x188d50u,3.f},std::pair{0x187f90u,8.f},std::pair{0x188d50u,11.f}}){c=context(pc,0,0,time);invoke(*rt,controls,c);check(seenTime==3,"sky and lighting share one held time without stopping game timers");}
    setEnabled(Cheat::FreezeTimeOfDay,false);c=context(0x187f90,0,0,9);invoke(*rt,controls,c);check(seenTime==9,"disabling freeze resumes current time input");
    setEnabled(Cheat::FreezeTimeOfDay,true);c=context(0x187f90,0,0,10);invoke(*rt,controls,c);check(seenTime==10,"re-enabling freezes the new current time");
    setEnabled(Cheat::FreezeTimeOfDay,false);
    std::printf("Native cheats: %d failures\n",failures);return failures?1:0;
}
