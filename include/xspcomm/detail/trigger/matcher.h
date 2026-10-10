#ifndef XSPCOMM_DETAIL_TRIGGER_MATCHER_H
#define XSPCOMM_DETAIL_TRIGGER_MATCHER_H

#include "xspcomm/detail/trigger/program.h"
#include <algorithm>

namespace xspcomm::detail {

inline bool MatchCondition(bool known, bool current, XConditionMode mode, bool &previous)
{
    if (!known) return false;
    const bool hit = mode == XConditionMode::EachSample ? current :
                     mode == XConditionMode::Enter ? current && !previous : current != previous;
    previous = current;
    return hit;
}

struct MatchUpdate {
    size_t started = 0;
    size_t completed = 0;
    size_t failed = 0;
    size_t expired = 0;
    size_t peak_active = 0;
    bool capacity_exhausted = false;
};

// One execution of a borrowed program. Other executions have independent history.
class PatternMatcher {
public:
    explicit PatternMatcher(PatternView program = {});
    MatchUpdate Advance(ExprEngine &expr, bool overlap = false, size_t max_active = 1);
    // Expression programs use the same history without temporal diagnostics.
    uint64_t AdvanceCondition(ExprEngine &expr);
    // Counts from the latest sample, optionally restricted to selected FSM terminals.
    uint64_t Count(const std::vector<unsigned int> &terminals = {}) const
    {
        if (terminals.empty()) return completed;
        uint64_t count = 0;
        for (size_t i = 0; i < terminal_ids.size(); ++i)
            if (std::find(terminals.begin(), terminals.end(), terminal_ids[i]) != terminals.end())
                count += terminal_counts[i];
        return count;
    }
    const std::vector<PatternState> &States() const { return attempts; }
    void Clear();

private:
    PatternView program;
    std::vector<PatternState> attempts;
    std::vector<unsigned int> terminal_ids;
    std::vector<uint64_t> terminal_counts;
    uint64_t completed = 0;
    bool last_condition = false;
};

} // namespace xspcomm::detail

#endif
