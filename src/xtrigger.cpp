#include "xspcomm/xtrigger.h"
#include "xspcomm/xexpr.h"

#include <chrono>
#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace xspcomm {

struct XTriggerEngine::CoverageState {
    std::vector<XCoverageItem> items;
    std::vector<XCoverageBin> bins;
    std::vector<std::vector<MatchState>> attempts;
    std::vector<MatchState> source_attempts;
    std::vector<Watcher> programs;
    std::vector<size_t> owners; // bin -> first bin running the same observation
    std::vector<std::vector<unsigned int>> result_ids;
    std::vector<std::vector<uint64_t>> result_counts;
    std::vector<size_t> completions;
    std::vector<bool> last_conditions;
    bool overlap = false;
    size_t max_active = 1;
    enum { Started, Completed, Failed, Expired, Aborted, Cleared, PeakActive, DiagnosticCount };
    void Bump(size_t pattern, size_t field, uint64_t amount = 1) {
        if (snapshot.diagnostics.empty()) return;
        auto &count = snapshot.diagnostics[pattern * DiagnosticCount + field];
        if (field == PeakActive) { count = std::max<uint64_t>(count, amount); return; }
        if (std::numeric_limits<uint64_t>::max() - count < amount) {
            snapshot.incomplete = true;
            throw std::overflow_error("coverage diagnostic counter overflow");
        }
        count += amount;
    }
    void ClearAttempts(size_t pattern, std::vector<MatchState> &states, size_t reason) {
        Bump(pattern, reason, states.size());
        states.clear();
    }
    std::vector<uint64_t> delta, normal_counts;
    std::vector<bool> matched;
    XCoverageSnapshot snapshot;
    int gate = -1, abort = -1;
    bool raise_illegal = true;
    size_t diagnostic_capacity = 1024;
    bool sampled = false;
    uint64_t evaluated_tick = 0;
};

// The item fixes the gate; clock, phase, abort and binding belong to the group.
// Terminal selection, kind and thresholds do not change the observed process.
static bool SameCoverageExecution(const XCoverageBin &a, const XCoverageBin &b)
{
    if (std::tie(a.item, a.program_kind, a.root, a.mode, a.overlap, a.max_active,
                 a.state_count, a.start_state) !=
        std::tie(b.item, b.program_kind, b.root, b.mode, b.overlap, b.max_active,
                 b.state_count, b.start_state)) return false;
    if (a.steps.size() != b.steps.size() || a.transitions.size() != b.transitions.size()) return false;
    return std::equal(a.steps.begin(), a.steps.end(), b.steps.begin(), [](const auto &x, const auto &y) {
        return std::tie(x.kind, x.root, x.minimum, x.maximum, x.cycles) ==
               std::tie(y.kind, y.root, y.minimum, y.maximum, y.cycles);
    }) && std::equal(a.transitions.begin(), a.transitions.end(), b.transitions.begin(), [](const auto &x, const auto &y) {
        return std::tie(x.from_state, x.root, x.next_state, x.terminal_id, x.trigger) ==
               std::tie(y.from_state, y.root, y.next_state, y.terminal_id, y.trigger);
    });
}

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

int XTriggerEngine::ExprNewMaskedCompareSigConstBytes(
    XData *lhs, std::vector<unsigned char> &value, std::vector<unsigned char> &mask)
{
    const int root = expr_engine->NewMaskedCompareSigConstBytes(lhs, value, mask);
    if (root < 0) throw std::invalid_argument("invalid masked signal comparison");
    return root;
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
    if (watcher.coverage) {
        SampleCoverage(watcher);
        watcher.remaining = watcher.initial_count;
        watcher.sequence_index = watcher.sequence_age = watcher.sequence_held = 0;
        watcher.fsm_current_state = watcher.fsm_start_state;
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

bool XTriggerEngine::AdvanceSequence(const std::vector<XSequenceStep> &steps, MatchState &watcher)
{
    const auto &step = steps[watcher.sequence_index];
    bool complete = false;
    watcher.sequence_failed = watcher.sequence_expired = false;
    switch (step.kind) {
    case XSequenceStepKind::Wait:
        complete = expr_engine->IsKnown(step.root) &&
                   expr_engine->Eval(step.root) != 0;
        break;
    case XSequenceStepKind::Next:
        complete = expr_engine->IsKnown(step.root) && expr_engine->Eval(step.root) != 0;
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
    if (watcher.sequence_index == steps.size()) {
        watcher.sequence_index = 0;
        return true;
    }
    return false;
}

bool XTriggerEngine::AdvanceFsm(const Watcher &program, MatchState &state, uint32_t &terminal)
{
    for (const auto &transition : program.fsm_transitions) {
        if (transition.from_state != state.fsm_current_state) continue;
        if (transition.root >= 0 && (!expr_engine->IsKnown(transition.root) ||
                                    expr_engine->Eval(transition.root) == 0)) continue;
        if (transition.trigger) { terminal = transition.terminal_id; return true; }
        state.fsm_current_state = transition.next_state;
        break;
    }
    return false;
}

size_t XTriggerEngine::AdvanceCoverageAttempts(Watcher &watcher,
    const std::vector<XSequenceStep> &steps, std::vector<MatchState> &attempts,
    size_t pattern, bool overlap, size_t max_active,
    const Watcher *program, std::vector<uint64_t> *results, const std::vector<unsigned int> *result_ids)
{
    auto &c = *watcher.coverage;
    const Watcher &code = program ? *program : watcher;
    const bool fsm = (program || pattern == 0) && code.kind == WatcherKind::Fsm;
    uint32_t terminal = 0;
    auto record = [&]() {
        if (results) {
            const auto found = std::find(result_ids->begin(), result_ids->end(), terminal);
            if (found == result_ids->end()) throw std::logic_error("unknown coverage completion terminal");
            auto &count = (*results)[found - result_ids->begin()];
            if (count == std::numeric_limits<uint64_t>::max()) {
                c.snapshot.incomplete = true;
                throw std::overflow_error("coverage terminal count overflow");
            }
            ++count;
        }
        c.Bump(pattern, CoverageState::Completed);
    };
    const bool had_active = !attempts.empty();
    auto advance = [&](MatchState &state) {
        return fsm ? AdvanceFsm(code, state, terminal) : AdvanceSequence(steps, state);
    };
    size_t completed = 0;
    size_t kept = 0;
    for (size_t i = 0; i < attempts.size(); ++i) {
        auto &state = attempts[i];
        const bool done = advance(state);
        const bool failed = fsm ? (!done && state.fsm_current_state == code.fsm_start_state)
                                : state.sequence_failed;
        if (done) { ++completed; record(); }
        else if (failed) c.Bump(pattern, state.sequence_expired ? CoverageState::Expired : CoverageState::Failed);
        if (!done && !failed) {
            if (kept != i) attempts[kept] = state;
            ++kept;
        }
    }
    attempts.resize(kept);
    if (overlap || !had_active) {
        MatchState candidate;
        candidate.fsm_current_state = code.fsm_start_state;
        const bool done = advance(candidate);
        const bool active = fsm ? candidate.fsm_current_state != code.fsm_start_state
                                : candidate.sequence_index || candidate.sequence_age || candidate.sequence_held;
        if (done || active) {
            if (attempts.size() >= max_active) {
                c.snapshot.incomplete = true;
                throw std::overflow_error("coverage active pattern capacity exhausted");
            }
            c.Bump(pattern, CoverageState::Started);
            c.Bump(pattern, CoverageState::PeakActive, attempts.size() + 1);
            if (done) { ++completed; record(); }
            else attempts.push_back(candidate);
        }
    }
    return completed;
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
        if (watcher.coverage) {
            auto &c = *watcher.coverage;
            if (c.sampled && c.evaluated_tick == clock->GetHalfTick()) continue;
            c.sampled = true;
            c.evaluated_tick = clock->GetHalfTick();
            if (c.abort >= 0 && expr_engine->IsKnown(c.abort) && expr_engine->Eval(c.abort)) {
                ClearCoverageHistory(watcher, true);
                continue;
            }
            if (watcher.kind == WatcherKind::Sequence || watcher.kind == WatcherKind::Fsm) {
                const auto completed = AdvanceCoverageAttempts(watcher, watcher.sequence_steps,
                    c.source_attempts, 0, c.overlap, c.max_active);
                for (size_t i = 0; i < completed; ++i) SampleCoverage(watcher);
                continue;
            }
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
            if (AdvanceSequence(watcher.sequence_steps, watcher)) {
                AppendHit(slot, watcher, XHitKind::Fsm, 1);
            }
            break;
        case WatcherKind::Fsm: {
            uint32_t terminal = 0;
            if (AdvanceFsm(watcher, watcher, terminal)) AppendHit(slot, watcher, XHitKind::Fsm, terminal);
            break;
        }
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


XTriggerEngine::Watcher &XTriggerEngine::CoverageWatcher(XRegistrationHandle handle)
{
    if (!handle.IsValid() || handle.slot >= watchers.size() ||
        !watchers[handle.slot].occupied || watchers[handle.slot].generation != handle.generation ||
        !watchers[handle.slot].coverage) throw std::invalid_argument("stale coverage handle");
    return watchers[handle.slot];
}

void XTriggerEngine::AttachCoverage(XRegistrationHandle handle,
    const std::vector<XCoverageItem> &items, const std::vector<XCoverageBin> &bins,
    int gate, int abort, bool raise_illegal, size_t diagnostic_capacity,
    bool overlap, size_t max_active, bool diagnostics)
{
    if (!handle.IsValid() || handle.slot >= watchers.size() ||
        !watchers[handle.slot].occupied || watchers[handle.slot].generation != handle.generation ||
        watchers[handle.slot].coverage) throw std::invalid_argument("invalid coverage registration");
    if (items.empty() || bins.empty() || diagnostic_capacity == 0)
        throw std::invalid_argument("empty coverage definition or diagnostic capacity");
    const auto &program = watchers[handle.slot];
    if (max_active == 0 || (!overlap && max_active != 1))
        throw std::invalid_argument("invalid coverage max_active");
    if (overlap && program.kind != WatcherKind::Sequence && program.kind != WatcherKind::Fsm)
        throw std::invalid_argument("coverage overlap requires Sequence or FSM");
    if (overlap && program.kind == WatcherKind::Sequence &&
        program.sequence_steps.front().kind != XSequenceStepKind::Wait)
        throw std::invalid_argument("overlapping Sequence must start with Wait");
    auto check = [&](int r) {
        if (r != -1 && !expr_engine->ValidRoot(r)) throw std::invalid_argument("invalid coverage expression");
    };
    check(gate); check(abort);
    for (size_t i = 0; i < items.size(); ++i) {
        check(items[i].gate);
        if (items[i].pattern && (items[i].signal || !items[i].dimensions.empty()))
            throw std::invalid_argument("pattern point cannot have a signal or dimensions");
        if (!items[i].pattern && items[i].dimensions.empty() != (items[i].signal != nullptr))
            throw std::invalid_argument("coverage point requires a signal; cross must not have one");
        for (auto d : items[i].dimensions)
            if (d >= i || !items[d].dimensions.empty() || items[d].pattern) throw std::invalid_argument("invalid cross point");
    }
    for (const auto &bin : bins) {
        if (bin.item >= items.size() || bin.kind > 3) throw std::invalid_argument("invalid coverage bin");
        check(bin.root);
        const bool pattern = items[bin.item].pattern;
        if (pattern) {
            if (bin.kind == 3 || bin.program_kind > 2 || static_cast<unsigned>(bin.mode) > 2 ||
                !bin.max_active || (!bin.overlap && bin.max_active != 1))
                throw std::invalid_argument("invalid pattern bin options");
            if (bin.program_kind == 0 && (bin.root < 0 || !bin.steps.empty() || !bin.transitions.empty() || !bin.terminals.empty() || bin.overlap))
                throw std::invalid_argument("invalid expression pattern");
            if (bin.program_kind == 1 && (bin.root != -1 || bin.steps.empty() || !bin.transitions.empty() || !bin.terminals.empty()))
                throw std::invalid_argument("invalid sequence pattern");
            if (bin.program_kind != 0 && bin.mode != XConditionMode::Enter)
                throw std::invalid_argument("condition modes apply only to expression patterns");
            if (bin.program_kind == 2) {
                if (bin.root != -1 || !bin.state_count || bin.start_state >= bin.state_count || !bin.steps.empty())
                    throw std::invalid_argument("invalid FSM pattern");
                for (const auto &t : bin.transitions) {
                    check(t.root);
                    if (t.from_state >= bin.state_count || (!t.trigger && t.next_state >= bin.state_count))
                        throw std::invalid_argument("invalid FSM transition");
                }
                for (auto terminal : bin.terminals)
                    if (std::none_of(bin.transitions.begin(), bin.transitions.end(), [&](const auto &t) { return t.trigger && t.terminal_id == terminal; }))
                        throw std::invalid_argument("unknown FSM terminal");
            }
        }
        for (const auto &step : bin.steps) {
            if (step.root < 0 || (!pattern && step.kind != XSequenceStepKind::Wait && step.kind != XSequenceStepKind::Next))
                throw std::invalid_argument("coverage transition requires adjacent steps");
            if (static_cast<unsigned>(step.kind) > 3 ||
                (step.kind == XSequenceStepKind::Within && step.minimum > step.maximum) ||
                (step.kind == XSequenceStepKind::Hold && !step.cycles))
                throw std::invalid_argument("invalid coverage sequence step");
            check(step.root);
        }
        if (pattern && bin.overlap && bin.program_kind == 1 && bin.steps.front().kind != XSequenceStepKind::Wait)
            throw std::invalid_argument("overlapping Sequence must start with Wait");
        const auto &dims = items[bin.item].dimensions;
        if (bin.dimensions.size() != dims.size()) throw std::invalid_argument("invalid cross tuple");
        for (size_t j = 0; j < dims.size(); ++j) {
            auto d = bin.dimensions[j];
            if (d >= bins.size() || bins[d].kind != 0 || bins[d].item != dims[j])
                throw std::invalid_argument("invalid cross bin");
        }
    }
    auto c = std::make_shared<CoverageState>();
    c->items = items; c->bins = bins; c->gate = gate; c->abort = abort;
    c->raise_illegal = raise_illegal; c->diagnostic_capacity = diagnostic_capacity;
    c->overlap = overlap; c->max_active = max_active;
    // Grow only with observed starts, never reserve arbitrary max_active up front.
    if (diagnostics) c->snapshot.diagnostics.resize((1 + bins.size()) * CoverageState::DiagnosticCount);
    c->snapshot.generation = handle.generation;
    c->snapshot.counters.resize(2 + items.size() * 5 + bins.size());
    c->delta.resize(c->snapshot.counters.size());
    c->matched.resize(bins.size()); c->normal_counts.resize(items.size());
    c->attempts.resize(bins.size());
    c->programs.resize(bins.size());
    c->owners.resize(bins.size());
    c->result_ids.resize(bins.size());
    c->result_counts.resize(bins.size());
    c->completions.resize(bins.size());
    c->last_conditions.resize(bins.size());
    for (size_t i = 0; i < bins.size(); ++i) {
        c->owners[i] = i;
        if (items[bins[i].item].pattern) {
            for (size_t previous = 0; previous < i; ++previous) {
                if (c->owners[previous] == previous && SameCoverageExecution(bins[i], bins[previous])) {
                    c->owners[i] = previous;
                    break;
                }
            }
        }
        if (c->owners[i] != i) {
            // The shared owner retains the executable descriptor and history.
            c->bins[i].steps.clear();
            c->bins[i].transitions.clear();
            continue;
        }
        auto &p = c->programs[i];
        p.kind = bins[i].program_kind == 2 ? WatcherKind::Fsm : WatcherKind::Sequence;
        p.fsm_state_count = bins[i].state_count; p.fsm_start_state = bins[i].start_state;
        p.fsm_transitions = bins[i].transitions;
        auto &ids = c->result_ids[i];
        if (items[bins[i].item].pattern && bins[i].program_kind == 2) {
            for (const auto &t : bins[i].transitions)
                if (t.trigger && std::find(ids.begin(), ids.end(), t.terminal_id) == ids.end())
                    ids.push_back(t.terminal_id);
        } else ids.push_back(0);
        c->result_counts[i].resize(ids.size());
        c->attempts[i].reserve(bins[i].steps.size());
    }
    c->snapshot.illegal_bins.reserve(diagnostic_capacity);
    c->snapshot.illegal_values.reserve(diagnostic_capacity);
    c->snapshot.illegal_ticks.reserve(diagnostic_capacity);
    watchers[handle.slot].coverage = std::move(c);
}

void XTriggerEngine::ClearCoverageHistory(Watcher &watcher, bool aborted)
{
    watcher.sequence_index = watcher.sequence_age = watcher.sequence_held = 0;
    watcher.fsm_current_state = watcher.fsm_start_state;
    watcher.last_condition = false;
    watcher.remaining = watcher.initial_count;
    auto &c = *watcher.coverage;
    const auto reason = aborted ? CoverageState::Aborted : CoverageState::Cleared;
    c.ClearAttempts(0, c.source_attempts, reason);
    for (size_t b = 0; b < c.attempts.size(); ++b) c.ClearAttempts(b + 1, c.attempts[b], reason);
    std::fill(c.last_conditions.begin(), c.last_conditions.end(), false);
}

void XTriggerEngine::ResetCoverage(XRegistrationHandle handle, bool counters)
{
    auto &w = CoverageWatcher(handle);
    ClearCoverageHistory(w);
    if (counters) {
        auto &s = w.coverage->snapshot;
        if (s.epoch == std::numeric_limits<uint64_t>::max()) throw std::overflow_error("coverage epoch overflow");
        std::fill(s.counters.begin(), s.counters.end(), 0);
        std::fill(s.diagnostics.begin(), s.diagnostics.end(), 0);
        s.incomplete = false;
        s.illegal_bins.clear(); s.illegal_values.clear(); s.illegal_ticks.clear(); ++s.epoch;
    }
}

XCoverageSnapshot XTriggerEngine::CoverageSnapshot(XRegistrationHandle handle, bool progress) const
{
    auto &c = *const_cast<XTriggerEngine *>(this)->CoverageWatcher(handle).coverage;
    auto snapshot = c.snapshot;
    snapshot.tick = clock->GetHalfTick();
    if (!snapshot.diagnostics.empty()) {
        for (size_t b = 0; b < c.bins.size(); ++b) if (c.owners[b] != b)
            std::copy_n(snapshot.diagnostics.begin() + (c.owners[b] + 1) * CoverageState::DiagnosticCount,
                        CoverageState::DiagnosticCount,
                        snapshot.diagnostics.begin() + (b + 1) * CoverageState::DiagnosticCount);
    }
    if (progress) {
        auto append = [&](size_t id, const std::vector<MatchState> &states) {
            for (const auto &s : states) snapshot.progress.insert(snapshot.progress.end(), {
                id, s.sequence_index, s.sequence_age, s.sequence_held, s.fsm_current_state});
        };
        append(0, c.source_attempts);
        for (size_t b = 0; b < c.attempts.size(); ++b) append(b + 1, c.attempts[c.owners[b]]);
    }
    return snapshot;
}

size_t XTriggerEngine::CoverageExecutionCount(XRegistrationHandle handle) const
{
    const auto &c = *const_cast<XTriggerEngine *>(this)->CoverageWatcher(handle).coverage;
    size_t count = 0;
    for (size_t b = 0; b < c.bins.size(); ++b)
        count += c.items[c.bins[b].item].pattern && c.owners[b] == b;
    return count;
}

void XTriggerEngine::SampleCoverage(Watcher &watcher)
{
    auto &c = *watcher.coverage;
    auto &delta = c.delta;
    std::fill(delta.begin(), delta.end(), 0);
    std::fill(c.matched.begin(), c.matched.end(), false);
    std::fill(c.normal_counts.begin(), c.normal_counts.end(), 0);
    const size_t offset = 2 + c.items.size() * 5;
    auto enabled = [&](int r) { return r < 0 || (expr_engine->IsKnown(r) && expr_engine->Eval(r)); };
    if (!enabled(c.gate)) {
        delta[1] = 1;
        std::fill(c.last_conditions.begin(), c.last_conditions.end(), false);
        for (size_t b = 0; b < c.attempts.size(); ++b) c.ClearAttempts(b + 1, c.attempts[b], CoverageState::Cleared);
    } else {
        delta[0] = 1;
        for (size_t i = 0; i < c.items.size(); ++i) {
            const auto &item = c.items[i];
            const size_t base = 2 + i * 5;
            const bool gate = enabled(item.gate);
            // U refreshes all native words; DataValid checks X/Z across the full width.
            const bool known = !item.signal || (item.signal->U(), item.signal->DataValid());
            if (!gate || !known) {
                delta[base + 1] = 1;
                if (gate && !known) delta[base + 4] = 1;
                for (size_t b = 0; b < c.bins.size(); ++b) if (c.bins[b].item == i) {
                    c.ClearAttempts(b + 1, c.attempts[b], CoverageState::Cleared);
                    c.last_conditions[b] = false;
                }
                continue;
            }
            delta[base] = 1;
            if (!item.dimensions.empty()) {
                uint64_t combinations = 1, hits = 0;
                for (auto d : item.dimensions) {
                    const auto n = c.normal_counts[d];
                    if (n && combinations > std::numeric_limits<uint64_t>::max() / n)
                        throw std::overflow_error("coverage cross product overflow");
                    combinations *= n;
                }
                for (size_t b = 0; b < c.bins.size(); ++b) {
                    const auto &bin = c.bins[b];
                    if (bin.item != i) continue;
                    bool match = true;
                    for (auto d : bin.dimensions) match = match && c.matched[d];
                    if (!match) continue;
                    delta[offset + b] = 1; ++hits;
                    if (bin.kind == 1) ++delta[base + 2];
                }
                delta[base + 3] = combinations ? combinations - hits : 1;
                continue;
            }
            if (item.pattern) {
                bool any = false;
                for (size_t b = 0; b < c.bins.size(); ++b) {
                    const auto &bin = c.bins[b];
                    if (bin.item != i) continue;
                    const auto owner = c.owners[b];
                    if (owner == b) {
                        if (bin.program_kind == 0) {
                            const bool known = expr_engine->IsKnown(bin.root);
                            const bool current = known && expr_engine->Eval(bin.root) != 0;
                            c.completions[b] = known && (bin.mode == XConditionMode::EachSample ? current :
                                bin.mode == XConditionMode::Enter ? current && !c.last_conditions[b] : current != c.last_conditions[b]);
                            if (known) c.last_conditions[b] = current;
                        } else {
                            auto &results = c.result_counts[b];
                            std::fill(results.begin(), results.end(), 0);
                            c.completions[b] = AdvanceCoverageAttempts(watcher, bin.steps, c.attempts[b],
                                b + 1, bin.overlap, bin.max_active, &c.programs[b], &results, &c.result_ids[b]);
                        }
                    }
                    size_t completed = c.completions[owner];
                    if (!bin.terminals.empty()) {
                        completed = 0;
                        const auto &ids = c.result_ids[owner];
                        for (size_t t = 0; t < ids.size(); ++t)
                            if (std::find(bin.terminals.begin(), bin.terminals.end(), ids[t]) != bin.terminals.end())
                                completed += c.result_counts[owner][t];
                    }
                    delta[offset + b] = completed;
                    any = any || completed;
                    if (bin.kind == 1 && completed) delta[base + 2] = 1;
                }
                if (!any) delta[base + 3] = 1;
                continue;
            }
            int priority = -1;
            for (size_t b = 0; b < c.bins.size(); ++b) {
                const auto &bin = c.bins[b];
                if (bin.item != i || bin.kind == 3) continue;
                bool hit = false;
                if (bin.steps.empty()) hit = enabled(bin.root);
                else {
                    hit = AdvanceCoverageAttempts(watcher, bin.steps, c.attempts[b],
                        b + 1, bin.overlap, bin.steps.size()) != 0;
                }
                c.matched[b] = hit;
                if (hit) priority = std::max(priority, static_cast<int>(bin.kind));
            }
            if (priority == 1) delta[base + 2] = 1;
            bool any = false;
            for (size_t b = 0; b < c.bins.size(); ++b) {
                const auto &bin = c.bins[b];
                if (bin.item != i) continue;
                const bool hit = priority < 0 ? bin.kind == 3 : c.matched[b] && static_cast<int>(bin.kind) == priority;
                c.matched[b] = hit && bin.kind == 0;
                if (c.matched[b]) ++c.normal_counts[i];
                if (hit) { delta[offset + b] = 1; any = true; }
            }
            if (!any) delta[base + 3] = 1;
        }
    }
    size_t illegal = 0;
    for (size_t b = 0; b < c.bins.size(); ++b) if (c.bins[b].kind == 2 && delta[offset + b]) ++illegal;
    if (c.snapshot.illegal_bins.size() + illegal > c.diagnostic_capacity)
        throw std::overflow_error("coverage diagnostic capacity exhausted");
    for (size_t i = 0; i < delta.size(); ++i)
        if (std::numeric_limits<uint64_t>::max() - c.snapshot.counters[i] < delta[i])
            throw std::overflow_error("coverage counter overflow");
    for (size_t i = 0; i < delta.size(); ++i) c.snapshot.counters[i] += delta[i];
    c.snapshot.tick = clock->GetHalfTick();
    for (size_t b = 0; b < c.bins.size(); ++b) if (c.bins[b].kind == 2 && delta[offset + b]) {
        c.snapshot.illegal_bins.push_back(b);
        c.snapshot.illegal_ticks.push_back(clock->GetHalfTick());
        auto *signal = c.items[c.bins[b].item].signal;
        c.snapshot.illegal_values.push_back(signal ? signal->String() : "0");
    }
    if (illegal && c.raise_illegal) throw std::runtime_error("coverage illegal bin");
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
