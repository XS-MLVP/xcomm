#include "xspcomm/xengine.h"
#include "xspcomm/xexpr.h"
#include "xspcomm/detail/coverage/state.h"
#include "xspcomm/detail/trigger/matcher.h"

#include <chrono>
#include <algorithm>
#include <stdexcept>

namespace xspcomm {

XEngine::XEngine(XClock *clock, size_t capacity)
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

XEngine::~XEngine() = default;

XRegistrationHandle XEngine::Allocate()
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
    watcher.coverage.reset();
    watcher.sequence_failed = watcher.sequence_expired = false;
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
    watcher.program.mode = XConditionMode::Enter;
    watcher.last_condition = false;
    watcher.program.kind = XTriggerProgramKind::Expr;
    watcher.program.root = -1;
    watcher.program.overlap = false;
    watcher.program.max_active = 1;
    watcher.program.steps.clear();
    watcher.sequence_index = 0;
    watcher.sequence_age = 0;
    watcher.sequence_held = 0;
    watcher.program.transitions.clear();
    watcher.program.state_count = 0;
    watcher.program.start_state = 0;
    watcher.fsm_current_state = 0;
    return {slot, watcher.generation};
}

XRegistrationHandle XEngine::ArmEdge(
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

XRegistrationHandle XEngine::ArmClockCycles(
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

XRegistrationHandle XEngine::ArmValueEq(
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
    watcher.program.mode = mode;
    return handle;
}

XRegistrationHandle XEngine::ArmValueEqBytes(
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
    watcher.program.mode = mode;
    return handle;
}

XRegistrationHandle XEngine::ArmValueChange(
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

XRegistrationHandle XEngine::ArmSample(
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

int XEngine::ExprNewConst(uint64_t value)
{
    return expr_engine->NewConst(value);
}

int XEngine::ExprNewSignal(XData *signal)
{
    if (signal == nullptr || signal->W() > 64) return -1;
    return expr_engine->NewSignal(signal);
}

int XEngine::ExprNewUnary(int op, int child)
{
    return expr_engine->NewUnary(static_cast<ExprOp>(op), child);
}

int XEngine::ExprNewBinary(int op, int lhs, int rhs)
{
    return expr_engine->NewBinary(static_cast<ExprOp>(op), lhs, rhs);
}

int XEngine::ExprNewCompare(int op, int lhs, int rhs)
{
    return expr_engine->NewCompare(static_cast<ExprOp>(op), lhs, rhs);
}

int XEngine::ExprNewCompareSigSig(int op, XData *lhs, XData *rhs)
{
    return expr_engine->NewCompareSigSig(static_cast<ExprOp>(op), lhs, rhs);
}

int XEngine::ExprNewCompareSigConstBytes(
    int op, XData *lhs, std::vector<unsigned char> &rhs)
{
    return expr_engine->NewCompareSigConstBytes(
        static_cast<ExprOp>(op), lhs, rhs);
}

int XEngine::ExprNewCompareConstBytesSig(
    int op, std::vector<unsigned char> &lhs, XData *rhs)
{
    return expr_engine->NewCompareConstBytesSig(
        static_cast<ExprOp>(op), lhs, rhs);
}

int XEngine::ExprNewMaskedCompareSigConstBytes(
    XData *lhs, std::vector<unsigned char> &value, std::vector<unsigned char> &mask)
{
    const int root = expr_engine->NewMaskedCompareSigConstBytes(lhs, value, mask);
    if (root < 0) throw std::invalid_argument("invalid masked signal comparison");
    return root;
}

XRegistrationHandle XEngine::ArmExpr(
    int root, XPhase phase, XConditionMode mode, uint64_t source_id)
{
    XTriggerProgram program;
    program.root = root;
    program.mode = mode;
    program.overlap = false;
    if (!detail::ProgramValidator::Validate(program, *expr_engine)) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Expr;
    watcher.phase = phase;
    watcher.source_id = source_id;
    watcher.program.root = root;
    watcher.program.mode = mode;
    expr_engine->OptimizeShortCircuitOrder(root);
    return handle;
}

XRegistrationHandle XEngine::ArmSequence(
    const std::vector<XSequenceStep> &steps, XPhase phase,
    uint64_t source_id)
{
    if (!detail::ProgramValidator::Sequence(steps, *expr_engine)) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Sequence;
    watcher.phase = phase;
    watcher.source_id = source_id;
    watcher.program.kind = XTriggerProgramKind::Sequence;
    watcher.program.steps = steps;
    for (const auto &step : steps) {
        expr_engine->OptimizeShortCircuitOrder(step.root);
    }
    return handle;
}

XRegistrationHandle XEngine::ArmFsm(
    uint32_t state_count, uint32_t start_state,
    const std::vector<XFsmTransition> &transitions, XPhase phase,
    uint64_t source_id)
{
    if (!detail::ProgramValidator::Fsm(state_count, start_state, transitions, *expr_engine)) return {};
    auto handle = Allocate();
    if (!handle.IsValid()) return handle;
    auto &watcher = watchers[handle.slot];
    watcher.kind = WatcherKind::Fsm;
    watcher.phase = phase;
    watcher.source_id = source_id;
    watcher.program.kind = XTriggerProgramKind::Fsm;
    watcher.program.state_count = state_count;
    watcher.program.start_state = start_state;
    watcher.program.transitions = transitions;
    watcher.fsm_current_state = start_state;
    for (const auto &transition : transitions) {
        if (transition.root >= 0) {
            expr_engine->OptimizeShortCircuitOrder(transition.root);
        }
    }
    return handle;
}

bool XEngine::Disarm(XRegistrationHandle handle)
{
    if (!handle.IsValid() || handle.slot >= watchers.size()) return false;
    auto &watcher = watchers[handle.slot];
    if (!watcher.occupied || watcher.generation != handle.generation) {
        return false;
    }
    watcher.coverage.reset();
    watcher.occupied = false;
    watcher.armed = false;
    watcher.signal = nullptr;
    watcher.expected_wide.reset();
    watcher.expected_bytes.clear();
    watcher.expected_x_bytes.clear();
    free_slots.push_back(handle.slot);
    return true;
}

bool XEngine::RearmSample(XRegistrationHandle handle)
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

bool XEngine::Rearm(XRegistrationHandle handle)
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
        watcher.fsm_current_state = watcher.program.start_state;
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

void XEngine::AppendHit(
    uint32_t slot, Watcher &watcher, XHitKind kind, uint64_t value,
    uint64_t event_id, uint64_t x_mask)
{
    if (watcher.coverage) {
        watcher.coverage->Sample(*expr_engine, clock->GetHalfTick());
        watcher.ResetPattern();
        return;
    }
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

bool XEngine::HasArmedPhase(XPhase phase) const
{
    for (const auto &watcher : watchers) {
        if (watcher.occupied && watcher.armed && watcher.phase == phase) {
            return true;
        }
    }
    return false;
}

// Keep the sampling adapter visible to the phase loop so it can inline.
inline bool XEngine::EvaluateCoverage(Watcher &watcher)
{
    auto &coverage = *watcher.coverage;
    switch (coverage.BeginSample(*expr_engine, clock->GetHalfTick())) {
    case detail::CoverageState::SampleStatus::Skip:
        return true;
    case detail::CoverageState::SampleStatus::Aborted:
        watcher.ResetPattern();
        watcher.last_condition = false;
        return true;
    case detail::CoverageState::SampleStatus::Ready:
        break;
    }
    const auto source = CoverageSource(watcher);
    if (source.sequence || source.fsm) {
        coverage.SamplePattern(*expr_engine, clock->GetHalfTick());
        return true;
    }
    return false;
}

void XEngine::EvaluatePhase(XPhase phase)
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
        if (watcher.coverage && EvaluateCoverage(watcher)) continue;
        const auto match = watcher.Evaluate(*expr_engine, phase);
        if (!match.hit) continue;
        uint64_t event_id = 0;
        if (watcher.kind == WatcherKind::Edge) {
            if (edge_event_id == 0) edge_event_id = next_event_id++;
            event_id = edge_event_id;
        }
        AppendHit(slot, watcher, match.kind, match.value, event_id, match.x_mask);
    }
}

XRunResult XEngine::RunUntil(
    uint64_t max_half_ticks, uint64_t max_wall_time_ns,
    uint32_t budget_check_interval)
{
    XRunResult result;
    result.stopped_phase = clock->GetPhase();
    hit_buffer.clear();
    const auto started = max_wall_time_ns ? std::chrono::steady_clock::now()
                                         : std::chrono::steady_clock::time_point{};
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

XRunResult XEngine::SamplePhase(XPhase phase)
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


size_t XEngine::ActiveCount() const
{
    size_t count = 0;
    for (const auto &watcher : watchers) {
        if (watcher.occupied) count += 1;
    }
    return count;
}

void XEngine::ClearExecutionState()
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
        watcher.program.steps.clear();
        watcher.program.transitions.clear();
        watcher.program.root = -1;
    }
    hit_buffer.clear();
    expr_engine->Clear();
}

void XEngine::Clear()
{
    watchers.clear();
    free_slots.clear();
    hit_buffer.clear();
    next_event_id = 1;
    expr_engine->Clear();
}

} // namespace xspcomm
