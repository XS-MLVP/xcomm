#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xtrigger.h"
#include "xspcomm/xexpr.h"

using namespace xspcomm;

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
