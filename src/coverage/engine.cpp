#include "xspcomm/detail/coverage/state.h"
#include "xspcomm/xexpr.h"

#include <algorithm>
#include <stdexcept>
#include <tuple>

namespace xspcomm {

namespace detail {

static bool SameCoverageExecution(const XCoverageBin &a, const XCoverageBin &b)
{
    if (std::tie(a.item, a.program.kind, a.program.root, a.program.mode, a.program.overlap, a.program.max_active,
                 a.program.state_count, a.program.start_state) !=
        std::tie(b.item, b.program.kind, b.program.root, b.program.mode, b.program.overlap, b.program.max_active,
                 b.program.state_count, b.program.start_state)) return false;
    if (a.program.steps.size() != b.program.steps.size() || a.program.transitions.size() != b.program.transitions.size()) return false;
    return std::equal(a.program.steps.begin(), a.program.steps.end(), b.program.steps.begin(), [](const auto &x, const auto &y) {
        return std::tie(x.kind, x.root, x.minimum, x.maximum, x.cycles) ==
               std::tie(y.kind, y.root, y.minimum, y.maximum, y.cycles);
    }) && std::equal(a.program.transitions.begin(), a.program.transitions.end(), b.program.transitions.begin(), [](const auto &x, const auto &y) {
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
        const auto &item = items[i];
        switch (item.kind) {
        case XCoverageItemKind::Value:
            if (!item.signal || !item.dimensions.empty())
                throw std::invalid_argument("value point requires only a signal");
            break;
        case XCoverageItemKind::Pattern:
            if (item.signal || !item.dimensions.empty())
                throw std::invalid_argument("pattern point cannot have a signal or dimensions");
            break;
        case XCoverageItemKind::Cross:
            if (item.signal || item.dimensions.empty())
                throw std::invalid_argument("cross requires only dimensions");
            break;
        default:
            throw std::invalid_argument("invalid coverage item kind");
        }
        for (auto d : items[i].dimensions)
            if (d >= i || items[d].kind != XCoverageItemKind::Value) throw std::invalid_argument("invalid cross point");
    }
    for (const auto &bin : bins) {
        if (bin.item >= items.size() || static_cast<unsigned>(bin.kind) > 3) throw std::invalid_argument("invalid coverage bin");
        check(bin.program.root);
        const bool pattern = items[bin.item].kind == XCoverageItemKind::Pattern;
        if (!pattern && !bin.program.steps.empty() && bin.program.kind != XTriggerProgramKind::Sequence)
            throw std::invalid_argument("transition bin requires a sequence program");
        if (pattern || bin.program.kind == XTriggerProgramKind::Sequence) {
            if (!ProgramValidator::Validate(bin.program, expr)) throw std::invalid_argument("invalid trigger program");
        } else if (bin.program.kind != XTriggerProgramKind::Expr || !bin.program.steps.empty() ||
                   !bin.program.transitions.empty() || bin.program.state_count || bin.program.start_state) {
            throw std::invalid_argument("invalid value/cross program");
        }
        if (pattern && bin.kind == XCoverageBinKind::Default)
            throw std::invalid_argument("pattern bin cannot be default");
        if (!pattern && !bin.terminals.empty())
            throw std::invalid_argument("terminals require a pattern FSM");
        if (bin.program.kind != XTriggerProgramKind::Fsm && !bin.terminals.empty())
            throw std::invalid_argument("terminals require an FSM program");
        for (auto terminal : bin.terminals)
            if (std::none_of(bin.program.transitions.begin(), bin.program.transitions.end(),
                [&](const auto &transition) { return transition.trigger && transition.terminal_id == terminal; }))
                throw std::invalid_argument("unknown FSM terminal");
        if (!pattern) for (const auto &step : bin.program.steps)
            if (step.kind != XSequenceStepKind::Wait && step.kind != XSequenceStepKind::Next)
                throw std::invalid_argument("coverage transition requires adjacent steps");
        const auto &dims = items[bin.item].dimensions;
        if (bin.dimensions.size() != dims.size()) throw std::invalid_argument("invalid cross tuple");
        for (size_t j = 0; j < dims.size(); ++j) {
            auto d = bin.dimensions[j];
            if (d >= bins.size() || bins[d].kind != XCoverageBinKind::Normal || bins[d].item != dims[j])
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
        if (bins[i].kind == XCoverageBinKind::Illegal) illegal_indices.push_back(i);
        if ((items[bins[i].item].kind == XCoverageItemKind::Pattern)) {
            for (size_t previous = 0; previous < i; ++previous) {
                if (owners[previous] == previous && SameCoverageExecution(bins[i], bins[previous])) {
                    owners[i] = previous;
                    break;
                }
            }
        }
        if (owners[i] != i) {
            this->bins[i].program.steps.clear();
            this->bins[i].program.transitions.clear();
            continue;
        }
        const auto &bin = this->bins[i];
        const auto program = PatternView::From(bin.program);
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

void CoverageState::RecordDiagnostics(size_t pattern, const MatchUpdate &update)
{
    Bump(pattern, Started, update.started);
    Bump(pattern, Completed, update.completed);
    Bump(pattern, Failed, update.failed);
    Bump(pattern, Expired, update.expired);
    Bump(pattern, PeakActive, update.peak_active);
}

inline size_t CoverageState::Advance(ExprEngine &expr, PatternMatcher &matcher,
                                    size_t pattern, bool overlap, size_t max_active)
{
    const auto update = matcher.Advance(expr, overlap, max_active);
    if (!snapshot.diagnostics.empty()) RecordDiagnostics(pattern, update);
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
        count += (items[bins[b].item].kind == XCoverageItemKind::Pattern) && owners[b] == b;
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

namespace {
inline bool Enabled(ExprEngine &expr, int root) {
    return root < 0 || (expr.IsKnown(root) && expr.Eval(root));
}
}

void CoverageState::SampleCrossPoint(size_t i, size_t offset)
{
    const auto &item = items[i];
    const size_t base = 2 + i * 5;
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
        if (bin.kind == XCoverageBinKind::Ignore) ++delta[base + 2];
    }
    delta[base + 3] = combinations ? combinations - hits : 1;
}

inline void CoverageState::SamplePatternPoint(ExprEngine &expr, size_t i, size_t offset)
{
    const size_t base = 2 + i * 5;
    bool any = false;
    for (size_t b = 0; b < bins.size(); ++b) {
        const auto &bin = bins[b];
        if (bin.item != i) continue;
        auto &matcher = matchers[owners[b]];
        if (owners[b] == b) {
            if (bin.program.kind == XTriggerProgramKind::Expr) matcher.AdvanceCondition(expr);
            else Advance(expr, matcher, b + 1, bin.program.overlap, bin.program.max_active);
        }
        const auto completed = matcher.Count(bin.terminals);
        delta[offset + b] = completed;
        any = any || completed;
        if (bin.kind == XCoverageBinKind::Ignore && completed) delta[base + 2] = 1;
    }
    if (!any) delta[base + 3] = 1;
}

void CoverageState::SampleValuePoint(ExprEngine &expr, size_t i, size_t offset)
{
    const size_t base = 2 + i * 5;
    int priority = -1;
    for (size_t b = 0; b < bins.size(); ++b) {
        const auto &bin = bins[b];
        if (bin.item != i || bin.kind == XCoverageBinKind::Default) continue;
        bool hit = false;
        if (bin.program.steps.empty()) hit = Enabled(expr, bin.program.root);
        else {
            hit = Advance(expr, matchers[b], b + 1, bin.program.overlap, bin.program.steps.size()) != 0;
        }
        matched[b] = hit;
        if (hit) priority = std::max(priority, static_cast<int>(bin.kind));
    }
    if (priority == 1) delta[base + 2] = 1;
    bool any = false;
    for (size_t b = 0; b < bins.size(); ++b) {
        const auto &bin = bins[b];
        if (bin.item != i) continue;
        const bool hit = priority < 0 ? bin.kind == XCoverageBinKind::Default : matched[b] && static_cast<int>(bin.kind) == priority;
        matched[b] = hit && bin.kind == XCoverageBinKind::Normal;
        if (matched[b]) ++normal_counts[i];
        if (hit) { delta[offset + b] = 1; any = true; }
    }
    if (!any) delta[base + 3] = 1;
}

void CoverageState::Sample(ExprEngine &expr, uint64_t tick)
{
    std::fill(delta.begin(), delta.end(), 0);
    std::fill(matched.begin(), matched.end(), false);
    std::fill(normal_counts.begin(), normal_counts.end(), 0);
    const size_t offset = 2 + items.size() * 5;
    if (!Enabled(expr, gate)) {
        delta[1] = 1;
        for (size_t b = 0; b < matchers.size(); ++b)
            if (owners[b] == b) ClearMatcher(b + 1, matchers[b], Cleared);
    } else {
        delta[0] = 1;
        for (size_t i = 0; i < items.size(); ++i) {
            const auto &item = items[i];
            const size_t base = 2 + i * 5;
            const bool gate = Enabled(expr, item.gate);
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
            switch (item.kind) {
            case XCoverageItemKind::Value: SampleValuePoint(expr, i, offset); break;
            case XCoverageItemKind::Pattern: SamplePatternPoint(expr, i, offset); break;
            case XCoverageItemKind::Cross: SampleCrossPoint(i, offset); break;
            }
        }
    }
    size_t illegal = 0;
    for (auto b : illegal_indices) if (delta[offset + b]) ++illegal;
    if (snapshot.illegal_bins.size() + illegal > diagnostic_capacity)
        throw std::overflow_error("coverage diagnostic capacity exhausted");
    for (size_t i = 0; i < delta.size(); ++i)
        if (std::numeric_limits<uint64_t>::max() - snapshot.counters[i] < delta[i])
            throw std::overflow_error("coverage counter overflow");
    for (size_t i = 0; i < delta.size(); ++i) snapshot.counters[i] += delta[i];
    snapshot.tick = tick;
    for (auto b : illegal_indices) if (delta[offset + b]) {
        snapshot.illegal_bins.push_back(b);
        snapshot.illegal_ticks.push_back(tick);
        auto *signal = items[bins[b].item].signal;
        snapshot.illegal_values.push_back(signal ? signal->String() : "0");
    }
    if (illegal && raise_illegal) throw std::runtime_error("coverage illegal bin");
}


} // namespace detail

} // namespace xspcomm
