#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xtrigger.h"
#include "xspcomm/xexpr.h"

using namespace xspcomm;

TEST_CASE("XTriggerEngine broadcasts one edge occurrence", "[xtrigger]") {
    XClock clock([](bool){ return 0; });
    XTriggerEngine engine(clock, 8);
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

TEST_CASE("Coverage counts persist without returning ordinary hits", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock, 4);
    XData value(8, XData::InOut);
    value = 1;
    const int source = engine.ExprNewSignal(&value);
    const int one = engine.ExprNewCompare(static_cast<int>(ExprOp::EQ), source, engine.ExprNewConst(1));
    XCoverageItem item;
    item.signal = &value;
    XCoverageBin bin;
    bin.root = one;
    auto handle = engine.ArmEdge(XPhase::RisingStable);
    engine.AttachCoverage(handle, {item}, {bin});
    auto limit = engine.ArmClockCycles(10);
    auto run = engine.RunUntil(100);
    REQUIRE(run.advanced_ticks == 20);
    REQUIRE(run.hits.size() == 1);
    REQUIRE(run.hits.front().slot == limit.slot);
    const auto first = engine.CoverageSnapshot(handle);
    REQUIRE(first.counters[0] == 10);
    REQUIRE(first.counters.back() == 10);
    REQUIRE(engine.CoverageSnapshot(handle).counters == first.counters);
    REQUIRE_THROWS(engine.ClearExecutionState());
    engine.ResetCoverage(handle, false);
    REQUIRE(engine.CoverageSnapshot(handle).counters == first.counters);
    engine.ResetCoverage(handle, true);
    REQUIRE(engine.CoverageSnapshot(handle).epoch == 1);
    REQUIRE(engine.CoverageSnapshot(handle).counters[0] == 0);
    REQUIRE(engine.Disarm(handle));
    REQUIRE_THROWS(engine.CoverageSnapshot(handle));
    REQUIRE(engine.Disarm(limit));
    engine.ClearExecutionState();
    REQUIRE(engine.ActiveCount() == 0);
}

TEST_CASE("Coverage transitions overlap and have isolated registration history", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData value(8, XData::InOut);
    value = 1;
    const int source = engine.ExprNewSignal(&value);
    const int one = engine.ExprNewCompare(static_cast<int>(ExprOp::EQ), source, engine.ExprNewConst(1));
    XCoverageItem item;
    item.signal = &value;
    XCoverageBin bin;
    bin.steps = {{XSequenceStepKind::Wait, one}, {XSequenceStepKind::Next, one}};
    auto h = engine.ArmEdge(XPhase::RisingStable);
    engine.AttachCoverage(h, {item}, {bin});
    REQUIRE(engine.RunUntil(6).hits.empty());
    REQUIRE(engine.CoverageSnapshot(h).counters.back() == 2);
    engine.ResetCoverage(h, false);
    REQUIRE(engine.RunUntil(2).hits.empty());
    REQUIRE(engine.CoverageSnapshot(h).counters.back() == 2);
    REQUIRE(engine.Disarm(h));
    auto next = engine.ArmEdge(XPhase::RisingStable);
    engine.AttachCoverage(next, {item}, {bin});
    engine.RunUntil(2);
    REQUIRE(engine.CoverageSnapshot(next).counters.back() == 0);
    REQUIRE(engine.Disarm(next));
}

TEST_CASE("Coverage rejects malformed descriptors without changing the watcher", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    auto h = engine.ArmEdge(XPhase::RisingStable);
    XCoverageItem item;
    XCoverageBin bin;
    XData value(8, XData::InOut);
    item.signal = &value;
    bin.root = 100000;
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {bin}));
    REQUIRE(engine.RunUntil(2).hits.size() == 1);
    REQUIRE(engine.Disarm(h));
}


TEST_CASE("Coverage snapshots preserve full-width illegal values", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData value(128, XData::InOut);
    value = "0x80000000000000000000000000000007";
    auto bytes = value.GetBytes();
    XCoverageItem item;
    item.signal = &value;
    XCoverageBin bin;
    bin.kind = 2;
    bin.root = engine.ExprNewCompareSigConstBytes(static_cast<int>(ExprOp::EQ), &value, bytes);
    auto handle = engine.ArmEdge(XPhase::RisingStable);
    engine.AttachCoverage(handle, {item}, {bin}, -1, -1, false);
    engine.RunUntil(2);
    value = 0;
    const auto snapshot = engine.CoverageSnapshot(handle);
    REQUIRE(snapshot.illegal_values == std::vector<std::string>{"80000000000000000000000000000007"});
    REQUIRE(snapshot.illegal_ticks == std::vector<unsigned long long>{2});
    REQUIRE(snapshot.counters.back() == 1);
    engine.ResetCoverage(handle, true);
    REQUIRE(engine.CoverageSnapshot(handle).illegal_values.empty());
    REQUIRE(engine.Disarm(handle));
    REQUIRE_THROWS(engine.CoverageSnapshot(handle));
    engine.ClearExecutionState();
    REQUIRE(engine.ActiveCount() == 0);
}


TEST_CASE("Coverage overlapping sequences count simultaneous completions", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData start(1, XData::InOut), done(1, XData::InOut), value(8, XData::InOut);
    const int start_root = engine.ExprNewSignal(&start), done_root = engine.ExprNewSignal(&done);
    auto h = engine.ArmSequence({{XSequenceStepKind::Wait, start_root},
                                {XSequenceStepKind::Within, done_root, 1, 3}});
    XCoverageItem item; item.signal = &value;
    XCoverageBin bin; bin.root = engine.ExprNewConst(1);
    engine.AttachCoverage(h, {item}, {bin}, -1, -1, true, 1024, true, 2, true);
    start = 1;
    REQUIRE(engine.RunUntil(4).hits.empty());
    auto pending = engine.CoverageSnapshot(h, true);
    REQUIRE(pending.diagnostics[0] == 2);
    REQUIRE(pending.diagnostics[6] == 2);
    REQUIRE(pending.progress.size() == 10);
    REQUIRE(engine.CoverageSnapshot(h).progress.empty());
    start = 0; done = 1;
    REQUIRE(engine.RunUntil(2).hits.empty());
    auto completed = engine.CoverageSnapshot(h, true);
    REQUIRE(completed.counters[0] == 2);
    REQUIRE(completed.counters.back() == 2);
    REQUIRE(completed.diagnostics[1] == 2);
    REQUIRE(completed.progress.empty());
    engine.ResetCoverage(h, true);
    REQUIRE(engine.CoverageSnapshot(h).diagnostics[0] == 0);
    REQUIRE(engine.Disarm(h));
    engine.ClearExecutionState();
}

TEST_CASE("Coverage overlap capacity failure preserves pending state for cleanup", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData start(1, XData::InOut), done(1, XData::InOut), value(8, XData::InOut);
    auto h = engine.ArmSequence({{XSequenceStepKind::Wait, engine.ExprNewSignal(&start)},
                                {XSequenceStepKind::Wait, engine.ExprNewSignal(&done)}});
    XCoverageItem item; item.signal = &value;
    XCoverageBin bin; bin.root = engine.ExprNewConst(1);
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {bin}, -1, -1, true, 1024, true, 0, true));
    engine.AttachCoverage(h, {item}, {bin}, -1, -1, true, 1024, true, 1, false);
    start = 1;
    REQUIRE_THROWS(engine.RunUntil(4));
    auto snapshot = engine.CoverageSnapshot(h, true);
    REQUIRE(snapshot.incomplete);
    REQUIRE(snapshot.counters[0] == 0);
    REQUIRE(snapshot.diagnostics.empty());
    REQUIRE(snapshot.progress.size() == 5);
    REQUIRE(engine.Disarm(h));
    REQUIRE_THROWS(engine.CoverageSnapshot(h, true));
    engine.ClearExecutionState();
    REQUIRE(engine.ActiveCount() == 0);
}

TEST_CASE("Coverage bins run full independent temporal programs", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData start(1, XData::InOut), done(1, XData::InOut), stable(1, XData::InOut);
    const auto req = engine.ExprNewSignal(&start), ack = engine.ExprNewSignal(&done);
    const auto ready = engine.ExprNewSignal(&stable);
    auto handle = engine.ArmSample(XPhase::RisingStable);
    XCoverageItem item; item.pattern = true;
    XCoverageBin long_bin; long_bin.program_kind = 1; long_bin.overlap = false;
    long_bin.steps = {{XSequenceStepKind::Wait, req}, {XSequenceStepKind::Within, ack, 1, 4},
                      {XSequenceStepKind::Hold, ready, 0, 0, 2}};
    auto short_bin = long_bin; short_bin.steps[1].maximum = 1;
    engine.AttachCoverage(handle, {item}, {long_bin, short_bin});
    start = 1; REQUIRE(engine.RunUntil(2).hits.empty());
    start = 0; REQUIRE(engine.RunUntil(2).hits.empty());
    done = 1; REQUIRE(engine.RunUntil(2).hits.empty());
    done = 0; stable = 1;
    REQUIRE(engine.RunUntil(4).hits.empty());
    const auto snapshot = engine.CoverageSnapshot(handle);
    REQUIRE(snapshot.counters[0] == 5);
    REQUIRE(snapshot.counters[7] == 1);
    REQUIRE(snapshot.counters[8] == 0);
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("Coverage FSM terminal filtering ends every terminal attempt", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData start(1, XData::InOut), success(1, XData::InOut), failure(1, XData::InOut);
    XCoverageItem item; item.pattern = true;
    XCoverageBin bin; bin.program_kind = 2; bin.overlap = false; bin.state_count = 2;
    bin.transitions = {{0, engine.ExprNewSignal(&start), 1, 0, false},
                       {1, engine.ExprNewSignal(&success), 0, 7, true},
                       {1, engine.ExprNewSignal(&failure), 0, 8, true}};
    bin.terminals = {7};
    auto h = engine.ArmSample(XPhase::RisingStable);
    engine.AttachCoverage(h, {item}, {bin}, -1, -1, true, 1024, false, 1, true);
    start = 1; engine.RunUntil(2);
    start = 0; failure = 1; engine.RunUntil(2);
    auto failed = engine.CoverageSnapshot(h, true);
    REQUIRE(failed.counters.back() == 0);
    REQUIRE(failed.progress.empty());
    REQUIRE(failed.diagnostics[8] == 1); // Bin completed even though its terminal was unselected.
    failure = 0; success = 1; engine.RunUntil(2);
    REQUIRE(engine.CoverageSnapshot(h).counters.back() == 0);
    start = 1; success = 0; engine.RunUntil(2);
    start = 0; success = 1; engine.RunUntil(2);
    REQUIRE(engine.CoverageSnapshot(h).counters.back() == 1);
    REQUIRE(engine.Disarm(h));
}

TEST_CASE("Coverage pattern overlap preserves simultaneous completion counts", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData start(1, XData::InOut), done(1, XData::InOut);
    auto h = engine.ArmSample(XPhase::RisingStable);
    XCoverageItem item; item.pattern = true;
    XCoverageBin bin; bin.program_kind = 1; bin.overlap = true; bin.max_active = 3;
    bin.steps = {{XSequenceStepKind::Wait, engine.ExprNewSignal(&start)},
                 {XSequenceStepKind::Within, engine.ExprNewSignal(&done), 1, 5}};
    engine.AttachCoverage(h, {item}, {bin});
    start = 1; REQUIRE(engine.RunUntil(6).hits.empty());
    start = 0; done = 1; REQUIRE(engine.RunUntil(2).hits.empty());
    REQUIRE(engine.CoverageSnapshot(h).counters.back() == 3);
    REQUIRE(engine.Disarm(h));
}

TEST_CASE("Coverage rejects malformed pattern descriptors atomically", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    auto h = engine.ArmSample(XPhase::RisingStable);
    XCoverageItem item; item.pattern = true;
    XCoverageBin bin; bin.overlap = false; bin.root = engine.ExprNewConst(1);
    auto invalid = bin; invalid.mode = static_cast<XConditionMode>(99);
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {invalid}));
    invalid = bin; invalid.max_active = 0;
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {invalid}));
    invalid = bin; invalid.program_kind = 2; invalid.state_count = 1;
    invalid.root = -1; invalid.terminals = {99};
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {invalid}));
    engine.AttachCoverage(h, {item}, {bin});
    engine.RunUntil(4);
    REQUIRE(engine.CoverageSnapshot(h).counters.back() == 1); // ENTER, not EACH_SAMPLE.
    REQUIRE(engine.Disarm(h));
}
