#ifndef XSPCOMM_DETAIL_TRIGGER_REGISTRATION_H
#define XSPCOMM_DETAIL_TRIGGER_REGISTRATION_H

#include "xspcomm/trigger/types.h"
#include "xspcomm/detail/trigger/matcher.h"
#include "xspcomm/xexpr.h"
#include <memory>

namespace xspcomm::detail {

struct TriggerMatch {
    bool hit = false;
    XHitKind kind = XHitKind::Condition;
    uint64_t value = 0;
    uint64_t x_mask = 0;
};

// The trigger kernel owns matching state and never references coverage policy.
struct TriggerRegistration : PatternState {
    enum class Kind : uint8_t { Edge, ClockCycles, ValueEq, ValueChange, Sample, Expr, Sequence, Fsm };
    uint32_t generation = 0;
    bool occupied = false;
    bool armed = false;
    Kind kind = Kind::Edge;
    XPhase phase = XPhase::RisingStable;
    uint64_t source_id = 0;
    uint64_t remaining = 0;
    uint64_t initial_count = 0;
    XData *signal = nullptr;
    uint64_t expected = 0;
    uint64_t expected_x_mask = 0;
    std::shared_ptr<XData> expected_wide;
    std::vector<unsigned char> expected_bytes;
    std::vector<unsigned char> expected_x_bytes;
    XTriggerProgram program;
    bool last_condition = false;

    void ResetPattern() {
        sequence_index = sequence_age = sequence_held = 0;
        fsm_current_state = program.start_state;
        remaining = initial_count;
    }

    TriggerMatch Evaluate(ExprEngine &expr, XPhase phase) {
        switch (kind) {
        case Kind::Edge:
            return {true, phase == XPhase::RisingStable ? XHitKind::ClockRise :
                          phase == XPhase::DriveStable ? XHitKind::DriveStable : XHitKind::ClockFall};
        case Kind::ClockCycles:
            if (--remaining == 0) return {true, XHitKind::ClockCycles};
            break;
        case Kind::ValueEq: {
            const uint64_t value = signal->W() > 64 ? 0 : signal->U();
            const bool known = signal->DataValid();
            const bool current = known && (expected_wide ? signal->Equal(*expected_wide) : value == expected);
            if (MatchCondition(known, current, program.mode, last_condition))
                return {true, XHitKind::Value, value};
            break;
        }
        case Kind::ValueChange:
            if (signal->W() > 64) {
                auto value = signal->GetVU8();
                auto x_value = signal->GetBvalBytes();
                if (value != expected_bytes || x_value != expected_x_bytes) {
                    expected_bytes = std::move(value);
                    expected_x_bytes = std::move(x_value);
                    return {true, XHitKind::ValueChange};
                }
            } else {
                const uint64_t value = signal->U();
                const uint64_t x_mask = signal->XMask();
                if (value != expected || x_mask != expected_x_mask) {
                    expected = value;
                    expected_x_mask = x_mask;
                    return {true, XHitKind::ValueChange, value, x_mask};
                }
            }
            break;
        case Kind::Sample:
            return {true, XHitKind::Condition};
        case Kind::Expr: {
            const bool known = expr.IsKnown(program.root);
            const bool current = known && expr.Eval(program.root) != 0;
            if (MatchCondition(known, current, program.mode, last_condition))
                return {true, XHitKind::Condition, current ? 1U : 0U};
            break;
        }
        case Kind::Sequence:
            if (PatternExecutor::Advance(PatternView::From(program), *this, expr).completed)
                return {true, XHitKind::Fsm, 1};
            break;
        case Kind::Fsm: {
            const auto result = PatternExecutor::Advance(PatternView::From(program), *this, expr);
            if (result.completed) return {true, XHitKind::Fsm, result.terminal};
            break;
        }
        }
        return {};
    }
};

} // namespace xspcomm::detail

#endif
