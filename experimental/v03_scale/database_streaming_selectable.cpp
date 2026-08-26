#include "bdr/resolutive_index.hpp"

#ifdef BDR_EXPERIMENTAL_COMPACT_INDEX
#include "bdr/compact_index.hpp"
#define ResolutiveIndex CompactIndex
#endif

#include "generated_database_streaming.cpp"

#ifdef BDR_EXPERIMENTAL_COMPACT_INDEX
#undef ResolutiveIndex
#endif
