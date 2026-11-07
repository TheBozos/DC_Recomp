// Production memcpy/memset handlers and complete compiled memmove, synthetic
// objects only. Raw copies may replace vptr bytes that typed assignment preserves.
#include "dc_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include "bozo/ops_mem.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
constexpr uint32_t src=0x420000,dst=0x440000,extent=0xa200;
int failures=0,cases=0;
void check(bool ok,const char *what) { if(!ok) { ++failures;std::printf("FAIL bulk case %d: %s\n",cases,what); } }
struct Host:bozo::ee::Host
{
    void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected bulk library"); }
    void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected bulk VU0"); }
    void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected bulk I/O read"); }
    void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected bulk I/O write"); }
    void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
};
}
int main()
{
    Host host;bozo::ee::Dispatcher dispatcher(host);auto &rt=dispatcher.runtime();auto *ram=rt.memory.ram();
    bozo::libs::Libraries libraries;rt.functions.setGenerated(dc_function_table,dc_function_count);
    auto run=[&](unsigned api,uint32_t to,uint32_t from,uint32_t bytes)
    {
        ++cases;std::memset(ram,0x53,bozo::kFlatScratchpadOffset);
        for(unsigned i=0;i<extent+16;++i) { ram[src-8+i]=uint8_t(i*13+17);ram[dst-8+i]=uint8_t(i*7+91); }
        auto expected=std::vector<uint8_t>(ram,ram+bozo::kFlatScratchpadOffset);
        const uint32_t destination=bozo::physicalAddress(to),source=bozo::physicalAddress(from);
        if(bytes)
        {
            if(api==2) std::memset(expected.data()+destination,0x7b,bytes);
            else std::memmove(expected.data()+destination,expected.data()+source,bytes);
        }
        R5900Context c;c.r[4].sd[0]=int32_t(to);c.r[5].sd[0]=int32_t(api==2?0x17b:from);
        c.r[6].ud[0]=bytes;c.r[29].ud[0]=0x700000;c.r[31].ud[0]=0x123450;
        if(api==1)
        {
            auto fn=rt.functions.generated(0x103460);if(!fn) throw std::runtime_error("missing compiled memmove");
            fn(ram,&c,&rt);check(c.pc==0x123450 && !c.leaving,"complete memmove returns normally");
        }
        else
        {
            auto function=libraries.find(api==0?"memcpy":"memset");
            if(!function) throw std::runtime_error("missing production bulk binding");
            bozo::libs::Guest guest(rt,&c);function->handler(libraries,guest);
        }
        check(c.r[2].uw[0]==to,"bulk return retains destination alias including mirror bits");
        check(!std::memcmp(ram,expected.data(),expected.size()),"exact byte range and copied/fill content, including vptr bytes and guards");
    };
    for(unsigned api=0;api<3;++api)
    {
        for(uint32_t field:{0xa0u,0x2b8u,0xb0u,0x1260u,0x8e30u})
        {
            run(api,dst+field,src+field,0);
            run(api,dst+field-1,src+field-1,1); // stops just before the vptr
            run(api,dst+field-1,src+field-1,2); // replaces its first byte
            run(api,dst+field,src+field,4);
            run(api,dst+field-1,src+field-1,6);
        }
        run(api,dst,src,0xa160); // complete CSHOT_EFFECT-sized raw copy/fill
        for(uint32_t mirror:{0x20000000u,0x80000000u,0xa0000000u}) run(api,(dst+0xb0)|mirror,(src+0xb0)|mirror,4);
        run(api,0x12000000,0x12001000,0); // zero length never dereferences invalid pointers
    }
    for(uint32_t bytes:{33u,0xa160u})
    {
        run(1,src+3,src,bytes);
        run(1,src,src+3,bytes);
    }
    const int bulkCases=cases;
    // Complete GetPackFile + its real compiled strcasecmp helper. Synthetic
    // ctype flags/pack nodes demonstrate the out-size contract, not asset validity.
    constexpr uint32_t pack=0x480000,out=0x490000,name=0x490100;
    auto word=[&](uint32_t at,uint32_t n) { std::memcpy(ram+at,&n,4); };
    for(uint32_t size:{0u,4u,0xffffffffu}) for(unsigned lookup=0;lookup<4;++lookup) for(bool optional:{false,true})
    {
        ++cases;std::memset(ram,0,bozo::kFlatScratchpadOffset);
        for(unsigned c='A';c<='Z';++c) ram[0x2967b1+c]=1;
        std::strcpy(reinterpret_cast<char *>(ram+pack),"sword");
        std::strcpy(reinterpret_cast<char *>(ram+pack+0x80),"chara");
        for(unsigned offset:{0u,0x80u}) { word(pack+offset+0x40,0x200+offset);word(pack+offset+0x44,size);word(pack+offset+0x48,0x80); }
        std::strcpy(reinterpret_cast<char *>(ram+name),lookup==0?"Models/SWORD":lookup==1?"Models/chara":"Models/missing");
        word(out,0xdeadbeef);
        auto expected=std::vector<uint8_t>(ram,ram+bozo::kFlatScratchpadOffset);
        if(lookup<2 && !optional) std::memcpy(expected.data()+out,&size,4);
        R5900Context c;
        c.r[4].ud[0]=lookup==3?0:pack;c.r[5].ud[0]=name;c.r[6].ud[0]=optional?0:out;
        c.r[29].ud[0]=0x700000;c.r[31].ud[0]=0x123450;
        auto fn=rt.functions.generated(0x13f720);if(!fn) throw std::runtime_error("missing GetPackFile");
        fn(ram,&c,&rt);
        check(c.pc==0x123450 && !c.leaving,"complete GetPackFile returns normally");
        check(c.r[2].uw[0]==(lookup<2?pack+(lookup?0x300:0x200):0),"pack lookup returns node-relative payload or null");
        std::memcpy(expected.data()+0x6ff000,ram+0x6ff000,0x1000);
        check(!std::memcmp(ram,expected.data(),expected.size()),"out-size written only on match; missing/null retain sentinel, optional output untouched");
    }
    std::printf("Bulk dispatch effects: %d cases; complete GetPackFile/strcasecmp: %d cases; %d failures\n",bulkCases,cases-bulkCases,failures);
    return failures?1:0;
}
