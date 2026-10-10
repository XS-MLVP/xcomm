#ifndef XSPCOMM_XCOVERAGE_H
#define XSPCOMM_XCOVERAGE_H

#include "xspcomm/xpattern.h"
#include <cstdint>
#include <string>
#include <vector>

namespace xspcomm {

class XData;

enum class XCoverageItemKind : uint8_t { Value = 0, Pattern = 1, Cross = 2 };
enum class XCoverageBinKind : uint8_t { Normal = 0, Ignore = 1, Illegal = 2, Default = 3 };

// Coverage owns classification and statistics; trigger programs own matching.
struct XCoverageItem {
    XCoverageItemKind kind = XCoverageItemKind::Value;
    int gate = -1;
    XData *signal = nullptr; // Value only
    std::vector<unsigned int> dimensions; // Cross only; preceding value point IDs
};

struct XCoverageBin {
    uint32_t item = 0;
    XCoverageBinKind kind = XCoverageBinKind::Normal;
    XTriggerProgram program;
    std::vector<unsigned int> terminals; // FSM completion selection; empty selects all
    std::vector<unsigned int> dimensions; // Cross tuple of normal bin IDs
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
