// The native sound transport (runtime/dc_sound.cpp): the library calls of ezMidiInit, ezMidi and ezTransToIOP, patched
// to nop, reach the compiled sound module's RPC server and memory directly with the requests the original calls made,
// charging each call's 8 cycles.
#include "../runtime/dc_sound.h"
#include "bozo/libs/guest.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <cstdio>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <vector>

namespace {
int failures=0;
void check(bool ok,const char *what){if(!ok){++failures;std::printf("FAIL %s\n",what);}}
constexpr uint32_t client=0x1cecec0,sbuff=0x1cece80,record=0x1cecef0;
struct Host : bozo::port::Host {
    bozo::libs::Libraries &libs;
    explicit Host(bozo::libs::Libraries &l):libs(l){}
    bozo::libs::Libraries &libraries() override{return libs;}
    void frameEnd(Runtime *,R5900Context *) override{throw std::runtime_error("unexpected frame end");}
    bool callLibrary(Runtime *,R5900Context *,uint32_t) override{return false;}
};
uint32_t rd32(const uint8_t *r,uint32_t at){uint32_t v;std::memcpy(&v,r+at,4);return v;}
void wr32(uint8_t *r,uint32_t at,uint32_t v){std::memcpy(r+at,&v,4);}
// One site with argument registers a0..t3 (r4..r11); returns v0.
int32_t site(Runtime &rt,uint32_t pc,std::initializer_list<uint32_t> args,uint64_t *cycles=nullptr){
    R5900Context c;int r=4;for(uint32_t a:args)c.r[r++].ud[0]=a;c.r[2].ud[0]=0x5555;
    const uint64_t before=c.cycles;
    check(dc::sound::nativeSite(rt.memory.ram(),&c,&rt,pc),"the site is native");
    if(cycles)*cycles=c.cycles-before;
    return static_cast<int32_t>(c.r[2].uw[0]);
}
}

int main(){
    auto rt=std::make_unique<Runtime>();bozo::libs::Libraries libs;Host host(libs);rt->port=&host;
    auto *r=rt->memory.ram();
    check(!dc::sound::nativeSite(r,nullptr,rt.get(),0x147a74),"other instructions are not sound sites");

    uint64_t cycles=0;
    check(site(*rt,dc::sound::kInitRpcSite,{0},&cycles)==0 && cycles==8,"sceSifInitRpc: 0, the call's 8 cycles");
    // Before the module's server exists: the client gets an id but no server, so ezMidiInit's loop retries.
    const uint32_t ids=libs.sif.rpcId;
    site(*rt,dc::sound::kBindRpcSite,{client,0x12346,0});
    check(rd32(r,client+4)==ids+1 && rd32(r,client+8)==0xffffffffu && rd32(r,client+0x24)==0,"no server yet: not bound");

    std::vector<bozo::libs::NativeRpcRequest> requests;std::vector<uint32_t> firstWords;
    libs.sif.servers[0x12346]=[&](bozo::libs::Libraries &,bozo::libs::Guest &g,const bozo::libs::NativeRpcRequest &q){
        requests.push_back(q);firstWords.push_back(g.read32(q.send));
        if(q.receiveBytes){uint8_t reply[64]={};reply[0]=0x2a;g.write(q.receive,reply,64);}
    };
    site(*rt,dc::sound::kBindRpcSite,{client,0x12346,0});
    check(rd32(r,client+4)==ids+2 && rd32(r,client+0x24)==(0x12346u|0x80000000u),"with the server, the client is bound");

    // ezMidi's two calls: (client, fno, mode, send, ssize, receive, rsize, end).
    wr32(r,sbuff,0x1234);
    check(site(*rt,dc::sound::kCallRpcSite2,{client,5,0,sbuff,16,sbuff,0,0},&cycles)==0 && cycles==8,"sceSifCallRpc: 0, 8 cycles");
    check(requests.size()==1 && requests[0].command==5 && requests[0].send==sbuff && requests[0].sendBytes==16 &&
          requests[0].receive==sbuff && requests[0].receiveBytes==0 && firstWords[0]==0x1234,"a plain command reaches the server as sent");
    check(rd32(r,client+0x0c)==0 && rd32(r,client+0x10)==5,"the client keeps the mode and the function");
    site(*rt,dc::sound::kCallRpcSite2,{client,0x8007,0,sbuff,16,sbuff,64,0});
    check(requests.size()==2 && requests[1].receiveBytes==64 && rd32(r,sbuff)==0x2a,"a 0x8000 command's reply lands in sbuff");
    wr32(r,0x500000,0xcafe);
    site(*rt,dc::sound::kCallRpcSite,{client,0x1003,0,0x500000,64,sbuff,0,0});
    check(requests.size()==3 && requests[2].send==0x500000 && requests[2].sendBytes==64 && firstWords[2]==0xcafe,
          "a 0x1000 command sends the 64 bytes at its argument");
    site(*rt,dc::sound::kCallRpcSite,{0x1cec000,9,0,sbuff,16,sbuff,0,0});
    check(requests.size()==3,"an unbound client reaches no server");

    // ezTransToIOP: FlushCache, then sceSifSetDma(record, 1) and sceSifDmaStat(id).
    check(site(*rt,dc::sound::kFlushSite,{0},&cycles)==0x5555 && cycles==8,"FlushCache: nothing, 8 cycles");
    uint32_t gotDest=0,gotBytes=0;std::vector<uint8_t> got;
    libs.sif.transfer=[&](uint32_t d,const uint8_t *b,uint32_t n){gotDest=d;gotBytes=n;got.assign(b,b+n);};
    for(uint32_t i=0;i<32;++i)r[0x600000+i]=static_cast<uint8_t>(i*3);
    wr32(r,record,0x600000);wr32(r,record+4,0x9000);wr32(r,record+8,32);wr32(r,record+12,0);
    const int32_t id=site(*rt,dc::sound::kSetDmaSite,{record,1});
    check(id>0 && gotDest==0x9000 && gotBytes==32 && got.size()==32 && got[5]==15,"the record's bytes reach the module's memory at once");
    check(!(libs.io.stat & (1u<<5)),"no DMAC channel 5 interrupt is raised");
    check(site(*rt,dc::sound::kDmaStatSite,{static_cast<uint32_t>(id)})==-1,"sceSifDmaStat: finished");

    std::printf("Native sound transport: %d failures\n",failures);return failures?1:0;
}
