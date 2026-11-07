// Controlled coverage of exact DUN dispatch spans and the whole EquipReAttach
// owner. Case landing bodies/callees are synthetic; this is not gameplay coverage.
#include "dun_dispatch_functions.h"
#include "dispatch_cases.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/guest.h"
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <vector>
namespace
{
constexpr uint32_t gp=0x300000, stack=0x700000, actor=0x420000, table=0x430000,
    otherTable=0x431000, target1=0x310000, target2=0x310010, parent=0x480000, found=0x481000;
int failures=0, cases=0, virtuals=0, reloads=0, lookups=0, attaches=0, decoys=0;
bool equip=false, swapTable=false; uint32_t activeActor=actor, activePc=0, selected=target1, equipBranch=0;
const CallCase *callCase=nullptr;
void check(bool ok,const char *what) { if(!ok) { ++failures;std::printf("FAIL DUN case %d: %s\n",cases,what); } }
void word(uint8_t *ram,uint32_t at,uint32_t v) { std::memcpy(ram+at,&v,4); }
uint32_t word(const uint8_t *ram,uint32_t at) { uint32_t v;std::memcpy(&v,ram+at,4);return v; }
void boundary(uint8_t *ram,R5900Context *ctx,Runtime *rt)
{
    bozo::libs::Guest g(*rt,ctx);
    if(ctx->pc==0x133070)
    {
        ++reloads;check(g.arg(0)==0x1c75870 && g.arg(1)==0 && g.arg(2)==29,"MainDraw reload consumes manager/group with null handle");
    }
    else if(ctx->pc==0x128700)
    {
        ++lookups;
        check(virtuals==0 && attaches==0,"lookup precedes both equipment methods");
        check(g.arg(0)==(equipBranch?0x490000u:0x491000u) && g.arg(1)==(equipBranch?0x1dc30d8u:0x1dc30e0u),"actual lookup branch inputs retained");
        g.retU(found);ctx->pc=ctx->r[31].uw[0];return;
    }
    else if(ctx->pc==0x128180)
    {
        check(virtuals==2 && lookups==1,"attachment follows both equipment methods");
        ++attaches;check(g.arg(0)==parent && g.arg(1)==found,"actual parent attachment consumes lookup result");
        g.ret(0x5634);ctx->pc=ctx->r[31].uw[0];return;
    }
    else if(ctx->pc==target1 || ctx->pc==target2)
    {
        ++virtuals;
        if(equip)
        {
            check(lookups==1 && attaches==0,"equipment methods follow lookup and precede attachment");
            check(g.arg(0)==actor,"equipment object retained across callbacks");
            check(ctx->r[31].uw[0]==(virtuals==1?0x1db960cu:0x1db9630u),"both real equipment JALR link PCs");
            check(ctx->f[12]==0 && ctx->f[13]==0 && ctx->f[14]==0,"float arguments cleared before each equipment method");
            check(ctx->pc==(virtuals==2 && swapTable?target2:target1),"second equipment call reloads the current table");
            ctx->f[12]=13;ctx->f[13]=17;ctx->f[14]=19;
            if(virtuals==1 && swapTable) word(ram,actor+0xa0,otherTable);
        }
        else if(callCase)
        {
            check(g.arg(0)==activeActor && ctx->pc==selected,"object/table/slot selects current callback");
            check(ctx->r[31].uw[0]==activePc+8,"actual virtual-call link and delay slot retained");
            if(activePc==0x1dae898) check(reloads==1,"reload precedes MainDraw callback");
            if(callCase->field==0x2b8) check(g.arg(1)==1,"MoveChara scalar argument is retained");
        }
        else check(ctx->r[31].uw[0]==0x123450,"mutable switch fallback is a tail transfer");
    }
    else throw std::runtime_error("unexpected dispatch boundary");
    g.ret(0xe001);ctx->pc=ctx->r[31].uw[0];
}
void decoy(uint8_t *,R5900Context *ctx,Runtime *) { ++decoys;ctx->r[2].ud[0]=0xabcd;ctx->pc=ctx->r[31].uw[0]; }
struct Host:bozo::ee::Host
{
    void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
    void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected VU0"); }
    void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected I/O read"); }
    void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected I/O write"); }
    void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
};
R5900Context context()
{
    R5900Context c;
    for(int r=1;r<32;++r) { c.r[r].ud[0]=0x1234567800000000ull+r;c.r[r].ud[1]=0xa5a55a5a00000000ull+r; }
    c.r[4].ud[0]=actor;c.r[5].ud[0]=0x12000000;c.r[28].ud[0]=gp;c.r[29].ud[0]=stack;c.r[31].ud[0]=0x123450;
    c.f[12]=3;c.f[13]=5;c.f[14]=7;return c;
}
void invoke(Runtime &rt,R5900Context &ctx,uint32_t entry,bool span=true)
{
    const auto before=ctx;
    // The production dispatcher unwinds register jumps. Drive its generated
    // resume entries here with a bounded synchronous harness (no scheduler run).
    auto fn=rt.functions.generated(entry);
    if(!fn) throw std::runtime_error("missing selected fixture entry");
    bool returned=false;
    for(unsigned transfers=0;transfers<8 && !returned;++transfers)
    {
        try { fn(rt.memory.ram(),&ctx,&rt);returned=true; }
        catch(const bozo::ee::StopGuest &)
        {
            if(auto callback=rt.functions.override(ctx.pc)) fn=callback;
            else if(auto resume=rt.functions.resumeEntry(ctx.pc)) fn=resume->resume;
            else throw std::runtime_error("unrecognized fixture resume target");
        }
    }
    check(returned,"bounded dispatch completes");
    check(ctx.pc==0x123450 && !ctx.leaving,"dispatch returns normally");
    for(int r:{16,17,18,19,20,21,22,23,28,29,30,31})
        if(!(span && r==23)) check(!std::memcmp(&ctx.r[r],&before.r[r],sizeof(ctx.r[r])),"saved full registers preserved");
}
void load(uint8_t *ram,const char *path)
{
    std::ifstream in(path,std::ios::binary);const std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)),{});
    auto u=[&](size_t p) { uint32_t v;if(p+4>b.size()) throw std::runtime_error("bad fixture ELF");std::memcpy(&v,b.data()+p,4);return v; };
    if(u(0)!=0x464c457f) throw std::runtime_error("missing fixture ELF");
    for(unsigned i=0;i<(u(44)&0xffff);++i) { const auto p=u(28)+i*32;if(u(p)!=1) continue;
        const auto offset=u(p+4),at=u(p+8),size=u(p+16);
        if(uint64_t(at)+size>bozo::kFlatScratchpadOffset || uint64_t(offset)+size>b.size()) throw std::runtime_error("bad fixture segment");
        std::memcpy(ram+at,b.data()+offset,size); }
}
}
int main(int argc,char **argv)
{
    if(argc!=2) return 2;
    Host host;bozo::ee::Dispatcher dispatcher(host);auto &rt=dispatcher.runtime();auto *ram=rt.memory.ram();load(ram,argv[1]);
    std::vector<BozoFunctionEntry> wrong;
    for(size_t i=0;i<dun_dispatch_function_count;++i) wrong.push_back({dun_dispatch_function_table[i].address,decoy});
    auto *identity=ram+0x1dabd00;
    rt.functions.addModule(0x1dabd00,0x1f00000,identity,{0x55},wrong.data(),wrong.size(),nullptr,0);
    rt.functions.addModule(0x1dabd00,0x1f00000,identity,{0xaa},dun_dispatch_function_table,dun_dispatch_function_count,dun_dispatch_resume_table,dun_dispatch_resume_count);
    *identity=0xaa;
    for(auto pc:{0x128700u,0x128180u,0x133070u,target1,target2}) rt.functions.setOverride(pc,boundary);
    word(ram,gp-0x742c,0x12000000);
    for(const auto &s:switches)
    {
        std::vector<uint32_t> indices;for(uint32_t i=0;i<s.count;++i) indices.push_back(i);
        indices.insert(indices.end(),{s.count,0xffffffffu,0x80000000u});
        for(uint32_t i:indices)
        {
            ++cases;auto c=context();const uint32_t input=i+s.bias;c.r[s.reg].sd[0]=int32_t(input);
            word(ram,gp-0x625c,input);word(ram,0x1d564a4,input);
            const std::vector<uint8_t> before(ram,ram+bozo::kFlatScratchpadOffset);
            invoke(rt,c,s.entry);check(c.r[2].uw[0]==(i<s.count?s.markers[i]:s.defaultMarker),"every switch index/alias/default lands correctly");
            check(!std::memcmp(ram,before.data(),before.size()),"switch dispatch leaves RAM unchanged");
        }
        for(uint32_t replacement:{s.targets[s.count-1],target1})
        {
            ++cases;word(ram,s.table,replacement);auto c=context();c.r[s.reg].sd[0]=s.bias;
            word(ram,gp-0x625c,s.bias);word(ram,0x1d564a4,s.bias);virtuals=0;
            invoke(rt,c,s.entry);check(c.r[2].uw[0]==(replacement==target1?0xe001u:s.markers[s.count-1]),"mutable switch table and dynamic jump dispatch remain real");
            check(virtuals==(replacement==target1?1:0),"only dynamic fallback calls the controlled resident target");
        }
        word(ram,s.table,s.targets[0]);
    }
    for(const auto &s:calls) for(unsigned variant=0;variant<2;++variant) for(unsigned mode=0;mode<(s.conditional?3u:1u);++mode)
    {
        ++cases;callCase=&s;activeActor=actor+variant*0x1000;activePc=s.pc;selected=variant?target2:target1;
        word(ram,activeActor+s.field,table);std::memset(ram+table,0x12,0x400);word(ram,table+s.slot,selected);
        word(ram,gp+uint32_t(int16_t(s.gpSlot)),activeActor);virtuals=reloads=0;
        auto c=context();c.r[16].ud[0]=mode;
        const std::vector<uint8_t> before(ram,ram+bozo::kFlatScratchpadOffset);
        invoke(rt,c,s.entry);check(virtuals==(!s.conditional || mode==1?1:0),"actual motion conditional gates callback");
        check(reloads==(s.pc==0x1dae898?1:0),"MainDraw reload remains before virtual draw");
        check(!std::memcmp(ram,before.data(),before.size()),"call setup and controlled callback leave RAM unchanged");
    }
    callCase=nullptr;equip=true;
    for(equipBranch=0;equipBranch<2;++equipBranch) for(unsigned mutate=0;mutate<2;++mutate)
    {
        ++cases;swapTable=mutate;word(ram,actor+0xa0,table);word(ram,actor+0xbc,parent);
        word(ram,table+0x18,target1);word(ram,table+0x30,target1);word(ram,otherTable+0x30,target2);
        word(ram,0x1ea2f8c,0x490000);word(ram,0x1ea1ddc,0x491000);virtuals=lookups=attaches=0;
        auto c=context();c.r[5].ud[0]=equipBranch;
        std::vector<uint8_t> expected(ram,ram+bozo::kFlatScratchpadOffset);
        if(swapTable) word(expected.data(),actor+0xa0,otherTable);
        invoke(rt,c,0x1db9590,false);
        check(virtuals==2 && lookups==1 && attaches==1 && c.r[2].uw[0]==0x5634,"complete equipment owner retains call order/result");
        check(!std::memcmp(ram,expected.data(),stack-0x1000) && !std::memcmp(ram+stack,expected.data()+stack,expected.size()-stack),"equipment changes only controlled callback state outside stack");
    }
    equip=false;*identity=0x55;auto c=context();invoke(rt,c,switches[0].entry);
    check(decoys==1 && c.r[2].uw[0]==0xabcd,"same address dispatches to selected other image");
    *identity=0xff;
    check(!rt.functions.generated(switches[0].entry) && !rt.functions.resumeEntry(switches[0].entry),"unknown image cannot select DUN entry or resume table");
    std::printf("DUN remaining dispatch: %d controlled cases, 10 sites (not live coverage), %d failures\n",cases,failures);
    return failures?1:0;
}
