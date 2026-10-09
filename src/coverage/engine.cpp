#include "state.h"
#include "xspcomm/xexpr.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace xspcomm {

namespace detail {

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

CoverageState::CoverageState(uint32_t generation, ExprEngine &expr, const PatternView &source,
    const std::vector<XCoverageItem> &items, const std::vector<XCoverageBin> &bins,
    int gate, int abort, bool raise_illegal, size_t diagnostic_capacity,
    bool overlap, size_t max_active, bool diagnostics)
    : source_matcher(source)
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
    this->items = items; this->bins = bins; this->gate = gate; this->abort = abort;
    this->raise_illegal = raise_illegal; this->diagnostic_capacity = diagnostic_capacity;
    this->overlap = overlap; this->max_active = max_active;
    // Grow only with observed starts, never reserve arbitrary max_active up front.
    if (diagnostics) snapshot.diagnostics.resize((1 + bins.size()) * CoverageState::DiagnosticCount);
    snapshot.generation = generation;
    snapshot.counters.resize(2 + items.size() * 5 + bins.size());
    delta.resize(snapshot.counters.size());
    matched.resize(bins.size()); normal_counts.resize(items.size());
    matchers.resize(bins.size());
    owners.resize(bins.size());
    for (size_t i = 0; i < bins.size(); ++i) {
        owners[i] = i;
        if (items[bins[i].item].pattern) {
            for (size_t previous = 0; previous < i; ++previous) {
                if (owners[previous] == previous && SameCoverageExecution(bins[i], bins[previous])) {
                    owners[i] = previous;
                    break;
                }
            }
        }
        if (owners[i] != i) {
            this->bins[i].steps.clear();
            this->bins[i].transitions.clear();
            continue;
        }
        const auto &bin = this->bins[i];
        PatternView program;
        if (items[bin.item].pattern && bin.program_kind == 2) {
            program.fsm = &bin.transitions;
            program.start_state = bin.start_state;
        } else if (!bin.steps.empty()) program.sequence = &bin.steps;
        else {
            program.root = bin.root;
            program.mode = bin.mode;
        }
        matchers[i] = PatternMatcher(program);
    }
    snapshot.illegal_bins.reserve(diagnostic_capacity);
    snapshot.illegal_values.reserve(diagnostic_capacity);
    snapshot.illegal_ticks.reserve(diagnostic_capacity);
}

void CoverageState::SamplePattern(ExprEngine &expr, uint64_t tick)
{
    const auto completed = Advance(expr, source_matcher, 0, overlap, max_active);
    for (size_t i = 0; i < completed; ++i) Sample(expr, tick);
}

inline size_t CoverageState::Advance(ExprEngine &expr, PatternMatcher &matcher,
                                    size_t pattern, bool overlap, size_t max_active)
{
    const auto update = matcher.Advance(expr, overlap, max_active);
    Bump(pattern, Started, update.started);
    Bump(pattern, Completed, update.completed);
    Bump(pattern, Failed, update.failed);
    Bump(pattern, Expired, update.expired);
    Bump(pattern, PeakActive, update.peak_active);
    if (update.capacity_exhausted) {
        snapshot.incomplete = true;
        throw std::overflow_error("coverage active pattern capacity exhausted");
    }
    return update.completed;
}

size_t CoverageState::ExecutionCount() const
{
    size_t count = 0;
    for (size_t b = 0; b < bins.size(); ++b)
        count += items[bins[b].item].pattern && owners[b] == b;
    return count;
}

void CoverageState::ClearHistory(bool aborted)
{
    const auto reason = aborted ? CoverageState::Aborted : CoverageState::Cleared;
    ClearMatcher(0, source_matcher, reason);
    for (size_t b = 0; b < matchers.size(); ++b)
        if (owners[b] == b) ClearMatcher(b + 1, matchers[b], reason);
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
    if (!snapshot.diagnostics.empty()) {
        for (size_t b = 0; b < bins.size(); ++b) if (owners[b] != b)
            std::copy_n(snapshot.diagnostics.begin() + (owners[b] + 1) * DiagnosticCount,
                        DiagnosticCount, snapshot.diagnostics.begin() + (b + 1) * DiagnosticCount);
    }
    if (progress) {
        auto append = [&](size_t id, const std::vector<PatternState> &states) {
            for (const auto &s : states) snapshot.progress.insert(snapshot.progress.end(), {
                id, s.sequence_index, s.sequence_age, s.sequence_held, s.fsm_current_state});
        };
        append(0, source_matcher.States());
        for (size_t b = 0; b < matchers.size(); ++b) append(b + 1, matchers[owners[b]].States());
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
        for (size_t b = 0; b < matchers.size(); ++b)
            if (owners[b] == b) ClearMatcher(b + 1, matchers[b], Cleared);
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
                for (size_t b = 0; b < bins.size(); ++b)
                    if (bins[b].item == i && owners[b] == b) ClearMatcher(b + 1, matchers[b], Cleared);
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
            if (item.pattern) {
                bool any = false;
                for (size_t b = 0; b < bins.size(); ++b) {
                    const auto &bin = bins[b];
                    if (bin.item != i) continue;
                    auto &matcher = matchers[owners[b]];
                    if (owners[b] == b) {
                        if (bin.program_kind == 0) matcher.Advance(expr);
                        else Advance(expr, matcher, b + 1, bin.overlap, bin.max_active);
                    }
                    const auto completed = matcher.Count(bin.terminals);
                    delta[offset + b] = completed;
                    any = any || completed;
                    if (bin.kind == 1 && completed) delta[base + 2] = 1;
                }
                if (!any) delta[base + 3] = 1;
                continue;
            }
            int priority = -1;
            for (size_t b = 0; b < bins.size(); ++b) {
                const auto &bin = bins[b];
                if (bin.item != i || bin.kind == 3) continue;
                bool hit = false;
                if (bin.steps.empty()) hit = enabled(bin.root);
                else {
                    hit = Advance(expr, matchers[b], b + 1, bin.overlap, bin.steps.size()) != 0;
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
