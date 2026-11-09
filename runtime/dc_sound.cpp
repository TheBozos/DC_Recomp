// Dark Cloud's sound transport, native. The game talks to its sound driver (the IOP module the port compiles natively,
// runtime/dc_audio.cpp) through three routines only, and they stay the game's own code:
// - ezMidiInit binds the client at 0x1CECEC0 to RPC service 0x12346 with sceSifInitRpc / sceSifBindRpc, spinning between
//   attempts until the server exists;
// - ezMidi(command, argument) spins 2000 times, then makes one blocking sceSifCallRpc: with command bit 0x1000 clear the
//   argument goes into sbuff (0x1CECE80) and those 16 bytes are sent, with it set the 64 bytes at the argument are;
//   with bit 0x8000 a 64-byte reply lands in sbuff, whose first word it returns;
// - ezTransToIOP(destination, source, bytes) fills a transfer record, calls FlushCache, sends it with sceSifSetDma and
//   waits on sceSifDmaStat.
// Their seven library calls are guarded patches to nop (recomp/dc_native.patches), and each is a native site here: the
// RPC goes straight to the compiled module's handler (registered when the module called sceSifRegisterRpc), the bytes
// straight into its memory, with no SIF RPC, SIF DMA, DMA id, DMAC interrupt or cache flush. Each site charges the 8 EE
// cycles the call's checkpoint did; the spins stay, so the game's timeline is the original's.
#include "dc_sound.h"
#include "bozo/libs/guest.h"
#include "bozo/libs/libraries.h"
#include "bozo/port.h"
#include "bozo/runtime.h"
#include <cstring>

namespace dc::sound {
namespace {
uint32_t reg(const R5900Context *c,int r){return c->r[r].uw[0];}
void wr32(uint8_t *ram,uint32_t at,uint32_t v){std::memcpy(ram+(at&0x1ffffffu),&v,4);}
void setV0(R5900Context *c,int32_t v){c->r[2].ud[0]=static_cast<uint64_t>(static_cast<int64_t>(v));}
// What the call's checkpoint charged (bozo::checkpoint's accounting; the next checkpoint takes any event due).
void chargeCall(R5900Context *c){c->cycles+=8;++c->checkpoints;}

// sceSifBindRpc(client, service, mode): the client header as the engine's binding leaves it; the server handle only
// once the module's server exists (the game's loop then stops retrying).
void bind(uint8_t *ram,R5900Context *c,bozo::libs::Libraries &l)
{
    const uint32_t client=reg(c,4),service=reg(c,5);
    wr32(ram,client+4,++l.sif.rpcId);wr32(ram,client+8,0xffffffffu);
    if(l.sif.servers.count(service)){wr32(ram,client+0x24,service|0x80000000u);l.sif.bound[client]=service;}
}
// sceSifCallRpc(client, fno, mode, send, ssize, receive, rsize, end): the module's handler answers within the call.
void call(uint8_t *ram,R5900Context *c,Runtime *rt,bozo::libs::Libraries &l)
{
    const uint32_t client=reg(c,4),command=reg(c,5);
    wr32(ram,client+0x0c,reg(c,6));wr32(ram,client+0x10,command);
    const auto bound=l.sif.bound.find(client);
    if(bound==l.sif.bound.end())return;
    if(const auto server=l.sif.servers.find(bound->second);server!=l.sif.servers.end()){
        bozo::libs::Guest guest(*rt,c);
        server->second(l,guest,bozo::libs::NativeRpcRequest{command,reg(c,7),reg(c,8),reg(c,9),reg(c,10)});
    }
}
// sceSifSetDma(transfers, count): each 16-byte record (source, destination, bytes) into the module's memory.
void transfer(R5900Context *c,Runtime *rt,bozo::libs::Libraries &l)
{
    if(!l.sif.transfer)return;
    bozo::libs::Guest guest(*rt,c);
    for(uint32_t i=0;i<reg(c,5) && i<32;++i){
        const uint32_t t=reg(c,4)+16*i,source=guest.read32(t),destination=guest.read32(t+4),bytes=guest.read32(t+8);
        if(bytes && bytes<=0x200000u)l.sif.transfer(destination,guest.in(source,bytes),bytes);
    }
}
}

bool nativeSite(uint8_t *ram,R5900Context *c,Runtime *rt,uint32_t pc)
{
    if(pc!=kInitRpcSite && pc!=kBindRpcSite && pc!=kCallRpcSite && pc!=kCallRpcSite2 && pc!=kFlushSite &&
       pc!=kSetDmaSite && pc!=kDmaStatSite)return false;
    chargeCall(c);
    auto &l=rt->port->libraries();
    if(pc==kInitRpcSite)setV0(c,0);                     // sceSifInitRpc: nothing to set up
    else if(pc==kBindRpcSite){bind(ram,c,l);setV0(c,0);}
    else if(pc==kCallRpcSite || pc==kCallRpcSite2){call(ram,c,rt,l);setV0(c,0);}
    else if(pc==kSetDmaSite){transfer(c,rt,l);setV0(c,1);} // a transfer id: done at once
    else if(pc==kDmaStatSite)setV0(c,-1);               // finished
    return true;                                        // FlushCache: no cache to write back
}
}
