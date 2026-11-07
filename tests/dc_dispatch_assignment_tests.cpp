// Real complete assignment owners, synthetic objects with inaccessible vptrs.
// No simulated callees: CCharacter calls the actual CObject assignment helper.
#include "dc_functions.h"
#include "bozo/ee/dispatcher.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>

namespace
{
constexpr uint32_t source=0x420000,destination=0x424000,extent=0x1200,stack=0x700000;
int failures=0,cases=0;
void check(bool ok,const char *what)
{
    if(!ok) { ++failures;std::printf("FAIL assignment case %d: %s\n",cases,what); }
}
void word(uint8_t *ram,uint32_t at,uint32_t v) { std::memcpy(ram+at,&v,4); }
struct Host:bozo::ee::Host
{
    void libraryCall(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected assignment library call"); }
    void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override { throw std::runtime_error("unexpected assignment VU0"); }
    void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override { throw std::runtime_error("unexpected assignment I/O read"); }
    void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override { throw std::runtime_error("unexpected assignment I/O write"); }
    void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &s) override { throw std::runtime_error(s); }
};
}
int main()
{
    Host host;bozo::ee::Dispatcher dispatcher(host);auto &rt=dispatcher.runtime();auto *ram=rt.memory.ram();
    rt.functions.setGenerated(dc_function_table,dc_function_count);
    for(uint32_t owner:{0x142da0u,0x1431e0u,0x124aa0u})
        for(unsigned variant=0;variant<4;++variant) for(bool self:{false,true})
        {
            ++cases;
            for(unsigned at=0;at<extent;at+=4)
            {
                word(ram,source+at,0x3f000000u+variant*0x10000u+at);
                word(ram,destination+at,0x40000000u+variant*0x10000u+at);
            }
            const uint32_t dst=self?source:destination,field=owner==0x124aa0?0x2b8:0xa0;
            word(ram,source+field,0x12001000u+variant*0x100);
            if(!self) word(ram,dst+field,0x12008000u+variant*0x100);
            const std::vector<uint8_t> before(ram,ram+bozo::kFlatScratchpadOffset);
            R5900Context c;
            for(int r=1;r<32;++r) { c.r[r].ud[0]=0x1234567800000000ull+r;c.r[r].ud[1]=0xa5a55a5a00000000ull+r; }
            c.r[4].ud[0]=dst;c.r[5].ud[0]=source;c.r[28].ud[0]=0x2a97f0;c.r[29].ud[0]=stack;c.r[31].ud[0]=0x123450;
            const auto saved=c;auto fn=rt.functions.generated(owner);
            if(!fn) throw std::runtime_error("missing assignment owner");
            fn(ram,&c,&rt);
            check(c.pc==0x123450 && !c.leaving && c.r[2].uw[0]==dst,"complete owner returns destination normally");
            for(int r:{16,17,18,19,20,21,22,23,28,29,30,31})
                check(!std::memcmp(&c.r[r],&saved.r[r],sizeof(c.r[r])),"saved full registers preserved");
            check(!std::memcmp(ram+dst+field,before.data()+dst+field,owner==0x124aa0?8:16),"own-type vptr and padding remain unchanged");
            if(owner==0x124aa0)
            {
                auto expected=before;
                std::memcpy(expected.data()+dst+0x2c0,before.data()+source+0x2c0,24);
                check(!std::memcmp(ram+dst,expected.data()+dst,extent),"CameraFollow copies only its six scalar fields");
            }
            else
            {
                check(!std::memcmp(ram+dst+0x20,before.data()+source+0x20,0x80),"CObject base vector block is copied through the real helper");
                if(owner==0x142da0)
                {
                    check(!std::memcmp(ram+dst+0xdc,before.data()+source+0xdc,0x1f0),"CCharacter array copy includes its ordinary field at numeric offset 2b8");
                    check(!std::memcmp(ram+dst+0x1068,before.data()+source+0x1068,0x140),"CCharacter tail data is copied");
                }
                else check(!std::memcmp(ram+dst+0xa0,before.data()+dst+0xa0,extent-0xa0),"base assignment leaves derived object and vptr intact");
            }
            if(self) check(!std::memcmp(ram+source,before.data()+source,extent),"self-assignment preserves the object");
            else check(!std::memcmp(ram+source,before.data()+source,extent),"distinct source remains unchanged");
            auto expected=before;
            std::copy(ram+dst,ram+dst+extent,expected.data()+dst);
            std::copy(ram+stack-0x1000,ram+stack,expected.data()+stack-0x1000);
            check(!std::memcmp(ram,expected.data(),expected.size()),"only destination and call stack may change");
        }
    std::printf("Dispatch assignments: %d cases, complete owners/real helper, %d failures\n",cases,failures);
    return failures?1:0;
}
