// Real compiled resource routines with synthetic player/inventory state.
// Disabled hooks must preserve original execution; enabled hooks must prevent
// lethal/depletion effects before the game's break/death checks.
#include "../runtime/dc_cheats.h"
#include "dc_functions.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
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
{r.find(c.pc)(rt.memory.ram(),&c,&rt);check(c.pc==0x123450 && !c.leaving,"resource hook returns");}
}
int main()
{
    using namespace dc::cheats;
    auto rt=std::make_unique<Runtime>();rt->functions.setGenerated(dc_function_table,dc_function_count);
    bozo::port::Registry registry;install(*rt,registry);auto *r=rt->memory.ram();
    for(const auto &entry:registry.functions())rt->functions.setOverride(entry.address,entry.function);
    check(bozo::port::undeclared(registry,dc_external_table,dc_external_count).empty(),"all cheat hooks are declared externals");
    for(auto cheat:{Cheat::InfiniteHealth,Cheat::InfiniteWeaponDurability,Cheat::InfiniteThirst})check(!enabled(cheat),"cheats default off");
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
    std::printf("Native cheats: %d failures\n",failures);return failures?1:0;
}
