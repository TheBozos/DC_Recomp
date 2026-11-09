#pragma once
#include <cstdint>
struct R5900Context;
struct Runtime;
namespace dc::sound {
    // The library calls of ezMidiInit, ezMidi and ezTransToIOP, patched to nop: sceSifInitRpc, sceSifBindRpc, the two
    // sceSifCallRpc, FlushCache, sceSifSetDma and sceSifDmaStat.
    constexpr uint32_t kInitRpcSite=0x14797c,kBindRpcSite=0x147998,kCallRpcSite=0x147a70,kCallRpcSite2=0x147ab4,
                       kFlushSite=0x147b10,kSetDmaSite=0x147b24,kDmaStatSite=0x147b48;
    // libsdr's (sceSdRemoteInit, sceSdTransToIOP, sceSdRemote: set_spu and CSound::SetReverb) calls, at the delay slot
    // that followed each, as its handler first does that instruction: sceSifInitRpc (lui s1), sceSifBindRpc
    // (daddu a2), sceSifSetDma (sw zero,12(v0)), sceSifDmaStat (daddu a0,s1) and the two sceSifCallRpc (addiu t2,64; nop).
    constexpr uint32_t kSdInitRpcSite=0x121264,kSdBindRpcSite=0x121278,kSdSetDmaSite=0x121334,kSdDmaStatSite=0x12134c,
                       kSdCallRpcSite=0x121530,kSdCallRpcSite2=0x121564;
    // The game's sound commands and sample uploads reach DC's compiled sound module (its RPC server and memory) directly,
    // with no SIF RPC, SIF DMA or DMA interrupt in between. True when `pc` is one of the sites (handled).
    bool nativeSite(uint8_t *ram, R5900Context *ctx, Runtime *rt, uint32_t pc);
}
