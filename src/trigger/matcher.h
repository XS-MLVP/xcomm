#ifndef XSPCOMM_TRIGGER_MATCHER_H
#define XSPCOMM_TRIGGER_MATCHER_H

#include "xspcomm/detail/pattern.h"
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

struct PatternResult {
    bool completed = false;
    bool failed = false;
    uint32_t terminal = 0;
};

inline PatternResult AdvancePattern(const PatternView &program, PatternState &state, ExprEngine &expr)
{
    PatternResult result;
    if (program.fsm) {
        result.completed = AdvanceFsm(*program.fsm, state, expr, result.terminal);
        result.failed = !result.completed && state.fsm_current_state == program.start_state;
    } else {
        result.completed = AdvanceSequence(*program.sequence, state, expr);
        result.failed = state.sequence_failed;
    }
    return result;
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
