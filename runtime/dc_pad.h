#pragma once
struct Runtime;
namespace bozo::port { class Registry; }
namespace dc::pad {
    // CGamePad's per-frame controller read and vibration, native: the PC's controller (the engine's PadInput) fills the
    // game's PAD_STATUS directly, with no libpad port, mode handshake or pad DMA buffer in between.
    void install(bozo::port::Registry &);
}
