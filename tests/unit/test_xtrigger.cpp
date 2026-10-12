#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xtrigger.h"
#include "xspcomm/xexpr.h"

using namespace xspcomm;

TEST_CASE("XTriggerEngine broadcasts one edge occurrence", "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock, 8);
    engine.AcquireClock();
    const auto first = engine.ArmEdge(XPhase::FallingStable);
    const auto second = engine.ArmEdge(XPhase::FallingStable);

    const auto result = engine.RunUntil(8);

    REQUIRE(result.stop_reason == XStopReason::EdgeBarrier);
    REQUIRE(result.advanced_ticks == 1);
    REQUIRE(result.hits.size() == 2);
    REQUIRE(result.hits[0].event_id == result.hits[1].event_id);
    REQUIRE(result.hits[0].phase == XPhase::FallingStable);
    REQUIRE(engine.Disarm(first));
    REQUIRE(engine.Disarm(second));
}

TEST_CASE("XTriggerEngine counts selected phases", "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmClockCycles(3);

    const auto result = engine.RunUntil(20);

    REQUIRE(result.stop_reason == XStopReason::TriggerHit);
    REQUIRE(result.advanced_ticks == 6);
    REQUIRE(result.hits.size() == 1);
    REQUIRE(result.hits[0].kind == XHitKind::ClockCycles);
    REQUIRE(result.hits[0].tick == 6);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine evaluates value equality at a stable phase",
          "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XData ready(1, XData::InOut);
    ready = 1;
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmValueEq(
        &ready, 1, XPhase::RisingStable);

    const auto result = engine.RunUntil(4);

    REQUIRE(result.stop_reason == XStopReason::TriggerHit);
    REQUIRE(result.hits.size() == 1);
    REQUIRE(result.hits[0].kind == XHitKind::Value);
    REQUIRE(result.hits[0].value == 1);
    REQUIRE(result.hits[0].tick == 2);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine rejects stale generations", "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock, 1);
    engine.AcquireClock();
    const auto old_handle = engine.ArmEdge(XPhase::FallingStable);
    REQUIRE(engine.Disarm(old_handle));

    const auto new_handle = engine.ArmEdge(XPhase::FallingStable);
    REQUIRE(new_handle.slot == old_handle.slot);
    REQUIRE(new_handle.generation > old_handle.generation);
    REQUIRE_FALSE(engine.Disarm(old_handle));
    REQUIRE(engine.Disarm(new_handle));
}

TEST_CASE("XTriggerEngine runtime reset preserves stale handle safety",
          "[xtrigger]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock, 1);
    engine.AcquireClock();
    auto stale = engine.ArmEdge(XPhase::RisingStable);
    REQUIRE(engine.Disarm(stale));

    engine.ClearExecutionState();
    auto current = engine.ArmEdge(XPhase::RisingStable);

    REQUIRE(current.slot == stale.slot);
    REQUIRE(current.generation > stale.generation);
    REQUIRE_FALSE(engine.Disarm(stale));
    REQUIRE(engine.Disarm(current));
}

TEST_CASE("XTriggerEngine evaluates ExprEngine roots", "[xtrigger]") {
    XData valid(1, XData::InOut);
    XData ready(1, XData::InOut);
    XData flush(1, XData::InOut);
    valid = 1;
    ready = 1;
    flush = 0;
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();

    const int valid_node = engine.ExprNewSignal(&valid);
    const int ready_node = engine.ExprNewSignal(&ready);
    const int flush_node = engine.ExprNewSignal(&flush);
    const int both = engine.ExprNewBinary(
        static_cast<int>(ExprOp::LAND), valid_node, ready_node);
    const int no_flush = engine.ExprNewUnary(
        static_cast<int>(ExprOp::LNOT), flush_node);
    const int root = engine.ExprNewBinary(
        static_cast<int>(ExprOp::LAND), both, no_flush);
    const auto handle = engine.ArmExpr(root);

    const auto result = engine.RunUntil(8);

    REQUIRE(result.stop_reason == XStopReason::TriggerHit);
    REQUIRE(result.advanced_ticks == 2);
    REQUIRE(result.hits.size() == 1);
    REQUIRE(result.hits[0].kind == XHitKind::Condition);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine runs non-overlapping sequence state", "[xtrigger]") {
    XData req(1, XData::InOut);
    XData ack(1, XData::InOut);
    XData valid(1, XData::InOut);
    int rising_count = 0;
    XClock clock([&](bool rising){
        if (!rising) return 0;
        rising_count += 1;
        if (rising_count == 1) req = 1;
        if (rising_count == 2) ack = 1;
        if (rising_count >= 3) valid = 1;
        return 0;
    });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const int req_root = engine.ExprNewSignal(&req);
    const int ack_root = engine.ExprNewSignal(&ack);
    const int valid_root = engine.ExprNewSignal(&valid);
    const std::vector<XSequenceStep> steps = {
        {XSequenceStepKind::Wait, req_root, 0, 0, 0},
        {XSequenceStepKind::Within, ack_root, 1, 4, 0},
        {XSequenceStepKind::Hold, valid_root, 0, 0, 2},
    };
    const auto handle = engine.ArmSequence(steps);

    const auto result = engine.RunUntil(16);

    REQUIRE(result.stop_reason == XStopReason::TriggerHit);
    REQUIRE(result.advanced_ticks == 8);
    REQUIRE(result.hits.size() == 1);
    REQUIRE(result.hits[0].kind == XHitKind::Fsm);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine rearms Python sampling without changing handle",
          "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmSample(XPhase::RisingStable);

    const auto first = engine.RunUntil(8);
    REQUIRE(first.hits.size() == 1);
    REQUIRE(first.hits[0].tick == 2);
    REQUIRE(engine.RearmSample(handle));

    const auto second = engine.RunUntil(8);
    REQUIRE(second.hits.size() == 1);
    REQUIRE(second.hits[0].tick == 4);
    REQUIRE(second.hits[0].slot == handle.slot);
    REQUIRE(second.hits[0].generation == handle.generation);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine rearms periodic cycle watchers", "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmClockCycles(2);

    const auto first = engine.RunUntil(8);
    REQUIRE(first.hits.size() == 1);
    REQUIRE(first.hits[0].tick == 4);
    REQUIRE(engine.Rearm(handle));

    const auto second = engine.RunUntil(8);
    REQUIRE(second.hits.size() == 1);
    REQUIRE(second.hits[0].tick == 8);
    REQUIRE(second.hits[0].generation == handle.generation);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine runs structured branching FSM", "[xtrigger]") {
    XData req(1, XData::InOut);
    XData ack(1, XData::InOut);
    int rising_count = 0;
    XClock clock([&](bool rising){
        if (!rising) return 0;
        rising_count += 1;
        if (rising_count == 1) req = 1;
        if (rising_count == 2) ack = 1;
        return 0;
    });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const int req_root = engine.ExprNewSignal(&req);
    const int ack_root = engine.ExprNewSignal(&ack);
    const std::vector<XFsmTransition> transitions = {
        {0, req_root, 1, 0, false},
        {1, ack_root, 0, 7, true},
    };
    const auto handle = engine.ArmFsm(2, 0, transitions);

    const auto result = engine.RunUntil(8);

    REQUIRE(result.stop_reason == XStopReason::TriggerHit);
    REQUIRE(result.hits.size() == 1);
    REQUIRE(result.hits[0].tick == 4);
    REQUIRE(result.hits[0].kind == XHitKind::Fsm);
    REQUIRE(result.hits[0].value == 7);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine samples value changes at selected phase",
          "[xtrigger]") {
    XData data(8, XData::InOut);
    XClock clock([&](bool rising){
        if (rising) data = 9;
        return 0;
    });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmValueChange(
        &data, XPhase::RisingStable);

    const auto result = engine.RunUntil(4);

    REQUIRE(result.hits.size() == 1);
    REQUIRE(result.hits[0].kind == XHitKind::ValueChange);
    REQUIRE(result.hits[0].value == 9);
    REQUIRE(result.hits[0].tick == 2);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine preserves four-state value changes", "[xtrigger]") {
    XData data(4, XData::InOut);
    XClock clock([&](bool rising){
        if (rising) data[1] = "x";
        return 0;
    });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmValueChange(
        &data, XPhase::RisingStable);

    const auto result = engine.RunUntil(4);

    REQUIRE(result.hits.size() == 1);
    REQUIRE(result.hits[0].value == 2);
    REQUIRE(result.hits[0].x_mask == 2);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine does not match unknown equality", "[xtrigger]") {
    XData data(4, XData::InOut);
    XClock clock([&](bool rising){
        if (rising) data[0] = "x";
        return 0;
    });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmValueEq(
        &data, 1, XPhase::RisingStable);

    const auto result = engine.RunUntil(4);

    REQUIRE(result.hits.empty());
    REQUIRE(result.stop_reason == XStopReason::RunLimit);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine supports signals wider than uint64", "[xtrigger]") {
    XData data(128, XData::InOut);
    std::vector<unsigned char> expected(16, 0);
    expected[0] = 0xef;
    expected[12] = 0x10;
    data.SetVU8(expected);
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();

    const auto value = engine.ArmValueEqBytes(
        &data, expected, XPhase::RisingStable, XConditionMode::Enter, 0);
    REQUIRE(value.IsValid());
    REQUIRE(engine.RunUntil(2).hits.size() == 1);
    REQUIRE(engine.Disarm(value));

    const int root = engine.ExprNewCompareSigConstBytes(
        static_cast<int>(ExprOp::EQ), &data, expected);
    REQUIRE(root >= 0);
    const auto expr = engine.ArmExpr(root);
    REQUIRE(expr.IsValid());
    REQUIRE(engine.RunUntil(2).hits.size() == 1);
    REQUIRE(engine.Disarm(expr));

    const auto changed = engine.ArmValueChange(&data);
    REQUIRE(changed.IsValid());
    expected[15] = 0x80;
    data.SetVU8(expected);
    REQUIRE(engine.RunUntil(2).hits.size() == 1);
    REQUIRE(engine.Disarm(changed));

    data[100] = "x";
    const auto unknown = engine.ArmValueEqBytes(
        &data, expected, XPhase::RisingStable, XConditionMode::Enter, 0);
    REQUIRE(unknown.IsValid());
    REQUIRE(engine.RunUntil(2).hits.empty());
    REQUIRE(engine.Disarm(unknown));

    XData high(128, XData::InOut);
    XData low(128, XData::InOut);
    std::vector<unsigned char> high_bytes(16, 0);
    std::vector<unsigned char> low_bytes(16, 0);
    high_bytes[12] = 1;
    low_bytes[0] = 0xff;
    low_bytes[8] = 1;
    high.SetVU8(high_bytes);
    low.SetVU8(low_bytes);
    REQUIRE(high.Comp(low, 2));
    REQUIRE_FALSE(high.Comp(low, 1));
}

TEST_CASE("XTriggerEngine condition enter state survives rearm", "[xtrigger]") {
    XData data(1, XData::InOut);
    data = 1;
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmValueEq(
        &data, 1, XPhase::RisingStable, XConditionMode::Enter);

    REQUIRE(engine.RunUntil(2).hits.size() == 1);
    REQUIRE(engine.Rearm(handle));
    REQUIRE(engine.RunUntil(2).hits.empty());
    data = 0;
    REQUIRE(engine.RunUntil(2).hits.empty());
    data = 1;
    REQUIRE(engine.RunUntil(2).hits.size() == 1);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine condition change emits both transitions", "[xtrigger]") {
    XData data(1, XData::InOut);
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmValueEq(
        &data, 1, XPhase::RisingStable, XConditionMode::Change);

    REQUIRE(engine.RunUntil(2).hits.empty());
    data = 1;
    const auto entered = engine.RunUntil(2);
    REQUIRE(entered.hits.size() == 1);
    REQUIRE(entered.hits[0].value == 1);
    REQUIRE(engine.Rearm(handle));
    data = 0;
    const auto left = engine.RunUntil(2);
    REQUIRE(left.hits.size() == 1);
    REQUIRE(left.hits[0].value == 0);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine yields on wall clock quantum", "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const auto handle = engine.ArmClockCycles(1000);

    const auto result = engine.RunUntil(1000, 1, 1);

    REQUIRE(result.stop_reason == XStopReason::QuantumExpired);
    REQUIRE(result.advanced_ticks >= 1);
    REQUIRE(result.advanced_ticks < 1000);
    REQUIRE(result.hits.empty());
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("XTriggerEngine samples DriveStable without advancing time",
          "[xtrigger]") {
    XData valid(1, XData::InOut);
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    const int root = engine.ExprNewSignal(&valid);
    const auto handle = engine.ArmExpr(
        root, XPhase::DriveStable, XConditionMode::EachSample);

    const auto falling = engine.RunUntil(8);
    REQUIRE(falling.stop_reason == XStopReason::EdgeBarrier);
    REQUIRE(falling.advanced_ticks == 1);
    REQUIRE(falling.stopped_phase == XPhase::FallingStable);
    REQUIRE(falling.hits.empty());

    valid = 1;
    const auto sampled = engine.SamplePhase(XPhase::DriveStable);
    REQUIRE(sampled.advanced_ticks == 0);
    REQUIRE(sampled.stopped_phase == XPhase::DriveStable);
    REQUIRE(sampled.IsPhaseBarrier());
    REQUIRE(sampled.hits.size() == 1);
    REQUIRE(sampled.hits[0].phase == XPhase::DriveStable);
    REQUIRE(sampled.hits[0].tick == 1);
    REQUIRE(engine.Disarm(handle));
}


TEST_CASE("XEngine explicitly leases a clock until release", "[xtrigger][ownership]") {
    XClock clock([](bool) { return 0; });
    XEngine first(clock), second(clock);
    REQUIRE_FALSE(clock.IsControlled());
    clock.Step(); // Construction leaves initialization under manual control.
    REQUIRE_THROWS_AS(first.ArmEdge(XPhase::RisingStable), std::logic_error);
    REQUIRE_THROWS_AS(first.RunUntil(2), std::logic_error);

    first.AcquireClock();
    first.AcquireClock(); // Idempotent for the same engine.
    REQUIRE(clock.IsControlled());
    REQUIRE_THROWS_AS(second.AcquireClock(), std::logic_error);
    second.ReleaseClock(); // Never releases another engine's lease.
    REQUIRE(clock.IsControlled());
    XClock idle([](bool) { return 0; });
    REQUIRE_THROWS_AS(XClock(clock), std::logic_error);
    REQUIRE_THROWS_AS(idle = clock, std::logic_error);
    REQUIRE_THROWS_AS(clock = idle, std::logic_error);
    const auto tick = clock.GetHalfTick();
    REQUIRE_THROWS_AS(clock.Step(), std::logic_error);
    REQUIRE_THROWS_AS(clock.StepHalf(), std::logic_error);
    REQUIRE_THROWS_AS(clock.RunStep(), std::logic_error);
    REQUIRE_THROWS_AS(clock.Reset(), std::logic_error);
    REQUIRE_THROWS_AS(clock.ReInit([](bool) { return 0; }), std::logic_error);
    REQUIRE_THROWS_AS(clock.default_stop_on_rise(false), std::logic_error);
    REQUIRE(clock.GetHalfTick() == tick);

    const auto handle = first.ArmEdge(XPhase::FallingStable);
    REQUIRE(first.RunUntil(8).hits.size() == 1);
    REQUIRE_THROWS_AS(first.ReleaseClock(), std::logic_error); // Fired is still registered.
    REQUIRE(first.Disarm(handle));
    REQUIRE_THROWS_AS(clock.StepHalf(), std::logic_error); // Still leased between batches.
    first.ClearExecutionState();
    first.ReleaseClock();
    REQUIRE_FALSE(clock.IsControlled());
    REQUIRE(clock.GetPhase() == XPhase::FallingStable); // Release never advances time.
    REQUIRE_FALSE(clock.IsAtCycleBoundary());
    REQUIRE(clock.StepHalf());
    REQUIRE(clock.IsAtCycleBoundary());
    second.AcquireClock();
    REQUIRE(second.RunUntil(2).advanced_ticks == 2);
    second.ReleaseClock();
    clock.Step();
}

TEST_CASE("XEngine releases ownership on destruction and checks clock lifetime",
          "[xtrigger][ownership]") {
    XClock clock([](bool) { return 0; });
    {
        XEngine engine(clock);
        engine.AcquireClock();
        engine.ArmEdge(XPhase::RisingStable);
    }
    REQUIRE_FALSE(clock.IsControlled());
    clock.Step();

    auto temporary = std::make_unique<XClock>([](bool) { return 0; });
    XEngine engine(*temporary);
    engine.AcquireClock();
    temporary.reset();
    REQUIRE_THROWS_AS(engine.RunUntil(2), std::logic_error);
    REQUIRE_THROWS_AS(engine.AcquireClock(), std::logic_error);
    REQUIRE_THROWS_AS(engine.ArmEdge(XPhase::RisingStable), std::logic_error);
    engine.Clear();
    REQUIRE_NOTHROW(engine.ReleaseClock());
}

TEST_CASE("XEngine rejects callback reentry and recovers from exceptions",
          "[xtrigger][ownership]") {
    XClock clock([](bool) { return 0; });
    XEngine engine(clock);
    clock.StepFal([&](uint64_t, void *) {
        REQUIRE_THROWS_AS(engine.AcquireClock(), std::logic_error);
        REQUIRE_THROWS_AS(engine.RunUntil(2), std::logic_error);
    });
    clock.Step();
    clock.ClearFalCallBacks();
    engine.AcquireClock();
    unsigned callbacks = 0;
    clock.StepFal([&](uint64_t, void *) {
        ++callbacks;
        REQUIRE_THROWS_AS(clock.Step(), std::logic_error);
        REQUIRE_THROWS_AS(engine.RunUntil(2), std::logic_error);
        REQUIRE_THROWS_AS(engine.SamplePhase(XPhase::DriveStable), std::logic_error);
        REQUIRE_THROWS_AS(engine.Clear(), std::logic_error);
        REQUIRE_THROWS_AS(engine.ClearExecutionState(), std::logic_error);
        REQUIRE_THROWS_AS(engine.CollectExpressions({}), std::logic_error);
        REQUIRE_THROWS_AS(engine.ReleaseClock(), std::logic_error);
    });
    REQUIRE(engine.RunUntil(2).advanced_ticks == 2);
    REQUIRE(callbacks == 1);
    clock.ClearFalCallBacks();
    clock.StepFal([](uint64_t, void *) { throw std::runtime_error("callback failed"); });
    REQUIRE_THROWS_WITH(engine.RunUntil(2), "callback failed");
    clock.ClearFalCallBacks();
    REQUIRE(engine.RunUntil(2).advanced_ticks == 2);
    REQUIRE_NOTHROW(engine.ReleaseClock());
}


TEST_CASE("XEngine cannot take over a manual step already in progress", "[xtrigger][ownership]") {
    XEngine *engine = nullptr;
    XClock clock([&](bool) {
        REQUIRE_THROWS_AS(engine->AcquireClock(), std::logic_error);
        return 0;
    });
    XEngine runtime(clock);
    engine = &runtime;
    clock.Step();
    REQUIRE_FALSE(clock.IsControlled());
    REQUIRE(clock.GetHalfTick() == 2);
    clock.StepHalf();
    REQUIRE_FALSE(clock.IsControlled());
    REQUIRE(clock.GetHalfTick() == 3);
    runtime.AcquireClock();
    runtime.ReleaseClock();
}

TEST_CASE("Collection preserves an in-flight FSM and a fired registration awaiting rearm", "[xtrigger][collection]") {
    XClock clock([](bool) { return 0; });
    XEngine engine(clock);
    engine.AcquireClock();
    XData request(1, XData::InOut), response(1, XData::InOut);
    request = 1; response = 0;
    for (int i = 0; i < 32; ++i) engine.ExprNewConst(i);
    const int req = engine.ExprNewSignal(&request), rsp = engine.ExprNewSignal(&response);
    const auto handle = engine.ArmFsm(2, 0, {{0, req, 1, 0, false}, {1, rsp, 0, 7, true}});
    REQUIRE(engine.RunUntil(2).hits.empty());
    engine.CollectExpressions({});
    REQUIRE(engine.ExpressionNodeCount() == 2);
    request = 0; response = 1;
    const auto first = engine.RunUntil(2);
    REQUIRE(first.hits.size() == 1);
    REQUIRE(first.hits[0].value == 7);
    engine.CollectExpressions({}); // fired, but still owned and rearmable
    REQUIRE(engine.ExpressionNodeCount() == 2);
    REQUIRE(engine.Rearm(handle));
    request = 1; response = 0;
    REQUIRE(engine.RunUntil(2).hits.empty());
    request = 0; response = 1;
    const auto second = engine.RunUntil(2);
    REQUIRE(second.hits.size() == 1);
    REQUIRE(second.hits[0].value == 7);
    REQUIRE(engine.Disarm(handle));
    engine.CollectExpressions({});
    REQUIRE(engine.ExpressionNodeCount() == 0);
    engine.ReleaseClock();
}
