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

    struct Watcher {
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
        size_t sequence_index = 0;
        uint64_t sequence_age = 0;
        uint64_t sequence_held = 0;
        std::vector<XFsmTransition> fsm_transitions;
        uint32_t fsm_state_count = 0;
        uint32_t fsm_start_state = 0;
        uint32_t fsm_current_state = 0;
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
    bool AdvanceSequence(Watcher &watcher);

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
