#pragma once
#include <cstdint>
struct R5900Context;
struct Runtime;
namespace dc::sound {
    // The library calls of ezMidiInit, ezMidi and ezTransToIOP, patched to nop: sceSifInitRpc, sceSifBindRpc, the two
    // sceSifCallRpc, FlushCache, sceSifSetDma and sceSifDmaStat.
    constexpr uint32_t kInitRpcSite=0x14797c,kBindRpcSite=0x147998,kCallRpcSite=0x147a70,kCallRpcSite2=0x147ab4,
                       kFlushSite=0x147b10,kSetDmaSite=0x147b24,kDmaStatSite=0x147b48;
    // The game's sound commands and sample uploads reach DC's compiled sound module (its RPC server and memory) directly,
    // with no SIF RPC, SIF DMA or DMA interrupt in between. True when `pc` is one of the sites (handled).
    bool nativeSite(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc);
}
