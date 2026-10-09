#ifndef XSPCOMM_XCOVERAGE_H
#define XSPCOMM_XCOVERAGE_H

#include "xspcomm/xpattern.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xspcomm {

class XData;
namespace detail { class CoverageState; }

// Coverage uses numeric IDs in the hot path. Human names live in the client.
struct XCoverageItem {
    int gate = -1;
    bool pattern = false; // event point; no synthetic XData source
    XData *signal = nullptr; // direct value source; null for a cross or pattern point
    std::vector<unsigned int> dimensions; // point IDs; empty for a point
};

struct XCoverageBin {
    uint32_t item = 0;
    uint32_t kind = 0; // normal / ignore / illegal / default
    int root = -1;
    std::vector<XSequenceStep> steps;
    bool overlap = true;
    uint32_t program_kind = 0; // Expr=0, Sequence=1, FSM=2 (pattern points)
    XConditionMode mode = XConditionMode::Enter;
    uint32_t max_active = 1;
    uint32_t state_count = 0, start_state = 0;
    std::vector<XFsmTransition> transitions;
    std::vector<unsigned int> terminals; // empty selects all
    std::vector<unsigned int> dimensions; // normal bin IDs for a cross tuple
};

struct XCoverageSnapshot {
    uint32_t generation = 0;
    uint64_t epoch = 0;
    uint64_t tick = 0;
    // group samples/gated; samples/gated/ignored/unmatched/unknown per item;
    // then bin counts in declaration order.
    std::vector<unsigned long long> counters;
    std::vector<unsigned int> illegal_bins;
    std::vector<std::string> illegal_values; // full known value in hexadecimal, sampled at the hit
    std::vector<unsigned long long> illegal_ticks;
    // started/completed/failed/expired/aborted/cleared/peak_active for source,
    // then each bin. Empty when summary diagnostics are disabled.
    std::vector<unsigned long long> diagnostics;
    // Optional rows: pattern ID (0=source, bin+1), step, age, held, FSM state.
    std::vector<unsigned long long> progress;
    bool incomplete = false;
};

} // namespace xspcomm

#endif
