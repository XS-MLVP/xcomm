#ifndef XSPCOMM_XPATTERN_H
#define XSPCOMM_XPATTERN_H

#include <cstdint>
#include <vector>

namespace xspcomm {

enum class XConditionMode : uint8_t {
    Enter = 0,
    EachSample = 1,
    Change = 2,
};

enum class XSequenceStepKind : uint8_t {
    Wait = 0,
    Within = 1,
    Hold = 2,
    Next = 3,
};

struct XSequenceStep {
    XSequenceStepKind kind = XSequenceStepKind::Wait;
    int root = -1;
    uint64_t minimum = 0;
    uint64_t maximum = 0;
    uint64_t cycles = 0;
};

struct XFsmTransition {
    uint32_t from_state = 0;
    int root = -1;
    uint32_t next_state = 0;
    uint32_t terminal_id = 0;
    bool trigger = false;
};

enum class XTriggerProgramKind : uint8_t { Expr = 0, Sequence = 1, Fsm = 2 };

// Immutable after registration; each execution keeps its own matching state.
struct XTriggerProgram {
    XTriggerProgramKind kind = XTriggerProgramKind::Expr;
    int root = -1;
    std::vector<XSequenceStep> steps;
    std::vector<XFsmTransition> transitions;
    uint32_t state_count = 0;
    uint32_t start_state = 0;
    XConditionMode mode = XConditionMode::Enter;
    bool overlap = true;
    uint32_t max_active = 1;
};

} // namespace xspcomm

#endif
