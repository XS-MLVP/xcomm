#include "xspcomm/xtrigger.h"
#include "xspcomm/xexpr.h"

#include <chrono>
#include <stdexcept>

namespace xspcomm {

XTriggerEngine::XTriggerEngine(XClock *clock, size_t capacity)
    : clock(clock), capacity(capacity), expr_engine(std::make_unique<ExprEngine>())
{
    if (clock == nullptr) {
        throw std::invalid_argument("XTriggerEngine clock must not be null");
    }
    if (capacity == 0) {
        throw std::invalid_argument("XTriggerEngine capacity must be positive");
    }
    watchers.reserve(capacity);
    free_slots.reserve(capacity);
    hit_buffer.reserve(capacity);
}

XTriggerEngine::~XTriggerEngine() = default;

XRegistrationHandle XTriggerEngine::Allocate()
{
    uint32_t slot;
    if (!free_slots.empty()) {
        slot = free_slots.back();
        free_slots.pop_back();
        watchers[slot].generation += 1;
    } else {
        if (watchers.size() >= capacity) {
            return {};
        }
        slot = static_cast<uint32_t>(watchers.size());
        watchers.emplace_back();
        watchers[slot].generation = 1;
    }
    Watcher &watcher = watchers[slot];
    watcher.occupied = true;
    watcher.armed = true;
    watcher.signal = nullptr;
    watcher.remaining = 0;
    watcher.initial_count = 0;
    watcher.expected = 0;
    watcher.expected_x_mask = 0;
    watcher.expected_wide.reset();
    watcher.expected_bytes.clear();
    watcher.expected_x_bytes.clear();
    watcher.condition_mode = XConditionMode::Enter;
    watcher.last_condition = false;
    watcher.expr_root = -1;
    watcher.sequence_steps.clear();
    watcher.sequence_index = 0;
    watcher.sequence_age = 0;
    watcher.sequence_held = 0;
    watcher.fsm_transitions.clear();
    watcher.fsm_state_count = 0;
    watcher.fsm_start_state = 0;
    watcher.fsm_current_state = 0;
    return {slot, watcher.generation};
}

XRegistrationHandle XTriggerEngine::ArmEdge(
    XPhase phase, uint64_t source_id)
{
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Edge;
    watcher.phase = phase;
    watcher.source_id = source_id ? source_id : clock->CSelf();
    return handle;
}

XRegistrationHandle XTriggerEngine::ArmClockCycles(
    uint64_t cycles, XPhase phase, uint64_t source_id)
{
    if (cycles == 0) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::ClockCycles;
    watcher.phase = phase;
    watcher.source_id = source_id ? source_id : clock->CSelf();
    watcher.remaining = cycles;
    watcher.initial_count = cycles;
    return handle;
}

XRegistrationHandle XTriggerEngine::ArmValueEq(
    XData *signal, uint64_t expected, XPhase phase, XConditionMode mode,
    uint64_t source_id)
{
    if (signal == nullptr || signal->W() > 64) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::ValueEq;
    watcher.phase = phase;
    watcher.source_id = source_id ? source_id : signal->CSelf();
    watcher.signal = signal;
    watcher.expected = expected;
    watcher.condition_mode = mode;
    return handle;
}

XRegistrationHandle XTriggerEngine::ArmValueEqBytes(
    XData *signal, std::vector<unsigned char> &expected, XPhase phase,
    XConditionMode mode, uint64_t source_id)
{
    if (signal == nullptr || signal->W() <= 64) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::ValueEq;
    watcher.phase = phase;
    watcher.source_id = source_id ? source_id : signal->CSelf();
    watcher.signal = signal;
    watcher.expected_wide = std::make_shared<XData>(signal->W(), XData::InOut);
    watcher.expected_wide->SetVU8(expected);
    watcher.condition_mode = mode;
    return handle;
}

XRegistrationHandle XTriggerEngine::ArmValueChange(
    XData *signal, XPhase phase, uint64_t source_id)
{
    if (signal == nullptr) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::ValueChange;
    watcher.phase = phase;
    watcher.source_id = source_id ? source_id : signal->CSelf();
    watcher.signal = signal;
    if (signal->W() > 64) {
        watcher.expected_bytes = signal->GetVU8();
        watcher.expected_x_bytes = signal->GetBvalBytes();
    } else {
        watcher.expected = signal->U();
        watcher.expected_x_mask = signal->XMask();
    }
    return handle;
}

XRegistrationHandle XTriggerEngine::ArmSample(
    XPhase phase, uint64_t source_id)
{
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Sample;
    watcher.phase = phase;
    watcher.source_id = source_id;
    return handle;
}

int XTriggerEngine::ExprNewConst(uint64_t value)
{
    return expr_engine->NewConst(value);
}

int XTriggerEngine::ExprNewSignal(XData *signal)
{
    if (signal == nullptr || signal->W() > 64) return -1;
    return expr_engine->NewSignal(signal);
}

int XTriggerEngine::ExprNewUnary(int op, int child)
{
    return expr_engine->NewUnary(static_cast<ExprOp>(op), child);
}

int XTriggerEngine::ExprNewBinary(int op, int lhs, int rhs)
{
    return expr_engine->NewBinary(static_cast<ExprOp>(op), lhs, rhs);
}

int XTriggerEngine::ExprNewCompare(int op, int lhs, int rhs)
{
    return expr_engine->NewCompare(static_cast<ExprOp>(op), lhs, rhs);
}

int XTriggerEngine::ExprNewCompareSigSig(int op, XData *lhs, XData *rhs)
{
    return expr_engine->NewCompareSigSig(static_cast<ExprOp>(op), lhs, rhs);
}

int XTriggerEngine::ExprNewCompareSigConstBytes(
    int op, XData *lhs, std::vector<unsigned char> &rhs)
{
    return expr_engine->NewCompareSigConstBytes(
        static_cast<ExprOp>(op), lhs, rhs);
}

int XTriggerEngine::ExprNewCompareConstBytesSig(
    int op, std::vector<unsigned char> &lhs, XData *rhs)
{
    return expr_engine->NewCompareConstBytesSig(
        static_cast<ExprOp>(op), lhs, rhs);
}

XRegistrationHandle XTriggerEngine::ArmExpr(
    int root, XPhase phase, XConditionMode mode, uint64_t source_id)
{
    if (root < 0) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Expr;
    watcher.phase = phase;
    watcher.source_id = source_id;
    watcher.expr_root = root;
    watcher.condition_mode = mode;
    expr_engine->OptimizeShortCircuitOrder(root);
    return handle;
}

XRegistrationHandle XTriggerEngine::ArmSequence(
    const std::vector<XSequenceStep> &steps, XPhase phase,
    uint64_t source_id)
{
    if (steps.empty()) return {};
    for (const auto &step : steps) {
        if (step.root < 0) return {};
        if (step.kind == XSequenceStepKind::Within &&
            step.maximum < step.minimum) {
            return {};
        }
        if (step.kind == XSequenceStepKind::Hold && step.cycles == 0) {
            return {};
        }
    }
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Sequence;
    watcher.phase = phase;
    watcher.source_id = source_id;
    watcher.sequence_steps = steps;
    for (const auto &step : steps) {
        expr_engine->OptimizeShortCircuitOrder(step.root);
    }
    return handle;
}

XRegistrationHandle XTriggerEngine::ArmFsm(
    uint32_t state_count, uint32_t start_state,
    const std::vector<XFsmTransition> &transitions, XPhase phase,
    uint64_t source_id)
{
    if (state_count == 0 || start_state >= state_count || transitions.empty()) {
        return {};
    }
    for (const auto &transition : transitions) {
        if (transition.from_state >= state_count ||
            (!transition.trigger && transition.next_state >= state_count)) {
            return {};
        }
    }
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Fsm;
    watcher.phase = phase;
    watcher.source_id = source_id;
    watcher.fsm_transitions = transitions;
    watcher.fsm_state_count = state_count;
    watcher.fsm_start_state = start_state;
    watcher.fsm_current_state = start_state;
    for (const auto &transition : transitions) {
        if (transition.root >= 0) {
            expr_engine->OptimizeShortCircuitOrder(transition.root);
        }
    }
    return handle;
}

bool XTriggerEngine::Disarm(XRegistrationHandle handle)
{
    if (!handle.IsValid() || handle.slot >= watchers.size()) return false;
    auto &watcher = watchers[handle.slot];
    if (!watcher.occupied || watcher.generation != handle.generation) {
        return false;
    }
    watcher.occupied = false;
    watcher.armed = false;
    watcher.signal = nullptr;
    watcher.expected_wide.reset();
    watcher.expected_bytes.clear();
    watcher.expected_x_bytes.clear();
    free_slots.push_back(handle.slot);
    return true;
}

bool XTriggerEngine::RearmSample(XRegistrationHandle handle)
{
    if (!handle.IsValid() || handle.slot >= watchers.size()) return false;
    auto &watcher = watchers[handle.slot];
    if (!watcher.occupied || watcher.generation != handle.generation ||
        watcher.kind != WatcherKind::Sample || watcher.armed) {
        return false;
    }
    watcher.armed = true;
    return true;
}

bool XTriggerEngine::Rearm(XRegistrationHandle handle)
{
    if (!handle.IsValid() || handle.slot >= watchers.size()) return false;
    auto &watcher = watchers[handle.slot];
    if (!watcher.occupied || watcher.generation != handle.generation ||
        watcher.armed) {
        return false;
    }
    watcher.armed = true;
    if (watcher.kind == WatcherKind::ClockCycles) {
        watcher.remaining = watcher.initial_count;
    } else if (watcher.kind == WatcherKind::Sequence) {
        watcher.sequence_index = 0;
        watcher.sequence_age = 0;
        watcher.sequence_held = 0;
    } else if (watcher.kind == WatcherKind::Fsm) {
        watcher.fsm_current_state = watcher.fsm_start_state;
    } else if (watcher.kind == WatcherKind::ValueChange) {
        if (watcher.signal->W() > 64) {
            watcher.expected_bytes = watcher.signal->GetVU8();
            watcher.expected_x_bytes = watcher.signal->GetBvalBytes();
        } else {
            watcher.expected = watcher.signal->U();
            watcher.expected_x_mask = watcher.signal->XMask();
        }
    }
    return true;
}

void XTriggerEngine::AppendHit(
    uint32_t slot, Watcher &watcher, XHitKind kind, uint64_t value,
    uint64_t event_id, uint64_t x_mask)
{
    if (event_id == 0) event_id = next_event_id++;
    hit_buffer.push_back({
        event_id,
        clock->GetHalfTick(),
        watcher.source_id,
        value,
        x_mask,
        slot,
        watcher.generation,
        kind,
        evaluation_phase,
        0,
    });
    watcher.armed = false;
}

bool XTriggerEngine::AdvanceSequence(Watcher &watcher)
{
    auto &step = watcher.sequence_steps[watcher.sequence_index];
    bool complete = false;
    switch (step.kind) {
    case XSequenceStepKind::Wait:
        complete = expr_engine->IsKnown(step.root) &&
                   expr_engine->Eval(step.root) != 0;
        break;
    case XSequenceStepKind::Within:
        watcher.sequence_age += 1;
        if (watcher.sequence_age > step.maximum) {
            watcher.sequence_index = 0;
            watcher.sequence_age = 0;
            watcher.sequence_held = 0;
            return false;
        }
        complete = watcher.sequence_age >= step.minimum &&
                   expr_engine->IsKnown(step.root) &&
                   expr_engine->Eval(step.root) != 0;
        break;
    case XSequenceStepKind::Hold:
        if (expr_engine->IsKnown(step.root) &&
            expr_engine->Eval(step.root) != 0) {
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
    if (watcher.sequence_index == watcher.sequence_steps.size()) {
        watcher.sequence_index = 0;
        return true;
    }
    return false;
}

bool XTriggerEngine::HasArmedPhase(XPhase phase) const
{
    for (const auto &watcher : watchers) {
        if (watcher.occupied && watcher.armed && watcher.phase == phase) {
            return true;
        }
    }
    return false;
}

void XTriggerEngine::EvaluatePhase(XPhase phase)
{
    evaluation_phase = phase;
    uint64_t edge_event_id = 0;
    expr_engine->SetCycle(clock->GetHalfTick());
    for (uint32_t slot = 0; slot < watchers.size(); ++slot) {
        auto &watcher = watchers[slot];
        if (!watcher.occupied || !watcher.armed ||
            watcher.phase != phase) {
            continue;
        }
        switch (watcher.kind) {
        case WatcherKind::Edge: {
            if (edge_event_id == 0) edge_event_id = next_event_id++;
            XHitKind kind = XHitKind::ClockFall;
            if (phase == XPhase::RisingStable) {
                kind = XHitKind::ClockRise;
            } else if (phase == XPhase::DriveStable) {
                kind = XHitKind::DriveStable;
            }
            AppendHit(slot, watcher, kind, 0, edge_event_id);
            break;
        }
        case WatcherKind::ClockCycles:
            watcher.remaining -= 1;
            if (watcher.remaining == 0) {
                AppendHit(slot, watcher, XHitKind::ClockCycles, 0);
            }
            break;
        case WatcherKind::ValueEq: {
            const uint64_t value = watcher.signal->W() > 64
                                       ? 0 : watcher.signal->U();
            const bool known = watcher.signal->DataValid();
            const bool current = known &&
                (watcher.expected_wide
                     ? watcher.signal->Equal(*watcher.expected_wide)
                     : value == watcher.expected);
            const bool emit = known && (
                watcher.condition_mode == XConditionMode::EachSample
                    ? current
                    : watcher.condition_mode == XConditionMode::Enter
                          ? current && !watcher.last_condition
                          : current != watcher.last_condition);
            if (known) watcher.last_condition = current;
            if (emit) {
                AppendHit(slot, watcher, XHitKind::Value, value);
            }
            break;
        }
        case WatcherKind::ValueChange: {
            if (watcher.signal->W() > 64) {
                auto value = watcher.signal->GetVU8();
                auto x_value = watcher.signal->GetBvalBytes();
                if (value != watcher.expected_bytes ||
                    x_value != watcher.expected_x_bytes) {
                    watcher.expected_bytes = std::move(value);
                    watcher.expected_x_bytes = std::move(x_value);
                    AppendHit(slot, watcher, XHitKind::ValueChange, 0);
                }
                break;
            }
            const uint64_t value = watcher.signal->U();
            const uint64_t x_mask = watcher.signal->XMask();
            if (value != watcher.expected ||
                x_mask != watcher.expected_x_mask) {
                watcher.expected = value;
                watcher.expected_x_mask = x_mask;
                AppendHit(slot, watcher, XHitKind::ValueChange, value, 0,
                          x_mask);
            }
            break;
        }
        case WatcherKind::Sample:
            AppendHit(slot, watcher, XHitKind::Condition, 0);
            break;
        case WatcherKind::Expr: {
            const bool known = expr_engine->IsKnown(watcher.expr_root);
            const bool current = known &&
                                 expr_engine->Eval(watcher.expr_root) != 0;
            const bool emit = known && (
                watcher.condition_mode == XConditionMode::EachSample
                    ? current
                    : watcher.condition_mode == XConditionMode::Enter
                          ? current && !watcher.last_condition
                          : current != watcher.last_condition);
            if (known) watcher.last_condition = current;
            if (emit) {
                AppendHit(slot, watcher, XHitKind::Condition, current ? 1 : 0);
            }
            break;
        }
        case WatcherKind::Sequence:
            if (AdvanceSequence(watcher)) {
                AppendHit(slot, watcher, XHitKind::Fsm, 1);
            }
            break;
        case WatcherKind::Fsm:
            for (const auto &transition : watcher.fsm_transitions) {
                if (transition.from_state != watcher.fsm_current_state) {
                    continue;
                }
                if (transition.root >= 0 &&
                    (!expr_engine->IsKnown(transition.root) ||
                     expr_engine->Eval(transition.root) == 0)) {
                    continue;
                }
                if (transition.trigger) {
                    AppendHit(
                        slot, watcher, XHitKind::Fsm,
                        transition.terminal_id);
                } else {
                    watcher.fsm_current_state = transition.next_state;
                }
                break;
            }
            break;
        }
    }
}

XRunResult XTriggerEngine::RunUntil(
    uint64_t max_half_ticks, uint64_t max_wall_time_ns,
    uint32_t budget_check_interval)
{
    XRunResult result;
    result.stopped_phase = clock->GetPhase();
    hit_buffer.clear();
    const auto started = std::chrono::steady_clock::now();
    if (budget_check_interval == 0) budget_check_interval = 1;

    for (uint64_t i = 0; i < max_half_ticks; ++i) {
        if (!clock->StepHalf()) {
            result.stop_reason = XStopReason::BackendStop;
            result.hits = hit_buffer;
            return result;
        }
        result.advanced_ticks += 1;
        result.stopped_phase = clock->GetPhase();
        EvaluatePhase(clock->GetPhase());
        if (!hit_buffer.empty()) {
            result.stop_reason = XStopReason::TriggerHit;
            for (const auto &hit : hit_buffer) {
                if (hit.kind == XHitKind::ClockFall ||
                    hit.kind == XHitKind::ClockRise) {
                    result.stop_reason = XStopReason::EdgeBarrier;
                    break;
                }
            }
            result.hits = hit_buffer;
            return result;
        }
        if (clock->GetPhase() == XPhase::FallingStable &&
            HasArmedPhase(XPhase::DriveStable)) {
            result.stop_reason = XStopReason::EdgeBarrier;
            result.hits = hit_buffer;
            return result;
        }
        if (max_wall_time_ns != 0 &&
            (i + 1) % budget_check_interval == 0) {
            const auto elapsed = std::chrono::duration_cast<
                std::chrono::nanoseconds>(
                    std::chrono::steady_clock::now() - started);
            if (static_cast<uint64_t>(elapsed.count()) >= max_wall_time_ns) {
                result.stop_reason = XStopReason::QuantumExpired;
                result.hits = hit_buffer;
                return result;
            }
        }
    }
    result.stop_reason = XStopReason::RunLimit;
    result.hits = hit_buffer;
    return result;
}

XRunResult XTriggerEngine::SamplePhase(XPhase phase)
{
    if (phase != XPhase::DriveStable ||
        clock->GetPhase() != XPhase::FallingStable) {
        throw std::logic_error(
            "DriveStable may only be sampled after FallingStable");
    }
    XRunResult result;
    result.stopped_phase = phase;
    result.stop_reason = XStopReason::EdgeBarrier;
    hit_buffer.clear();
    clock->RefreshComb();
    EvaluatePhase(phase);
    if (!hit_buffer.empty()) {
        result.stop_reason = XStopReason::TriggerHit;
    }
    result.hits = hit_buffer;
    return result;
}

size_t XTriggerEngine::ActiveCount() const
{
    size_t count = 0;
    for (const auto &watcher : watchers) {
        if (watcher.occupied) count += 1;
    }
    return count;
}

void XTriggerEngine::ClearExecutionState()
{
    if (ActiveCount() != 0) {
        throw std::logic_error(
            "cannot reset trigger runtime with active watchers");
    }
    for (auto &watcher : watchers) {
        watcher.signal = nullptr;
        watcher.expected_wide.reset();
        watcher.expected_bytes.clear();
        watcher.expected_x_bytes.clear();
        watcher.sequence_steps.clear();
        watcher.fsm_transitions.clear();
        watcher.expr_root = -1;
    }
    hit_buffer.clear();
    expr_engine->Clear();
}

void XTriggerEngine::Clear()
{
    watchers.clear();
    free_slots.clear();
    hit_buffer.clear();
    next_event_id = 1;
    expr_engine->Clear();
}

} // namespace xspcomm
