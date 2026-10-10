#include "xspcomm/detail/trigger/matcher.h"
#include "xspcomm/xexpr.h"

#include <algorithm>
#include <stdexcept>

namespace xspcomm::detail {

PatternMatcher::PatternMatcher(PatternView program) : program(program)
{
    if (program.sequence) attempts.reserve(program.sequence->size());
    if (program.fsm) {
        for (const auto &transition : *program.fsm) {
            if (transition.trigger &&
                std::find(terminal_ids.begin(), terminal_ids.end(), transition.terminal_id) == terminal_ids.end())
                terminal_ids.push_back(transition.terminal_id);
        }
        terminal_counts.resize(terminal_ids.size());
    }
}

uint64_t PatternMatcher::AdvanceCondition(ExprEngine &expr)
{
    const bool known = expr.IsKnown(program.root);
    const bool current = known && expr.Eval(program.root) != 0;
    completed = MatchCondition(known, current, program.mode, last_condition);
    return completed;
}

MatchUpdate PatternMatcher::Advance(ExprEngine &expr, bool overlap, size_t max_active)
{
    MatchUpdate update;
    completed = 0;
    std::fill(terminal_counts.begin(), terminal_counts.end(), 0);
    if (!program.sequence && !program.fsm) {
        update.completed = AdvanceCondition(expr);
        return update;
    }
    auto record = [&](uint32_t terminal) {
        ++completed;
        ++update.completed;
        if (program.fsm) {
            const auto found = std::find(terminal_ids.begin(), terminal_ids.end(), terminal);
            if (found == terminal_ids.end()) throw std::logic_error("unknown trigger completion terminal");
            ++terminal_counts[found - terminal_ids.begin()];
        }
    };
    const bool had_active = !attempts.empty();
    size_t kept = 0;
    for (size_t i = 0; i < attempts.size(); ++i) {
        auto &state = attempts[i];
        const auto result = PatternExecutor::Advance(program, state, expr);
        if (result.completed) record(result.terminal);
        else if (result.failed) {
            if (state.sequence_expired) ++update.expired;
            else ++update.failed;
        }
        if (!result.completed && !result.failed) {
            if (kept != i) attempts[kept] = state;
            ++kept;
        }
    }
    attempts.resize(kept);
    if (overlap || !had_active) {
        PatternState candidate;
        candidate.fsm_current_state = program.start_state;
        const auto result = PatternExecutor::Advance(program, candidate, expr);
        const bool active = program.fsm ? candidate.fsm_current_state != program.start_state :
                           candidate.sequence_index || candidate.sequence_age || candidate.sequence_held;
        if (result.completed || active) {
            if (attempts.size() >= max_active) {
                update.capacity_exhausted = true;
                return update;
            }
            ++update.started;
            update.peak_active = attempts.size() + 1;
            if (result.completed) record(result.terminal);
            else attempts.push_back(candidate);
        }
    }
    return update;
}

void PatternMatcher::Clear()
{
    attempts.clear();
    last_condition = false;
    completed = 0;
    std::fill(terminal_counts.begin(), terminal_counts.end(), 0);
}

} // namespace xspcomm::detail
