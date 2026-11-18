// Dark Cloud's save-state section (dc_state.h): the parts the engine's own sections do not hold.
#include "dc_state.h"
#include "dc_cheats.h"
#include "dc_ingame_cheats.h"
#include "bozo/runtime.h"
#include "bozo/state.h"

namespace dc
{
    void audioState(bozo::state::Archive &);
    std::string audioUnsaveable();
    namespace native
    {
        void state(bozo::state::Archive &, Runtime &);
        std::string unsaveable();
    }
}

namespace dc::state
{
    void serialize(bozo::state::Archive &a, Runtime &runtime)
    {
        dc::audioState(a);              // the compiled sound modules' IOP host
        dc::native::state(a, runtime);  // the renderer's and asset builders' facts; caches dropped on load
        dc::cheats::menuState(a, runtime.memory.ram());
        if (a.loading())
            dc::cheats::stateLoaded();
    }

    std::string unsaveable()
    {
        if (std::string why = dc::audioUnsaveable(); !why.empty())
            return why;
        return dc::native::unsaveable();
    }
}
