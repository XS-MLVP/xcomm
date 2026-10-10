#ifndef XSPCOMM_XENGINE_H
#define XSPCOMM_XENGINE_H

#include "xspcomm/runtime/types.h"
#include "xspcomm/xcoverage.h"

#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>

namespace xspcomm {

class XClock;
class XData;
class ExprEngine;

// Shared sampling runtime. Trigger matches and coverage statistics are clients
// of the same clock/phase schedule; the matching kernel has no coverage policy.
class XEngine {
    struct Watcher;

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
    bool EvaluateCoverage(Watcher &watcher);
    void CheckCoverageHandle(XRegistrationHandle handle) const;

public:
    explicit XEngine(XClock *clock, size_t capacity = 1024);
    explicit XEngine(XClock &clock, size_t capacity = 1024);
    ~XEngine();

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
    static uint32_t CoverageVersion() { return 5; }
    void AttachCoverage(XRegistrationHandle handle,
                        const std::vector<XCoverageItem> &items,
                        const std::vector<XCoverageBin> &bins,
                        int gate = -1, int abort = -1,
                        bool raise_illegal = true, size_t diagnostic_capacity = 1024,
                        bool overlap = false, size_t max_active = 1, bool diagnostics = false);
    XCoverageSnapshot CoverageSnapshot(XRegistrationHandle handle, bool progress = false) const;
    void ResetCoverage(XRegistrationHandle handle, bool counters = true);
    size_t CoverageExecutionCount(XRegistrationHandle handle) const;
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
