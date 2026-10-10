#include "trigger/executor.h"
#include "xspcomm/xexpr.h"

#include <algorithm>
#include <stdexcept>

namespace xspcomm {

namespace detail {

bool ProgramValidator::Sequence(const std::vector<XSequenceStep> &steps, const ExprEngine &expr)
{
    if (steps.empty()) return false;
    for (const auto &step : steps)
        if (!expr.ValidRoot(step.root) || static_cast<unsigned>(step.kind) > 3 ||
            (step.kind == XSequenceStepKind::Within && step.minimum > step.maximum) ||
            (step.kind == XSequenceStepKind::Hold && !step.cycles)) return false;
    return true;
}

bool ProgramValidator::Fsm(uint32_t states, uint32_t start,
                           const std::vector<XFsmTransition> &transitions, const ExprEngine &expr)
{
    if (!states || start >= states || transitions.empty()) return false;
    for (const auto &transition : transitions)
        if (transition.from_state >= states || (!transition.trigger && transition.next_state >= states) ||
            (transition.root != -1 && !expr.ValidRoot(transition.root))) return false;
    return true;
}

bool ProgramValidator::Validate(const XTriggerProgram &program, const ExprEngine &expr)
{
    if (!program.max_active || (!program.overlap && program.max_active != 1) ||
        static_cast<unsigned>(program.mode) > 2) return false;
    switch (program.kind) {
    case XTriggerProgramKind::Expr:
        return expr.ValidRoot(program.root) && program.steps.empty() &&
               program.transitions.empty() && !program.state_count && !program.start_state && !program.overlap;
    case XTriggerProgramKind::Sequence:
        return program.root == -1 && program.transitions.empty() &&
               !program.state_count && !program.start_state && program.mode == XConditionMode::Enter &&
               Sequence(program.steps, expr) &&
               (!program.overlap || program.steps.front().kind == XSequenceStepKind::Wait);
    case XTriggerProgramKind::Fsm:
        return program.root == -1 && program.steps.empty() && program.mode == XConditionMode::Enter &&
               Fsm(program.state_count, program.start_state, program.transitions, expr);
    }
    return false;
}

bool PatternExecutor::AdvanceSequence(const std::vector<XSequenceStep> &steps, PatternState &watcher, ExprEngine &expr)
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

bool PatternExecutor::AdvanceFsm(const std::vector<XFsmTransition> &transitions, PatternState &state, ExprEngine &expr, uint32_t &terminal)
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
