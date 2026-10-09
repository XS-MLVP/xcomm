#ifndef XSPCOMM_COVERAGE_STATE_H
#define XSPCOMM_COVERAGE_STATE_H

#include "xspcomm/xcoverage.h"
#include "../trigger/matcher.h"
#include "xspcomm/xexpr.h"
#include <algorithm>
#include <limits>
#include <stdexcept>

namespace xspcomm::detail {

// Coverage applies statistical policies to executions provided by trigger matchers.
class CoverageState {
public:
    enum class SampleStatus { Ready, Skip, Aborted };
    CoverageState(uint32_t generation, ExprEngine &expr, const PatternView &source,
                  const std::vector<XCoverageItem> &items,
                  const std::vector<XCoverageBin> &bins,
                  int gate, int abort, bool raise_illegal, size_t diagnostic_capacity,
                  bool overlap, size_t max_active, bool diagnostics);
    SampleStatus BeginSample(ExprEngine &expr, uint64_t tick)
    {
        if (sampled && evaluated_tick == tick) return SampleStatus::Skip;
        sampled = true;
        evaluated_tick = tick;
        if (abort >= 0 && expr.IsKnown(abort) && expr.Eval(abort)) {
            ClearHistory(true);
            return SampleStatus::Aborted;
        }
        return SampleStatus::Ready;
    }
    void Sample(ExprEngine &expr, uint64_t tick);
    void SamplePattern(ExprEngine &expr, uint64_t tick);
    size_t ExecutionCount() const;
    void ClearHistory(bool aborted = false);
    void ResetCounters();
    XCoverageSnapshot Snapshot(uint64_t tick, bool progress) const;

private:
    size_t Advance(ExprEngine &expr, PatternMatcher &matcher,
                   size_t pattern, bool overlap, size_t max_active);
    std::vector<XCoverageItem> items;
    std::vector<XCoverageBin> bins;
    PatternMatcher source_matcher;
    std::vector<PatternMatcher> matchers;
    std::vector<size_t> owners;
    bool overlap = false;
    size_t max_active = 1;
    enum { Started, Completed, Failed, Expired, Aborted, Cleared, PeakActive, DiagnosticCount };
    void Bump(size_t pattern, size_t field, uint64_t amount = 1) {
        if (snapshot.diagnostics.empty()) return;
        auto &count = snapshot.diagnostics[pattern * DiagnosticCount + field];
        if (field == PeakActive) { count = std::max<uint64_t>(count, amount); return; }
        if (std::numeric_limits<uint64_t>::max() - count < amount) {
            snapshot.incomplete = true;
            throw std::overflow_error("coverage diagnostic counter overflow");
        }
        count += amount;
    }
    void ClearMatcher(size_t pattern, PatternMatcher &matcher, size_t reason) {
        Bump(pattern, reason, matcher.States().size());
        matcher.Clear();
    }
    std::vector<uint64_t> delta, normal_counts;
    std::vector<bool> matched;
    XCoverageSnapshot snapshot;
    int gate = -1, abort = -1;
    bool raise_illegal = true;
    size_t diagnostic_capacity = 1024;
    bool sampled = false;
    uint64_t evaluated_tick = 0;
};

} // namespace xspcomm::detail

#endif
