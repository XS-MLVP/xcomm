#ifndef __xspcomm_xtrigger_h__
#define __xspcomm_xtrigger_h__

#include "xspcomm/xclock.h"
#include "xspcomm/xdata.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

namespace xspcomm {

class ExprEngine;

enum class XHitKind : uint8_t {
    ClockFall = 0,
    ClockRise = 1,
    ClockCycles = 2,
    Value = 3,
    Condition = 4,
    Fsm = 5,
    ValueChange = 6,
    DriveStable = 7,
};

enum class XStopReason : uint8_t {
    RunLimit = 0,
    TriggerHit = 1,
    BackendStop = 2,
    QuantumExpired = 3,
    EdgeBarrier = 4,
    UserPause = 5,
    SimulationClose = 6,
    CallbackError = 7,
    BackendError = 8,
};

enum class XConditionMode : uint8_t {
    Enter = 0,
    EachSample = 1,
    Change = 2,
};

struct XRegistrationHandle {
    uint32_t slot = std::numeric_limits<uint32_t>::max();
    uint32_t generation = 0;

    bool IsValid() const {
        return slot != std::numeric_limits<uint32_t>::max();
    }
};

struct XBackendHit {
    uint64_t event_id = 0;
    uint64_t tick = 0;
    uint64_t source_id = 0;
    uint64_t value = 0;
    uint64_t x_mask = 0;
    uint32_t slot = 0;
    uint32_t generation = 0;
    XHitKind kind = XHitKind::ClockFall;
    XPhase phase = XPhase::FallingStable;
    uint16_t flags = 0;
};

struct XRunResult {
    uint64_t advanced_ticks = 0;
    XPhase stopped_phase = XPhase::RisingStable;
    XStopReason stop_reason = XStopReason::RunLimit;
    std::vector<XBackendHit> hits;

    bool IsPhaseBarrier() const {
        return advanced_ticks != 0 || stopped_phase == XPhase::DriveStable;
    }
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

// Coverage uses numeric IDs in the hot path. Human names live in the client.
struct XCoverageItem {
    int gate = -1;
    bool pattern = false; // event point; no synthetic XData source
    XData *signal = nullptr; // direct value source; null for a cross or pattern point
    std::vector<unsigned int> dimensions; // point IDs; empty for a point
};

struct XCoverageBin {
    uint32_t item = 0;
    uint32_t kind = 0; // normal / ignore / illegal / default
    int root = -1;
    std::vector<XSequenceStep> steps;
    bool overlap = true;
    uint32_t program_kind = 0; // Expr=0, Sequence=1, FSM=2 (pattern points)
    XConditionMode mode = XConditionMode::Enter;
    uint32_t max_active = 1;
    uint32_t state_count = 0, start_state = 0;
    std::vector<XFsmTransition> transitions;
    std::vector<unsigned int> terminals; // empty selects all
    std::vector<unsigned int> dimensions; // normal bin IDs for a cross tuple
};

struct XCoverageSnapshot {
    uint32_t generation = 0;
    uint64_t epoch = 0;
    uint64_t tick = 0;
    // group samples/gated; samples/gated/ignored/unmatched/unknown per item;
    // then bin counts in declaration order.
    std::vector<unsigned long long> counters;
    std::vector<unsigned int> illegal_bins;
    std::vector<std::string> illegal_values; // full known value in hexadecimal, sampled at the hit
    std::vector<unsigned long long> illegal_ticks;
    // started/completed/failed/expired/aborted/cleared/peak_active for source,
    // then each bin. Empty when summary diagnostics are disabled.
    std::vector<unsigned long long> diagnostics;
    // Optional rows: pattern ID (0=source, bin+1), step, age, held, FSM state.
    std::vector<unsigned long long> progress;
    bool incomplete = false;
};

class XTriggerEngine {
    enum class WatcherKind : uint8_t {
        Edge,
        ClockCycles,
        ValueEq,
        ValueChange,
        Sample,
        Expr,
        Sequence,
        Fsm,
    };

    struct CoverageState;
    struct MatchState {
        bool sequence_failed = false;
        bool sequence_expired = false;
        size_t sequence_index = 0;
        uint64_t sequence_age = 0;
        uint64_t sequence_held = 0;
        uint32_t fsm_current_state = 0;
    };
    struct Watcher : MatchState {
        uint32_t generation = 0;
        bool occupied = false;
        bool armed = false;
        WatcherKind kind = WatcherKind::Edge;
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
        XConditionMode condition_mode = XConditionMode::Enter;
        bool last_condition = false;
        int expr_root = -1;
        std::vector<XSequenceStep> sequence_steps;
        std::vector<XFsmTransition> fsm_transitions;
        uint32_t fsm_state_count = 0;
        uint32_t fsm_start_state = 0;
        std::shared_ptr<CoverageState> coverage;
    };

    XClock *clock = nullptr;
    size_t capacity = 0;
    uint64_t next_event_id = 1;
    std::vector<Watcher> watchers;
    std::vector<uint32_t> free_slots;
    std::vector<XBackendHit> hit_buffer;
    std::unique_ptr<ExprEngine> expr_engine;
    XPhase evaluation_phase = XPhase::RisingStable;

    XRegistrationHandle Allocate();
    void EvaluatePhase(XPhase phase);
    bool HasArmedPhase(XPhase phase) const;
    void AppendHit(uint32_t slot, Watcher &watcher, XHitKind kind,
                   uint64_t value, uint64_t event_id = 0,
                   uint64_t x_mask = 0);
    bool AdvanceSequence(const std::vector<XSequenceStep> &steps, MatchState &state);
    bool AdvanceFsm(const Watcher &program, MatchState &state, uint32_t &terminal);
    size_t AdvanceCoverageAttempts(Watcher &watcher, const std::vector<XSequenceStep> &steps,
        std::vector<MatchState> &attempts, size_t pattern, bool overlap, size_t max_active,
        const Watcher *program = nullptr, const std::vector<unsigned int> *terminals = nullptr);
    void SampleCoverage(Watcher &watcher);
    void ClearCoverageHistory(Watcher &watcher, bool aborted = false);
    Watcher &CoverageWatcher(XRegistrationHandle handle);

public:
    explicit XTriggerEngine(XClock *clock, size_t capacity = 1024);
    explicit XTriggerEngine(XClock &clock, size_t capacity = 1024)
        : XTriggerEngine(&clock, capacity) {}
    ~XTriggerEngine();

    XRegistrationHandle ArmEdge(XPhase phase, uint64_t source_id = 0);
    XRegistrationHandle ArmClockCycles(
        uint64_t cycles, XPhase phase = XPhase::RisingStable,
        uint64_t source_id = 0);
    XRegistrationHandle ArmValueEq(
        XData *signal, uint64_t expected,
        XPhase phase = XPhase::RisingStable,
        XConditionMode mode = XConditionMode::Enter,
        uint64_t source_id = 0);
    XRegistrationHandle ArmValueEqBytes(
        XData *signal, std::vector<unsigned char> &expected,
        XPhase phase, XConditionMode mode, uint64_t source_id);
    XRegistrationHandle ArmValueChange(
        XData *signal, XPhase phase = XPhase::RisingStable,
        uint64_t source_id = 0);
    XRegistrationHandle ArmSample(
        XPhase phase = XPhase::RisingStable, uint64_t source_id = 0);
    int ExprNewConst(uint64_t value);
    int ExprNewSignal(XData *signal);
    int ExprNewUnary(int op, int child);
    int ExprNewBinary(int op, int lhs, int rhs);
    int ExprNewCompare(int op, int lhs, int rhs);
    int ExprNewCompareSigSig(int op, XData *lhs, XData *rhs);
    int ExprNewCompareSigConstBytes(
        int op, XData *lhs, std::vector<unsigned char> &rhs);
    int ExprNewCompareConstBytesSig(
        int op, std::vector<unsigned char> &lhs, XData *rhs);
    int ExprNewMaskedCompareSigConstBytes(
        XData *lhs, std::vector<unsigned char> &value, std::vector<unsigned char> &mask);
    XRegistrationHandle ArmExpr(
        int root, XPhase phase = XPhase::RisingStable,
        XConditionMode mode = XConditionMode::Enter,
        uint64_t source_id = 0);
    XRegistrationHandle ArmSequence(
        const std::vector<XSequenceStep> &steps,
        XPhase phase = XPhase::RisingStable, uint64_t source_id = 0);
    XRegistrationHandle ArmFsm(
        uint32_t state_count, uint32_t start_state,
        const std::vector<XFsmTransition> &transitions,
        XPhase phase = XPhase::RisingStable, uint64_t source_id = 0);
    static uint32_t CoverageVersion() { return 4; }
    void AttachCoverage(XRegistrationHandle handle,
                        const std::vector<XCoverageItem> &items,
                        const std::vector<XCoverageBin> &bins,
                        int gate = -1, int abort = -1,
                        bool raise_illegal = true, size_t diagnostic_capacity = 1024,
                        bool overlap = false, size_t max_active = 1, bool diagnostics = false);
    XCoverageSnapshot CoverageSnapshot(XRegistrationHandle handle, bool progress = false) const;
    void ResetCoverage(XRegistrationHandle handle, bool counters = true);
    bool Disarm(XRegistrationHandle handle);
    bool Rearm(XRegistrationHandle handle);
    bool RearmSample(XRegistrationHandle handle);
    XRunResult RunUntil(
        uint64_t max_half_ticks, uint64_t max_wall_time_ns = 0,
        uint32_t budget_check_interval = 64);
    XRunResult SamplePhase(XPhase phase);
    size_t ActiveCount() const;
    size_t Capacity() const { return capacity; }
    void ClearExecutionState();
    void Clear();
};

} // namespace xspcomm

#endif
