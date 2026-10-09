#include "xspcomm/detail/pattern.h"
#include "xspcomm/xexpr.h"

#include <algorithm>
#include <stdexcept>

namespace xspcomm {

namespace detail {

bool AdvanceSequence(const std::vector<XSequenceStep> &steps, PatternState &watcher, ExprEngine &expr)
{
    const auto &step = steps[watcher.sequence_index];
    bool complete = false;
    watcher.sequence_failed = watcher.sequence_expired = false;
    switch (step.kind) {
    case XSequenceStepKind::Wait:
        complete = expr.IsKnown(step.root) &&
                   expr.Eval(step.root) != 0;
        break;
    case XSequenceStepKind::Next:
        complete = expr.IsKnown(step.root) && expr.Eval(step.root) != 0;
        if (!complete) {
            watcher.sequence_index = watcher.sequence_age = watcher.sequence_held = 0;
            watcher.sequence_failed = true;
        }
        break;
    case XSequenceStepKind::Within:
        watcher.sequence_age += 1;
        if (watcher.sequence_age > step.maximum) {
            watcher.sequence_index = 0;
            watcher.sequence_age = 0;
            watcher.sequence_held = 0;
            watcher.sequence_failed = watcher.sequence_expired = true;
            return false;
        }
        complete = watcher.sequence_age >= step.minimum &&
                   expr.IsKnown(step.root) &&
                   expr.Eval(step.root) != 0;
        break;
    case XSequenceStepKind::Hold:
        if (expr.IsKnown(step.root) &&
            expr.Eval(step.root) != 0) {
            watcher.sequence_held += 1;
        } else {
            watcher.sequence_held = 0;
        }
        complete = watcher.sequence_held >= step.cycles;
        break;
    }
    if (!complete) return false;
    watcher.sequence_index += 1;
    watcher.sequence_age = 0;
    watcher.sequence_held = 0;
    if (watcher.sequence_index == steps.size()) {
        watcher.sequence_index = 0;
        return true;
    }
    return false;
}

bool AdvanceFsm(const std::vector<XFsmTransition> &transitions, PatternState &state, ExprEngine &expr, uint32_t &terminal)
{
    for (const auto &transition : transitions) {
        if (transition.from_state != state.fsm_current_state) continue;
        if (transition.root >= 0 && (!expr.IsKnown(transition.root) ||
                                    expr.Eval(transition.root) == 0)) continue;
        if (transition.trigger) { terminal = transition.terminal_id; return true; }
        state.fsm_current_state = transition.next_state;
        break;
    }
    return false;
}


} // namespace detail

} // namespace xspcomm
