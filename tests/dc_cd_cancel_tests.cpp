// The companion menu's original BreakReadBG must cancel through the native SDK
// binding and still execute InitReadBG. Synthetic queue state; no game data.
#include "dc_functions.h"
#include "bozo/ee/dispatcher.h"
#include "bozo/libs/libraries.h"
#include <cstdio>
#include <cstring>
#include <stdexcept>

namespace {
constexpr uint32_t breakRead=0x13eec0,cdBreak=0x10b4f0,done=0x80000;
constexpr uint32_t gp=0x300000,queue=0x1cbb0c0,stack=0x700000;
struct Host final : bozo::ee::Host {
    bozo::libs::Libraries libs;
    unsigned cancellations=0;
    bool returned=false;
    void libraryCall(bozo::ee::Dispatcher &d,R5900Context *c,uint32_t target) override {
        if(target==done){returned=true;throw bozo::ee::EndRun{};}
        if(target!=cdBreak)throw std::runtime_error("unexpected SDK/RPC call");
        ++cancellations;
        bozo::libs::Guest g(d.runtime(),c);
        libs.find("sceCdBreak")->handler(libs,g);
        c->pc=c->r[31].uw[0];
    }
    void vu0Microprogram(bozo::ee::Dispatcher &,R5900Context *,uint32_t) override {throw std::runtime_error("unexpected VU");}
    void ioRead(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,uint64_t[2]) override {throw std::runtime_error("unexpected IO");}
    void ioWrite(bozo::ee::Dispatcher &,R5900Context *,uint32_t,uint32_t,const uint64_t[2]) override {throw std::runtime_error("unexpected IO");}
    void fault(bozo::ee::Dispatcher &,R5900Context *,const std::string &why) override {throw std::runtime_error(why);}
};
void word(uint8_t *r,uint32_t at,uint32_t n){std::memcpy(r+at,&n,4);}
uint32_t word(uint8_t *r,uint32_t at){uint32_t n;std::memcpy(&n,r+at,4);return n;}
}
int main(){
    Host host;
    bool declared=false;
    for(size_t i=0;i<dc_external_count;++i)if(dc_external_table[i].address==cdBreak)declared=true;
    if(!host.libs.find("sceCdBreak") || !declared){std::puts("FAIL missing native sceCdBreak binding");return 1;}
    for(bool busy:{false,true}){
        bozo::ee::Dispatcher d(host);host.libs.attach(d);host.returned=false;
        d.setFunctions(dc_function_table,dc_function_count,dc_resume_table,dc_resume_count);
        d.bindLibrary(cdBreak);d.bindLibrary(done);
        auto *r=d.runtime().memory.ram();
        std::memset(r+queue,0x5a,32*0x9c);
        word(r,gp-0x72fc,9);word(r,gp-0x72f8,123);
        host.libs.cd.readEnd=busy?10000000:0;host.libs.cd.error=0;
        d.boot(breakRead,stack);
        auto &c=d.kernel().thread(1)->frames.front()->ctx;
        c.r[28].uw[0]=gp;c.r[31].uw[0]=done;
        try{d.run();}catch(const bozo::ee::EndRun &){}
        if(!host.returned || host.libs.cd.readEnd!=0 || host.libs.cd.error!=1 ||
           c.r[29].uw[0]!=stack || word(r,gp-0x72fc)!=0xffffffffu || word(r,gp-0x72f8)!=0)return 2;
        for(unsigned i=0;i<32;++i){
            if(word(r,queue+i*0x9c)!=0)return 3;
            for(unsigned j=4;j<0x9c;++j)if(r[queue+i*0x9c+j]!=0x5a)return 4;
        }
    }
    if(host.cancellations!=2)return 5;
    std::puts("Native CD cancellation: idle/busy BreakReadBG returns and preserves original queue cleanup");
}
