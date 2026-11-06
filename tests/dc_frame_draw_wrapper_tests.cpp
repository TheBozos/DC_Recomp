// Whole retail wrapper fixture versus the production native dispatcher.
#include "frame_draw_ref_functions.h"
constexpr uint32_t wrapperPc = 0x12ed80;
#define FRAME_REFERENCE frame_draw_ref_function_table
#include "frame_dispatch_fixture.h"
