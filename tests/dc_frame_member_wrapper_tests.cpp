// Whole retail wrapper fixture versus the production native dispatcher.
#include "frame_member_ref_functions.h"
constexpr uint32_t wrapperPc = 0x12a110;
#define FRAME_REFERENCE frame_member_ref_function_table
#include "frame_dispatch_fixture.h"
