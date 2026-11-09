// The level-up, map, fishing, spawner and fast-forward cheats. Each wrapped original is a recording stub, so the hooks'
// own decisions are checked exactly: what they pass on, what they write, and what they refuse.
#include "../runtime/dc_cheats.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <bit>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
namespace {
constexpr uint32_t user=0x400000,gp=0x300000,stack=0x700000,link=0x123450;
constexpr uint32_t mapMain=0x01DC4BE0,mapSub=0x01DD56F0,fish=0x500000,messages=0x600000;
int failures=0;
void check(bool ok,const char *what){if(!ok){++failures;std::printf("FAIL %s\n",what);}}
template<class T> T read(uint8_t *r,uint32_t p){T n;std::memcpy(&n,r+p,sizeof(n));return n;}
template<class T> void write(uint8_t *r,uint32_t p,T n){std::memcpy(r+p,&n,sizeof(n));}
void ret(R5900Context *c,int32_t v){c->r[2].ud[0]=uint64_t(int64_t(v));c->pc=c->r[31].uw[0];}

// The recording stubs.
float seenProbability=-1;int stepCalls=0,maxExpCalls=0,itemChecks=0,weaponChecks=0,gives=0,checkResult=0,rotResult=0;
std::vector<int> given;
void randCheck(uint8_t *,R5900Context *c,Runtime *){seenProbability=std::bit_cast<float>(c->fu[12]);ret(c,0);}
void fishStep(uint8_t *,R5900Context *c,Runtime *){++stepCalls;ret(c,0);}
void checkMask(uint8_t *,R5900Context *c,Runtime *){ret(c,0);}
void flushMask(uint8_t *r,R5900Context *c,Runtime *){
    const uint32_t m=c->r[4].uw[0];for(uint32_t i=0;i<400;++i)write<int32_t>(r,m+0x8710+i*4,0);ret(c,0);}
void maxExp(uint8_t *,R5900Context *c,Runtime *){++maxExpCalls;ret(c,40);}
void checkItemGet(uint8_t *,R5900Context *c,Runtime *){++itemChecks;ret(c,checkResult);}
void checkWeaponRot(uint8_t *,R5900Context *c,Runtime *){++weaponChecks;ret(c,rotResult);}
void getItem(uint8_t *,R5900Context *c,Runtime *){++gives;given.push_back(c->r[5].sw[0]);ret(c,0);}
void plain(uint8_t *,R5900Context *c,Runtime *){ret(c,0);}
int hitTests=0,symbolCalls=0,sprites=0;int32_t spriteRect[4]={};uint32_t spriteColour[4]={};
void checkHitUser(uint8_t *,R5900Context *c,Runtime *){++hitTests;ret(c,3);}
void mapSymbols(uint8_t *,R5900Context *c,Runtime *){++symbolCalls;ret(c,0);}
void getPosition(uint8_t *r,R5900Context *c,Runtime *){ // (x, y, z) = (1000, 0, -200)
    const uint32_t out=c->r[5].uw[0];write<float>(r,out,1000);write<float>(r,out+4,0);write<float>(r,out+8,-200);ret(c,0);}
void colourRect(uint8_t *r,R5900Context *c,Runtime *){
    ++sprites;std::memcpy(spriteRect,r+c->r[5].uw[0],16);
    for(int i=0;i<4;++i)spriteColour[i]=read<uint32_t>(r,c->r[6+i].uw[0]);c->pc=c->r[31].uw[0];}

R5900Context at(uint32_t pc,uint32_t a0,uint32_t from=link)
{R5900Context c;c.pc=pc;c.r[4].uw[0]=a0;c.r[28].uw[0]=gp;c.r[29].uw[0]=stack;c.r[31].uw[0]=from;return c;}
void call(Runtime &rt,bozo::port::Registry &reg,R5900Context &c)
{const uint32_t back=c.r[31].uw[0];reg.find(c.pc)(rt.memory.ram(),&c,&rt);check(c.pc==back && !c.leaving,"hook returns to its caller");}

// The game's message table: n, then (key, offset) pairs, then the 16-bit text.
void messageTable(uint8_t *r,const std::vector<std::pair<int,std::string>> &entries)
{
    const int n=int(entries.size());write<int16_t>(r,messages,int16_t(n));
    int16_t offset=int16_t(n+1);std::vector<uint16_t> text; // text starts at u16 index 2n + 2 = n + (n + 1) + 1
    for(int i=0;i<n;++i){
        write<int16_t>(r,messages+4+4*i,int16_t(entries[size_t(i)].first));write<int16_t>(r,messages+6+4*i,offset);
        for(char ch:entries[size_t(i)].second)
            text.push_back(ch==' '?0xFF02:ch>='a'?uint16_t(0xFD3B+ch-'a'):ch>='A'?uint16_t(0xFD21+ch-'A'):uint16_t(0xFD6F+ch-'0'));
        text.push_back(0xFF01);offset=int16_t(n+1+int(text.size()));
    }
    for(size_t i=0;i<text.size();++i)write<uint16_t>(r,messages+2*uint32_t(2*n+2)+2*uint32_t(i),text[i]);
    write<uint32_t>(r,0x2A2528,messages);
}
}
int main()
{
    using namespace dc::cheats;
    // Sorted by address: the resource routines install() requires, then the wrapped originals.
    const BozoFunctionEntry table[]={
        {0x1B5920,checkHitUser},
        {0x1B5D90,plain},{0x1BDD40,checkItemGet},{0x1BDF90,checkWeaponRot},{0x1BE060,getItem},{0x1BE510,plain},
        {0x1BE710,plain},{0x1BE900,plain},{0x1BEA50,plain},{0x1C39C0,checkMask},{0x1C3C00,flushMask},{0x1D7AA0,mapSymbols},
        {0x200000,getPosition},{0x232080,maxExp},{0x240050,randCheck},{0x240480,fishStep}};
    const BozoFunctionEntry withDun[]={{0x1B5920,checkHitUser},
        {0x1B5D90,plain},{0x1BDD40,checkItemGet},{0x1BDF90,checkWeaponRot},{0x1BE060,getItem},{0x1BE510,plain},
        {0x1BE710,plain},{0x1BE900,plain},{0x1BEA50,plain},{0x1C39C0,checkMask},{0x1C3C00,flushMask},{0x1D7AA0,mapSymbols},
        {0x200000,getPosition},{0x232080,maxExp},{0x240050,randCheck},{0x240480,fishStep}};
    auto rt=std::make_unique<Runtime>();rt->functions.setGenerated(table,std::size(table));
    bozo::port::Registry reg;install(*rt,reg);auto *r=rt->memory.ram();

    // Fishing: the Leave rolls only, and only when on.
    auto roll=[&](uint32_t from){auto c=at(0x240050,fish,from);c.fu[12]=std::bit_cast<uint32_t>(0.1f);call(*rt,reg,c);return seenProbability;};
    check(roll(0x2403FC)==0.1f && roll(0x24044C)==0.1f,"fishing off: the Leave rolls keep their odds");
    setEnabled(Cheat::FishingHelper,true);
    check(roll(0x2403FC)==0 && roll(0x24044C)==0,"fishing on: a fish near the bait never leaves it");
    check(roll(0x24022C)==0.1f && roll(0x2402F0)==0.1f && roll(0x2403B0)==0.1f,"fishing on: Find/Eat/Bite odds unchanged");
    write<int32_t>(r,fish+0x50,8);write<int32_t>(r,fish+0x58,0);
    {auto c=at(0x240480,fish);call(*rt,reg,c);}
    check(read<int32_t>(r,fish+0x58)==1 && read<int32_t>(r,fish+0x50)==8 && stepCalls==1,"a hooked fish keeps its timer; Step still runs");
    write<int32_t>(r,fish+0x50,7);write<int32_t>(r,fish+0x58,0);
    {auto c=at(0x240480,fish);call(*rt,reg,c);}
    check(read<int32_t>(r,fish+0x58)==0,"a nibbling fish's timer is not touched");
    setEnabled(Cheat::FishingHelper,false);
    write<int32_t>(r,fish+0x50,8);write<int32_t>(r,fish+0x58,0);
    {auto c=at(0x240480,fish);call(*rt,reg,c);}
    check(read<int32_t>(r,fish+0x58)==0,"fishing off: the hooked fish's timer runs out as before");

    // Map: floor cells (part != -1) except column 19, two rooms.
    auto layout=[&](uint32_t m){
        for(uint32_t i=0;i<400;++i){write<int32_t>(r,m+0x9C50+i*16,(i%3==0 || i%20==19)?2:-1);write<int32_t>(r,m+0x8710+i*4,0);}
        for(uint32_t i=0;i<16;++i){write<int32_t>(r,m+8+i*4,0);write<int32_t>(r,m+0xB558+i*16,i<2?1:0);}
        write<int32_t>(r,m+0xB650,3);};
    auto revealed=[&](uint32_t m){
        for(uint32_t i=0;i<400;++i){const bool want=i%20!=19 && i%3==0;if((read<int32_t>(r,m+0x8710+i*4)==1)!=want)return false;}
        return read<int32_t>(r,m+8)==1 && read<int32_t>(r,m+12)==1 && read<int32_t>(r,m+16)==0;};
    layout(mapMain);setEnabled(Cheat::RevealFloorMap,true);
    {auto c=at(0x1C39C0,mapMain,0x1DB159C);call(*rt,reg,c);}
    check(read<int32_t>(r,mapMain+0x8710)==0,"no reveal while the DUN overlay is not resident");
    auto dun=std::make_unique<Runtime>();dun->functions.setGenerated(withDun,std::size(withDun));
    bozo::port::Registry dunReg;dunReg.registerFunction(0x15E120,colourRect);install(*dun,dunReg);auto *d=dun->memory.ram();
    // The DUN overlay resident: its module registered with identity bytes in place, as the game loads it.
    const std::vector<uint8_t> identity{0x44,0x55,0x4E,0x21};std::memcpy(d+0x1DABD00,identity.data(),identity.size());
    const BozoFunctionEntry dunModule[]={{0x1DB7450,plain}};
    dun->functions.addModule(0x1DABD00,0x1F00000,d+0x1DABD00,identity,dunModule,1,nullptr,0);
    std::swap(r,d);std::swap(rt,dun);
    layout(mapMain);{auto c=at(0x1C39C0,mapMain,0x1DB159C);call(*rt,dunReg,c);}
    check(revealed(mapMain),"walking update: every floor cell explored, column 19 and empty cells left alone");
    layout(mapMain);{auto c=at(0x1C39C0,mapMain,0x1DB1000);call(*rt,dunReg,c);}
    check(read<int32_t>(r,mapMain+0x8710)==0,"another caller of checkMask is not revealed");
    layout(mapSub);{auto c=at(0x1C3C00,mapSub,0x1DB913C);call(*rt,dunReg,c);}
    check(revealed(mapSub),"floor build: the sub-map is revealed after FlushCheckMask clears it");
    layout(0x500000);{auto c=at(0x1C39C0,0x500000,0x1DB159C);call(*rt,dunReg,c);}
    check(read<int32_t>(r,0x500000+0x8710)==0,"only the two real map objects are written");
    setEnabled(Cheat::RevealFloorMap,false);layout(mapMain);{auto c=at(0x1C3C00,mapMain,0x1DB8A1C);call(*rt,dunReg,c);}
    check(read<int32_t>(r,mapMain+0x8710)==0,"map off: the floor stays unexplored");

    // The player (gp-0x6388) with character 0's equipped weapon in slot 0.
    std::memset(r+user,0,0x8C00);write<uint32_t>(r,gp-0x6388,user);write<int8_t>(r,user+5,2);
    const uint32_t weapon=user+0x450C;write<int16_t>(r,weapon,0x103);write<int16_t>(r,weapon+2,3);write<int16_t>(r,weapon+0x14,7);
    auto tick=[&]{auto c=at(0x1BEA50,user);call(*rt,dunReg,c);};
    requestWeaponLevelUp();tick();
    check(read<int16_t>(r,weapon+0x14)==40 && maxExpCalls==1,"level-up: ABS filled to the game's own threshold");
    write<int16_t>(r,weapon+0x14,7);tick();check(read<int16_t>(r,weapon+0x14)==7,"level-up request is used once");
    write<int16_t>(r,weapon,0x101);requestWeaponLevelUp();tick();check(read<int16_t>(r,weapon+0x14)==7,"default weapon refused");
    write<int16_t>(r,weapon,0x10C);requestWeaponLevelUp();tick();check(read<int16_t>(r,weapon+0x14)==7,"Serpent Sword refused");
    write<int16_t>(r,weapon,0x103);write<int16_t>(r,weapon+2,99);requestWeaponLevelUp();tick();
    check(read<int16_t>(r,weapon+0x14)==7,"level 99 refused");

    // Spawner: names from the message table; bag items and attachments by id + 100, weapons through ComItemInfo
    // (indexed id - 0x51).
    write<int16_t>(r,0x27DE50+(0x103-0x51)*8+6,3);
    messageTable(r,{{0x91+100,"Bread"},{0xC3+100,"Drans Crest"},{0x5B+100,"Attack"},{3+100,"Baselard"}});
    tick();
    bool bread=false,crest=false,sphere=false,blade=false;
    for(int i=0;i<spawnItemCount();++i){const auto it=spawnItem(i);const std::string n=it.name;
        bread|=it.id==0x91 && n=="Bread";crest|=it.id==0xC3;sphere|=it.id==0x5B && n=="Attack";blade|=it.id==0x103 && n=="Baselard";}
    check(bread && sphere && blade && spawnItemCount()==3,"spawner lists named items with their game names");
    check(!crest,"spawner never offers a key or story item");
    requestSpawn(0x91,3);tick();check(gives==3 && given[0]==0x91 && itemChecks==3,"three units given through GetItem");
    checkResult=1;gives=0;requestSpawn(0x91,3);tick();check(gives==0,"a full bag refuses without entering the overflow path");
    checkResult=0;gives=0;requestSpawn(0xC3,1);tick();check(gives==0,"a key item request is ignored");
    write<int8_t>(r,0x275A7E + 0x103*0x4C,1);rotResult=3;gives=0;requestSpawn(0x103,1);tick();
    check(gives==1 && weaponChecks==1,"a weapon goes to its joined owner");
    write<int8_t>(r,0x275A7E + 0x103*0x4C,4);gives=0;requestSpawn(0x103,1);tick();check(gives==0,"a weapon for a character not yet joined is refused");
    write<int8_t>(r,0x275A7E + 0x103*0x4C,1);rotResult=9;gives=0;requestSpawn(0x103,1);tick();check(gives==0,"a full weapon list is refused");

    // Damage dealt: the instruction before the game's own subtraction (v0 HP, s4 damage).
    auto hit=[&](int damage,int hp){R5900Context c;c.r[20].ud[0]=uint64_t(int64_t(damage));c.r[2].ud[0]=uint64_t(int64_t(hp));
        const bool handled=dunReg.instructionHandler()(r,&c,rt.get(),0x1DC3F4);check(!handled,"the original subtraction still runs");
        return int(c.r[20].sw[0]);};
    check(hit(30,100)==30,"damage unchanged at 1x");
    setDamageMultiplier(2);check(hit(30,100)==60,"2x doubles the damage");
    setDamageMultiplier(16);check(hit(30,100)==100,"scaled damage is capped at the HP left");
    check(hit(300,100)==300,"a hit already above the HP keeps its own damage");
    check(hit(0,100)==0,"an immune hit (0 damage) stays immune");
    setDamageMultiplier(1);setEnabled(Cheat::OneHitKills,true);check(hit(1,250)==250 && hit(0,250)==0,"one-hit kills, immunities kept");
    setEnabled(Cheat::OneHitKills,false);

    // Damage taken: only BtCheckDamageProc's hit test, only in the dungeon.
    auto test=[&](uint32_t from){auto c=at(0x1B5920,0x480000,from);call(*rt,dunReg,c);return c.r[2].sw[0];};
    check(test(0x1DBB0E0)==3 && hitTests==1,"no-damage off: the hit test runs");
    setEnabled(Cheat::NoDamageTaken,true);
    check(test(0x1DBB0E0)==-1 && hitTests==1,"no-damage on: the player's hit test answers no hit");
    check(test(0x1DBC000)==3 && hitTests==2,"other hit tests are untouched");
    setEnabled(Cheat::NoDamageTaken,false);

    // Key finder: unit at 0x800000 with slot 5 holding the floor's key (0xC9) and slot 2 another key (0xC4).
    const uint32_t unit=0x800000,floorMap=0x480000;
    std::memset(r+unit,0,0x80000);write<uint32_t>(r,gp-0x6320,unit);write<uint32_t>(r,gp-0x6338,floorMap);
    write<int32_t>(r,floorMap+0x464,0xC9);write<float>(r,gp-0x7F80,0.1f);
    for(uint32_t i=0;i<16;++i){write<int32_t>(r,unit+i*400+0x1E3D0,1);write<int32_t>(r,unit+i*400+0x1E3F4,10);write<int16_t>(r,unit+i*400+0x1E470,-1);}
    write<int16_t>(r,unit+5*400+0x1E470,0xC9);write<int16_t>(r,unit+2*400+0x1E470,0xC4);
    for(uint32_t i=0;i<16;++i){const uint32_t character=unit+i*0x3510+0x1FCD0;write<uint32_t>(r,character+0xA0,0x460000);}
    write<uint32_t>(r,0x460000+0xA0,0x200000); // the vtable's GetPosition
    auto symbols=[&]{auto c=at(0x1D7AA0,unit,0x1DB0348);call(*rt,dunReg,c);};
    symbols();check(sprites==0 && symbolCalls==1,"key finder off: only the game's icons");
    setEnabled(Cheat::KeyFinder,true);symbols();
    check(sprites==1 && spriteRect[0]==100+384-1 && spriteRect[1]==-20+68-1 && spriteRect[2]==10 && spriteRect[3]==10,
          "the marker sits on the holder at the game's map transform");
    check((spriteColour[0]&0xFFFFFF)==0x20C8FF && spriteColour[0]==spriteColour[3],"a gold square");
    write<int32_t>(r,unit+5*400+0x1E3F4,0);symbols();check(sprites==2,"the other key holder is marked once the floor's is gone");
    write<int32_t>(r,unit+2*400+0x1E3D0,-1);symbols();check(sprites==2,"no live holder: no marker");
    setEnabled(Cheat::KeyFinder,false);

    // Fast-forward.
    check(gameSpeedPercent()==100,"normal speed by default");
    setFastForwardHeld(true);check(gameSpeedPercent()==300,"held: the fast-forward speed (300% by default)");
    setFastForwardPercent(1000);check(gameSpeedPercent()==400,"fast-forward speed capped at 400%");
    setFastForwardHeld(false);check(gameSpeedPercent()==100,"released: normal speed");
    setEnabled(Cheat::FastForwardLock,true);check(gameSpeedPercent()==400,"locked on without holding");
    setEnabled(Cheat::FastForwardLock,false);setFastForwardPercent(300);
    std::printf("Extra cheats: %d failures\n",failures);return failures?1:0;
}
