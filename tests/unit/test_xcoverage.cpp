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
    bin.program.root = one;
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
    bin.program.kind = XTriggerProgramKind::Sequence;
    bin.program.steps = {{XSequenceStepKind::Wait, one}, {XSequenceStepKind::Next, one}};
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
    bin.program.root = 100000;
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
    bin.kind = XCoverageBinKind::Illegal;
    bin.program.root = engine.ExprNewCompareSigConstBytes(static_cast<int>(ExprOp::EQ), &value, bytes);
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
    XCoverageBin bin; bin.program.root = engine.ExprNewConst(1);
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
    XCoverageBin bin; bin.program.root = engine.ExprNewConst(1);
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
    XCoverageItem item; item.kind = XCoverageItemKind::Pattern;
    XCoverageBin long_bin; long_bin.program.kind = XTriggerProgramKind::Sequence; long_bin.program.overlap = false;
    long_bin.program.steps = {{XSequenceStepKind::Wait, req}, {XSequenceStepKind::Within, ack, 1, 4},
                      {XSequenceStepKind::Hold, ready, 0, 0, 2}};
    auto short_bin = long_bin; short_bin.program.steps[1].maximum = 1;
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
    XCoverageItem item; item.kind = XCoverageItemKind::Pattern;
    XCoverageBin bin; bin.program.kind = XTriggerProgramKind::Fsm; bin.program.overlap = false; bin.program.state_count = 2;
    bin.program.transitions = {{0, engine.ExprNewSignal(&start), 1, 0, false},
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
    XCoverageItem item; item.kind = XCoverageItemKind::Pattern;
    XCoverageBin bin; bin.program.kind = XTriggerProgramKind::Sequence; bin.program.overlap = true; bin.program.max_active = 3;
    bin.program.steps = {{XSequenceStepKind::Wait, engine.ExprNewSignal(&start)},
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
    XCoverageItem item; item.kind = XCoverageItemKind::Pattern;
    XCoverageBin bin; bin.program.overlap = false; bin.program.root = engine.ExprNewConst(1);
    auto invalid = bin; invalid.program.mode = static_cast<XConditionMode>(99);
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {invalid}));
    invalid = bin; invalid.program.max_active = 0;
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {invalid}));
    invalid = bin; invalid.program.kind = XTriggerProgramKind::Fsm; invalid.program.state_count = 1;
    invalid.program.root = -1; invalid.terminals = {99};
    REQUIRE_THROWS(engine.AttachCoverage(h, {item}, {invalid}));
    engine.AttachCoverage(h, {item}, {bin});
    engine.RunUntil(4);
    REQUIRE(engine.CoverageSnapshot(h).counters.back() == 1); // ENTER, not EACH_SAMPLE.
    REQUIRE(engine.Disarm(h));
}

TEST_CASE("Coverage shares an FSM and routes every terminal to its bins", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData request(1, XData::InOut), response(1, XData::InOut), correct(1, XData::InOut);
    const auto req = engine.ExprNewSignal(&request), rsp = engine.ExprNewSignal(&response);
    const auto good = engine.ExprNewSignal(&correct);
    const auto ok = engine.ExprNewBinary(static_cast<int>(ExprOp::LAND), rsp, good);
    XCoverageItem item; item.kind = XCoverageItemKind::Pattern;
    XCoverageBin first; first.program.kind = XTriggerProgramKind::Fsm; first.program.state_count = 2; first.program.overlap = false;
    first.program.transitions = {{0, req, 1, 0, false}, {1, ok, 0, 7, true}, {1, rsp, 0, 8, true}};
    first.terminals = {7};
    auto second = first; second.terminals = {8}; second.kind = XCoverageBinKind::Illegal;
    auto all = first; all.terminals.clear();
    const auto handle = engine.ArmSample(XPhase::RisingStable);
    engine.AttachCoverage(handle, {item}, {first, second, all}, -1, -1, false, 1024, false, 1, true);
    REQUIRE(engine.ActiveCount() == 1);
    REQUIRE(engine.CoverageExecutionCount(handle) == 1);
    request = 1; engine.RunUntil(2);
    auto pending = engine.CoverageSnapshot(handle, true);
    REQUIRE(pending.progress.size() == 15); // Three bin views of the same pending observation.
    request = 0; response = 1; correct = 1; engine.RunUntil(2);
    auto success = engine.CoverageSnapshot(handle);
    REQUIRE(success.counters[7] == 1);
    REQUIRE(success.counters[8] == 0);
    REQUIRE(success.counters[9] == 1);
    request = 1; response = 0; engine.RunUntil(2);
    request = 0; response = 1; correct = 0; engine.RunUntil(2);
    auto failure = engine.CoverageSnapshot(handle, true);
    REQUIRE(failure.counters[7] == 1);
    REQUIRE(failure.counters[8] == 1);
    REQUIRE(failure.counters[9] == 2);
    REQUIRE(failure.illegal_bins.size() == 1);
    REQUIRE(failure.illegal_bins[0] == 1);
    REQUIRE(failure.progress.empty());
    for (size_t bin = 0; bin < 3; ++bin) {
        REQUIRE(failure.diagnostics[(bin + 1) * 7] == 2);
        REQUIRE(failure.diagnostics[(bin + 1) * 7 + 1] == 2);
    }
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("Shared coverage FSM retains simultaneous counts for different terminals", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData request(1, XData::InOut), response(1, XData::InOut);
    const auto req = engine.ExprNewSignal(&request), rsp = engine.ExprNewSignal(&response);
    XCoverageItem item; item.kind = XCoverageItemKind::Pattern;
    XCoverageBin fast; fast.program.kind = XTriggerProgramKind::Fsm; fast.program.state_count = 3;
    fast.program.overlap = true; fast.program.max_active = 3;
    fast.program.transitions = {{0, req, 1, 0, false}, {1, rsp, 0, 7, true},
                        {1, -1, 2, 0, false}, {2, rsp, 0, 8, true}};
    fast.terminals = {7};
    auto slow = fast; slow.terminals = {8};
    auto all = fast; all.terminals.clear();
    const auto handle = engine.ArmSample(XPhase::RisingStable);
    engine.AttachCoverage(handle, {item}, {fast, slow, all}, -1, -1, true, 1024, false, 1, true);
    REQUIRE(engine.CoverageExecutionCount(handle) == 1);
    request = 1; REQUIRE(engine.RunUntil(6).hits.empty());
    request = 0; response = 1; REQUIRE(engine.RunUntil(2).hits.empty());
    const auto snapshot = engine.CoverageSnapshot(handle);
    REQUIRE(snapshot.counters[7] == 1);
    REQUIRE(snapshot.counters[8] == 2);
    REQUIRE(snapshot.counters[9] == 3);
    for (size_t bin = 0; bin < 3; ++bin) {
        REQUIRE(snapshot.diagnostics[(bin + 1) * 7] == 3);
        REQUIRE(snapshot.diagnostics[(bin + 1) * 7 + 1] == 3);
        REQUIRE(snapshot.diagnostics[(bin + 1) * 7 + 6] == 3);
    }
    REQUIRE(engine.Disarm(handle));
}

TEST_CASE("Coverage shares only identical execution policies in the same point", "[xtrigger][coverage]") {
    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    XData request(1, XData::InOut), response(1, XData::InOut);
    const auto req = engine.ExprNewSignal(&request), rsp = engine.ExprNewSignal(&response);
    XCoverageItem item; item.kind = XCoverageItemKind::Pattern;
    XCoverageBin original; original.program.kind = XTriggerProgramKind::Sequence; original.program.overlap = false;
    original.program.steps = {{XSequenceStepKind::Wait, req}, {XSequenceStepKind::Within, rsp, 1, 4}};
    auto alias = original;
    auto other_point = original; other_point.item = 1;
    auto short_window = original; short_window.program.steps[1].maximum = 2;
    auto overlapping = original; overlapping.program.overlap = true; overlapping.program.max_active = 3;
    auto h = engine.ArmSample(XPhase::RisingStable);
    engine.AttachCoverage(h, {item, item}, {original, alias, other_point, short_window, overlapping});
    REQUIRE(engine.CoverageExecutionCount(h) == 4);
    request = 1; engine.RunUntil(2);
    engine.ResetCoverage(h, false);
    request = 0; response = 1; engine.RunUntil(2);
    const auto snapshot = engine.CoverageSnapshot(h);
    for (size_t bin = 0; bin < 5; ++bin) REQUIRE(snapshot.counters[12 + bin] == 0);
    REQUIRE(engine.Disarm(h));
}

TEST_CASE("Explicit coverage kinds reject mixed definitions", "[coverage][validation]") {
    XClock clock([](bool) { return 0; });
    XEngine engine(clock);
    XData signal(8, XData::InOut);
    const auto root = engine.ExprNewConst(1);
    auto handle = engine.ArmSample(XPhase::RisingStable);
    XCoverageItem item;
    XCoverageBin bin;
    bin.program.root = root;
    REQUIRE_THROWS_AS(engine.AttachCoverage(handle, {item}, {bin}), std::invalid_argument);
    item.kind = XCoverageItemKind::Pattern;
    item.signal = &signal;
    REQUIRE_THROWS_AS(engine.AttachCoverage(handle, {item}, {bin}), std::invalid_argument);
    item.signal = nullptr;
    bin.program.overlap = false;
    bin.program.transitions = {{0, root, 0, 0, true}};
    REQUIRE_THROWS_AS(engine.AttachCoverage(handle, {item}, {bin}), std::invalid_argument);
    bin.program.transitions.clear();
    bin.program.kind = static_cast<XTriggerProgramKind>(99);
    REQUIRE_THROWS_AS(engine.AttachCoverage(handle, {item}, {bin}), std::invalid_argument);
    bin.program.kind = XTriggerProgramKind::Expr;
    engine.AttachCoverage(handle, {item}, {bin});
    engine.RunUntil(2);
    REQUIRE(engine.CoverageSnapshot(handle).counters.back() == 1);
}

TEST_CASE("Trigger and coverage share program validation", "[coverage][trigger][validation]") {
    XClock clock([](bool) { return 0; });
    XEngine engine(clock);
    const auto root = engine.ExprNewConst(1);
    const std::vector<XSequenceStep> invalid{{static_cast<XSequenceStepKind>(99), root}};
    REQUIRE_FALSE(engine.ArmSequence(invalid).IsValid());
    XCoverageItem item;
    item.kind = XCoverageItemKind::Pattern;
    XCoverageBin bin;
    bin.program.kind = XTriggerProgramKind::Sequence;
    bin.program.steps = invalid;
    bin.program.overlap = false;
    const auto handle = engine.ArmSample(XPhase::RisingStable);
    REQUIRE_THROWS_AS(engine.AttachCoverage(handle, {item}, {bin}), std::invalid_argument);
}
