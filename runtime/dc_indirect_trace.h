#pragma once
#include "bozo/context.h"
// Only linked in an explicitly enabled DC_AUDIT_INDIRECT build. No guest writes,
// instruction decoding, target replacement or dispatch decisions.
uint32_t dcTraceIndirect(Runtime *, const R5900Context *, uint32_t image, uint32_t pc, uint32_t target);
bool dcFinishIndirectTrace(bool complete);
