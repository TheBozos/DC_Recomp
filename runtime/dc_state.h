#pragma once
#include <string>
struct Runtime;
namespace bozo::state { class Archive; }
namespace dc::state {
    // DC's save-state section: what the port keeps across frames that guest memory and the engine's own sections do not
    // hold (the compiled sound modules' IOP host, facts the game told the native renderer once), and, on load, the caches
    // decoded from the old guest memory dropped.
    void serialize(bozo::state::Archive &, Runtime &);
    // Why a state cannot be saved right now (empty: it can).
    std::string unsaveable();
}
