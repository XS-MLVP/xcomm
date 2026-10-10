#ifndef XSPCOMM_TRIGGER_EXECUTOR_H
#define XSPCOMM_TRIGGER_EXECUTOR_H

#include "xspcomm/xpattern.h"
#include <cstddef>
#include <vector>

namespace xspcomm {
class ExprEngine;
namespace detail {

class ProgramValidator {
public:
    static bool Validate(const XTriggerProgram &program, const ExprEngine &expr);
    static bool Sequence(const std::vector<XSequenceStep> &steps, const ExprEngine &expr);
    static bool Fsm(uint32_t states, uint32_t start, const std::vector<XFsmTransition> &transitions,
                    const ExprEngine &expr);
};

struct PatternState {
    bool sequence_failed = false;
    bool sequence_expired = false;
    size_t sequence_index = 0;
    uint64_t sequence_age = 0;
    uint64_t sequence_held = 0;
    uint32_t fsm_current_state = 0;
};

// A non-owning view, valid only while evaluating the owning registration.
struct PatternView {
    const std::vector<XSequenceStep> *sequence = nullptr;
    const std::vector<XFsmTransition> *fsm = nullptr;
    uint32_t start_state = 0;
    int root = -1;
    XConditionMode mode = XConditionMode::Enter;

    static PatternView From(const XTriggerProgram &program) {
        PatternView view;
        view.start_state = program.start_state;
        view.root = program.root;
        view.mode = program.mode;
        if (program.kind == XTriggerProgramKind::Sequence) view.sequence = &program.steps;
        if (program.kind == XTriggerProgramKind::Fsm) view.fsm = &program.transitions;
        return view;
    }
};

struct PatternResult {
    bool completed = false;
    bool failed = false;
    uint32_t terminal = 0;
};

class PatternExecutor {
public:
    static PatternResult Advance(const PatternView &program, PatternState &state, ExprEngine &expr) {
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
private:
    static bool AdvanceSequence(const std::vector<XSequenceStep> &steps,
                                PatternState &state, ExprEngine &expr);
    static bool AdvanceFsm(const std::vector<XFsmTransition> &transitions,
                           PatternState &state, ExprEngine &expr, uint32_t &terminal);
};

} // namespace detail
} // namespace xspcomm

#endif
