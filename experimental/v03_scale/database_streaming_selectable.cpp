#include "bdr/resolutive_index.hpp"

#ifdef BDR_EXPERIMENTAL_COMPACT_INDEX
#include "compact_candidate.hpp"
#define ResolutiveIndex CompactIndex
#endif

#include "generated_database_streaming.cpp"

#ifdef BDR_EXPERIMENTAL_COMPACT_INDEX
#undef ResolutiveIndex
#endif
