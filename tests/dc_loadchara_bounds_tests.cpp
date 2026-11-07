// Actual generated LoadChara2 resume spans, complete GetPackFile/strcasecmp and
// child allocator, production memcpy. Explicit start/end boundaries; this does
// not execute character loading or provide gameplay coverage.
#include "dc_functions.h"
#include "dun_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <utility>
#include <vector>
namespace
{
constexpr uint32_t arena=0x1f06660,base=0x2ab080,capacity=0x33450,stack=0x80000,
    pack=0x100000,payload=pack+0x200,table=0x1dc2e70,gp=0x2a97f0;
int failures=0,cases=0,copies=0,allocations=0;bool rejected=false;
uint32_t observedRequest=0,observedDestination=0,observedSource=0,observedBytes=0;
std::vector<uint8_t> expectedCopy;
bozo::libs::Libraries libraries;
struct End {}; struct Reject {};
void check(bool ok,const char *s) { if(!ok) { ++failures;std::printf("FAIL LoadChara case %d: %s\n",cases,s); } }
void word(uint8_t *p,uint32_t a,uint32_t n) { std::memcpy(p+a,&n,4); }
uint32_t word(const uint8_t *p,uint32_t a) { uint32_t n;std::memcpy(&n,p+a,4);return n; }
void boundary(uint8_t *ram,R5900Context *c,Runtime *rt)
{
    if(c->pc==0x1033b0)
    {
        ++copies;observedDestination=c->r[4].uw[0];observedSource=c->r[5].uw[0];observedBytes=c->r[6].uw[0];
        const auto *to=rt->memory.hostPointer(observedDestination,observedBytes);
        const auto *from=rt->memory.hostPointer(observedSource,observedBytes);
        expectedCopy.assign(ram,ram+bozo::kFlatScratchpadOffset);
        if(observedBytes && to && from)
        {
            std::memmove(expectedCopy.data()+(to-ram),expectedCopy.data()+(from-ram),observedBytes);
        }
        bozo::libs::Guest g(*rt,c);libraries.find("memcpy")->handler(libraries,g);
        if(!expectedCopy.empty()) check(!std::memcmp(ram,expectedCopy.data(),expectedCopy.size()),"exact production copy before allocator");
        c->pc=c->r[31].uw[0];return;
    }
    if(c->pc==0x1278a0)
    {
        ++allocations;observedRequest=c->r[5].uw[0];
        check(c->r[4].uw[0]==arena,"actual child allocator object");
        const uint32_t link=c->r[31].uw[0],address=word(ram,arena)+(word(ram,arena+8)<<4);
        rt->functions.generated(0x1278a0)(ram,c,rt);
        check(c->pc==link && !c->leaving && c->r[2].uw[0]==address,"complete allocator returns old cursor address to actual caller link");
        throw End{};
    }
    if(c->pc==0x104698) { rejected=true;throw Reject{}; } // stop before diagnostic + infinite loop
    throw std::runtime_error("unexpected LoadChara boundary");
}
struct Host:bozo::ee::Host
{
    void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected library"); }
    void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected VU0"); }
    void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected I/O read"); }
    void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected I/O write"); }
    void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
};
struct Site {uint32_t entry,afterCopy,slot,name;const char *text;};
constexpr Site sites[]={{0x1dba6cc,0x1dba744,0x1d8,0x1dc3188,"c05w_h.img"},
                        {0x1dba7a0,0x1dba7e8,0x1d8,0x1dc3198,"mgan01.chr"},
                        {0x1dba814,0x1dba868,0x1dc,0x1dc31a8,"c05_f03.chr"}};
}
int main()
{
    Host host;bozo::ee::Dispatcher dispatcher(host);auto &rt=dispatcher.runtime();auto *ram=rt.memory.ram();
    rt.functions.setGenerated(dc_function_table,dc_function_count);rt.functions.setResume(dc_resume_table,dc_resume_count);
    rt.functions.addModule(0x1dabd00,0x1f06b6c,ram+0x1dabd00,{0xaa},dun_function_table,dun_function_count,dun_resume_table,dun_resume_count);
    for(auto pc:{0x1033b0u,0x1278a0u,0x104698u}) rt.functions.setOverride(pc,boundary);
    auto run=[&](const Site &s,uint32_t used,uint32_t size,uint32_t wantRequest,bool wantReject,
                 bool arithmeticOnly=false,bool missing=false,uint32_t arenaBase=base)
    {
        ++cases;copies=allocations=0;rejected=false;observedRequest=0xdeadbeef;expectedCopy.clear();
        std::memset(ram,0x53,bozo::kFlatScratchpadOffset);ram[0x1dabd00]=0xaa;
        for(unsigned c='A';c<='Z';++c) ram[0x2967b1+c]=1;
        std::strcpy(reinterpret_cast<char *>(ram+s.name),s.text);
        std::strcpy(reinterpret_cast<char *>(ram+pack),missing?"missing":s.text);
        word(ram,pack+0x40,0x200);word(ram,pack+0x44,size);word(ram,pack+0x48,0x80);ram[pack+0x80]=0;
        word(ram,arena,arenaBase);word(ram,arena+8,used);word(ram,arena+12,capacity);
        word(ram,0x1c74980,0x11223344);
        word(ram,stack+s.slot,size);word(ram,table,0x12345678);
        R5900Context c;c.pc=arithmeticOnly?s.afterCopy:s.entry;c.r[19].ud[0]=pack;c.r[21].ud[0]=s.slot==0x1dc?3:5;
        c.r[28].ud[0]=gp;c.r[29].ud[0]=stack;c.r[31].ud[0]=0x123450;
        bool invalidCopy=false;
        try {rt.functions.resumeEntry(c.pc)->resume(ram,&c,&rt);check(false,"span stops at declared allocator boundary");}
        catch(const End &) {} catch(const Reject &) {}
        catch(const bozo::libs::BadAccess &b)
        {
            check(!arithmeticOnly && size==0xffffffffu && b.bytes==size,"only declared invalid native range faults");
            invalidCopy=true;
        }
        if(invalidCopy)
        {
            check(copies==1 && allocations==0,"invalid full native range rejected before allocation");
            check(!std::memcmp(ram,expectedCopy.data(),expectedCopy.size()),"invalid native range leaves all pre-copy RAM unchanged");
            check(word(ram,table)==0x12345678,"invalid range does not partially write table");return;
        }
        check(copies==(arithmeticOnly?0:1) && allocations==1,"copy precedes child check; arithmetic-only explicitly skips copy");
        check(observedRequest==wantRequest,"actual signed instructions give expected reservation");
        check(rejected==wantReject,"complete allocator signed comparison outcome");
        if(!wantReject) check(word(ram,arena+8)==used+wantRequest,"complete child allocator cursor store");
        else check(word(ram,arena+8)==used,"rejected allocator leaves cursor unchanged");
        if(!arithmeticOnly)
        {
            check(observedBytes==size && observedDestination==arenaBase+(used<<4),"pack size and wrapped destination reach native copy");
            check(observedSource==(missing?0:payload),"missing lookup passes null guest source without a caller check");
            if(observedDestination==table && size==4) check(word(ram,table)==0x53535353,"forged cursor overwrites synthetic DUN table bytes");
            if(size==table-base+1) check(ram[table]==0x53 && ram[table+1]==0x56,"oversize copy touches table before allocator rejection");
            if(arenaBase==0x1c74980-capacity*16-16 && size==32)
                check(word(ram,0x1c74980)==0x53535353 && word(ram,table)==0x12345678,"child overflow may corrupt global cursor while dispatch bytes stay disjoint");
        }
    };
    for(const auto &s:sites)
    {
        for(auto n:{0u,63u,64u,132224u,25360u,53552u})
            run(s,0,n,n==0||n==63?4:n==64?8:n==132224?8268:n==25360?1588:3348,false);
        run(s,capacity-4,64,8,true);run(s,capacity-4,65,8,true);
        run(s,capacity,0,4,true);run(s,0,32,4,false,false,true);
        if(s.entry==0x1dba7a0) run(s,0,8268,520,false,false,true); // explicitly seeded preceding reservation, as bytes
        for(uint32_t mirror:{0x20000000u,0x30000000u,0x80000000u,0xa0000000u}) run(s,0,64,8,false,false,false,base|mirror);
        run(s,capacity,32,4,true,false,false,0x1c74980-capacity*16-16);
        run(s,(table-base)/16,4,4,true); // invalid logical cursor, mapped destination
        run(s,0,0xffffffffu,0,false); // production mapped-range rejection
        run(s,0,table-base+1,((table-base+1)/64+1)*4,true);
        for(const auto pair: {std::pair{0x7fffffbfu,0x07fffffcu},std::pair{0x7fffffc0u,0xf8000000u},
                              std::pair{0x80000000u,0xf8000004u},std::pair{0xffffffbfu,0xfffffffcu},
                              std::pair{0xffffffc0u,0u},std::pair{0xffffffffu,0u}})
            run(s,0,pair.first,pair.second,pair.first==0x7fffffbf,true);
        run(s,0x7ffffffc,64,8,false,true); // addition wraps negative and passes
        run(s,0xffffffff,32,4,false); // negative cursor accepted, writes before base
    }
    std::printf("LoadChara2 copy/cursor boundaries: %d cases; %d failures\n",cases,failures);return failures?1:0;
}
