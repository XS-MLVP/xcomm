#ifndef XSPCOMM_TRIGGER_TYPES_H
#define XSPCOMM_TRIGGER_TYPES_H

#include "xspcomm/xclock.h"
#include <cstdint>
#include <limits>
#include <vector>

namespace xspcomm {

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

} // namespace xspcomm

#endif
