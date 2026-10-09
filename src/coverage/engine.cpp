#include "state.h"
#include "xspcomm/xexpr.h"

#include <algorithm>
#include <stdexcept>

namespace xspcomm {

namespace detail {

CoverageState::CoverageState(uint32_t generation, ExprEngine &expr, const PatternView &source,
    const std::vector<XCoverageItem> &items, const std::vector<XCoverageBin> &bins,
    int gate, int abort, bool raise_illegal, size_t diagnostic_capacity,
    bool overlap, size_t max_active, bool diagnostics)
{
    if (items.empty() || bins.empty() || diagnostic_capacity == 0)
        throw std::invalid_argument("empty coverage definition or diagnostic capacity");
    if (max_active == 0 || (!overlap && max_active != 1))
        throw std::invalid_argument("invalid coverage max_active");
    if (overlap && !source.sequence && !source.fsm)
        throw std::invalid_argument("coverage overlap requires Sequence or FSM");
    if (overlap && source.sequence != nullptr &&
        source.sequence->front().kind != XSequenceStepKind::Wait)
        throw std::invalid_argument("overlapping Sequence must start with Wait");
    auto check = [&](int r) {
        if (r != -1 && !expr.ValidRoot(r)) throw std::invalid_argument("invalid coverage expression");
    };
    check(gate); check(abort);
    for (size_t i = 0; i < items.size(); ++i) {
        check(items[i].gate);
        if (items[i].dimensions.empty() != (items[i].signal != nullptr))
            throw std::invalid_argument("coverage point requires a signal; cross must not have one");
        for (auto d : items[i].dimensions)
            if (d >= i || !items[d].dimensions.empty()) throw std::invalid_argument("invalid cross point");
    }
    for (const auto &bin : bins) {
        if (bin.item >= items.size() || bin.kind > 3) throw std::invalid_argument("invalid coverage bin");
        check(bin.root);
        for (const auto &step : bin.steps) {
            if (step.root < 0 || (step.kind != XSequenceStepKind::Wait && step.kind != XSequenceStepKind::Next))
                throw std::invalid_argument("coverage transition requires adjacent steps");
            check(step.root);
        }
        const auto &dims = items[bin.item].dimensions;
        if (bin.dimensions.size() != dims.size()) throw std::invalid_argument("invalid cross tuple");
        for (size_t j = 0; j < dims.size(); ++j) {
            auto d = bin.dimensions[j];
            if (d >= bins.size() || bins[d].kind != 0 || bins[d].item != dims[j])
                throw std::invalid_argument("invalid cross bin");
        }
    }
    this->items = items; this->bins = bins; this->gate = gate; this->abort = abort;
    this->raise_illegal = raise_illegal; this->diagnostic_capacity = diagnostic_capacity;
    this->overlap = overlap; this->max_active = max_active;
    // Grow only with observed starts, never reserve arbitrary max_active up front.
    if (diagnostics) snapshot.diagnostics.resize((1 + bins.size()) * CoverageState::DiagnosticCount);
    snapshot.generation = generation;
    snapshot.counters.resize(2 + items.size() * 5 + bins.size());
    delta.resize(snapshot.counters.size());
    matched.resize(bins.size()); normal_counts.resize(items.size());
    attempts.resize(bins.size());
    for (size_t i = 0; i < bins.size(); ++i) attempts[i].reserve(bins[i].steps.size());
    snapshot.illegal_bins.reserve(diagnostic_capacity);
    snapshot.illegal_values.reserve(diagnostic_capacity);
    snapshot.illegal_ticks.reserve(diagnostic_capacity);
}

void CoverageState::SamplePattern(ExprEngine &expr, const PatternView &source, uint64_t tick)
{
    const auto completed = AdvanceAttempts(expr, source, source_attempts, 0, overlap, max_active);
    for (size_t i = 0; i < completed; ++i) Sample(expr, tick);
}

size_t CoverageState::AdvanceAttempts(ExprEngine &expr, const PatternView &program,
    std::vector<PatternState> &attempts, size_t pattern, bool overlap, size_t max_active)
{
    const bool fsm = program.fsm != nullptr;
    const bool had_active = !attempts.empty();
    auto advance = [&](PatternState &state) {
        uint32_t terminal = 0;
        return fsm ? AdvanceFsm(*program.fsm, state, expr, terminal) : AdvanceSequence(*program.sequence, state, expr);
    };
    size_t completed = 0;
    size_t kept = 0;
    for (size_t i = 0; i < attempts.size(); ++i) {
        auto &state = attempts[i];
        const bool done = advance(state);
        const bool failed = fsm ? (!done && state.fsm_current_state == program.start_state)
                                : state.sequence_failed;
        if (done) { ++completed; Bump(pattern, CoverageState::Completed); }
        else if (failed) Bump(pattern, state.sequence_expired ? CoverageState::Expired : CoverageState::Failed);
        if (!done && !failed) {
            if (kept != i) attempts[kept] = state;
            ++kept;
        }
    }
    attempts.resize(kept);
    if (overlap || !had_active) {
        PatternState candidate;
        candidate.fsm_current_state = program.start_state;
        const bool done = advance(candidate);
        const bool active = fsm ? candidate.fsm_current_state != program.start_state
                                : candidate.sequence_index || candidate.sequence_age || candidate.sequence_held;
        if (done || active) {
            if (attempts.size() >= max_active) {
                snapshot.incomplete = true;
                throw std::overflow_error("coverage active pattern capacity exhausted");
            }
            Bump(pattern, CoverageState::Started);
            Bump(pattern, CoverageState::PeakActive, attempts.size() + 1);
            if (done) { ++completed; Bump(pattern, CoverageState::Completed); }
            else attempts.push_back(candidate);
        }
    }
    return completed;
}

void CoverageState::ClearHistory(bool aborted)
{
    const auto reason = aborted ? CoverageState::Aborted : CoverageState::Cleared;
    ClearAttempts(0, source_attempts, reason);
    for (size_t b = 0; b < attempts.size(); ++b) ClearAttempts(b + 1, attempts[b], reason);
}

void CoverageState::ResetCounters()
{
    if (snapshot.epoch == std::numeric_limits<uint64_t>::max())
        throw std::overflow_error("coverage epoch overflow");
    std::fill(snapshot.counters.begin(), snapshot.counters.end(), 0);
    std::fill(snapshot.diagnostics.begin(), snapshot.diagnostics.end(), 0);
    snapshot.incomplete = false;
    snapshot.illegal_bins.clear();
    snapshot.illegal_values.clear();
    snapshot.illegal_ticks.clear();
    ++snapshot.epoch;
}

XCoverageSnapshot CoverageState::Snapshot(uint64_t tick, bool progress) const
{
    auto snapshot = this->snapshot;
    snapshot.tick = tick;
    if (progress) {
        auto append = [&](size_t id, const std::vector<PatternState> &states) {
            for (const auto &s : states) snapshot.progress.insert(snapshot.progress.end(), {
                id, s.sequence_index, s.sequence_age, s.sequence_held, s.fsm_current_state});
        };
        append(0, source_attempts);
        for (size_t b = 0; b < attempts.size(); ++b) append(b + 1, attempts[b]);
    }
    return snapshot;
}

void CoverageState::Sample(ExprEngine &expr, uint64_t tick)
{
    std::fill(delta.begin(), delta.end(), 0);
    std::fill(matched.begin(), matched.end(), false);
    std::fill(normal_counts.begin(), normal_counts.end(), 0);
    const size_t offset = 2 + items.size() * 5;
    auto enabled = [&](int r) { return r < 0 || (expr.IsKnown(r) && expr.Eval(r)); };
    if (!enabled(gate)) {
        delta[1] = 1;
        for (size_t b = 0; b < attempts.size(); ++b) ClearAttempts(b + 1, attempts[b], CoverageState::Cleared);
    } else {
        delta[0] = 1;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto &item = items[i];
            const size_t base = 2 + i * 5;
            const bool gate = enabled(item.gate);
            // U refreshes all native words; DataValid checks X/Z across the full width.
            const bool known = !item.signal || (item.signal->U(), item.signal->DataValid());
            if (!gate || !known) {
                delta[base + 1] = 1;
                if (gate && !known) delta[base + 4] = 1;
                for (size_t b = 0; b < bins.size(); ++b) if (bins[b].item == i)
                    ClearAttempts(b + 1, attempts[b], CoverageState::Cleared);
                continue;
            }
            delta[base] = 1;
            if (!item.dimensions.empty()) {
                uint64_t combinations = 1, hits = 0;
                for (auto d : item.dimensions) {
                    const auto n = normal_counts[d];
                    if (n && combinations > std::numeric_limits<uint64_t>::max() / n)
                        throw std::overflow_error("coverage cross product overflow");
                    combinations *= n;
                }
                for (size_t b = 0; b < bins.size(); ++b) {
                    const auto &bin = bins[b];
                    if (bin.item != i) continue;
                    bool match = true;
                    for (auto d : bin.dimensions) match = match && matched[d];
                    if (!match) continue;
                    delta[offset + b] = 1; ++hits;
                    if (bin.kind == 1) ++delta[base + 2];
                }
                delta[base + 3] = combinations ? combinations - hits : 1;
                continue;
            }
            int priority = -1;
            for (size_t b = 0; b < bins.size(); ++b) {
                const auto &bin = bins[b];
                if (bin.item != i || bin.kind == 3) continue;
                bool hit = false;
                if (bin.steps.empty()) hit = enabled(bin.root);
                else {
                    hit = AdvanceAttempts(expr, {&bin.steps}, attempts[b],
                        b + 1, bin.overlap, bin.steps.size()) != 0;
                }
                matched[b] = hit;
                if (hit) priority = std::max(priority, static_cast<int>(bin.kind));
            }
            if (priority == 1) delta[base + 2] = 1;
            bool any = false;
            for (size_t b = 0; b < bins.size(); ++b) {
                const auto &bin = bins[b];
                if (bin.item != i) continue;
                const bool hit = priority < 0 ? bin.kind == 3 : matched[b] && static_cast<int>(bin.kind) == priority;
                matched[b] = hit && bin.kind == 0;
                if (matched[b]) ++normal_counts[i];
                if (hit) { delta[offset + b] = 1; any = true; }
            }
            if (!any) delta[base + 3] = 1;
        }
    }
    size_t illegal = 0;
    for (size_t b = 0; b < bins.size(); ++b) if (bins[b].kind == 2 && delta[offset + b]) ++illegal;
    if (snapshot.illegal_bins.size() + illegal > diagnostic_capacity)
        throw std::overflow_error("coverage diagnostic capacity exhausted");
    for (size_t i = 0; i < delta.size(); ++i)
        if (std::numeric_limits<uint64_t>::max() - snapshot.counters[i] < delta[i])
            throw std::overflow_error("coverage counter overflow");
    for (size_t i = 0; i < delta.size(); ++i) snapshot.counters[i] += delta[i];
    snapshot.tick = tick;
    for (size_t b = 0; b < bins.size(); ++b) if (bins[b].kind == 2 && delta[offset + b]) {
        snapshot.illegal_bins.push_back(b);
        snapshot.illegal_ticks.push_back(tick);
        auto *signal = items[bins[b].item].signal;
        snapshot.illegal_values.push_back(signal ? signal->String() : "0");
    }
    if (illegal && raise_illegal) throw std::runtime_error("coverage illegal bin");
}


} // namespace detail

} // namespace xspcomm
