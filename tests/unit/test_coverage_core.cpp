#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/detail/coverage/state.h"
#include "xspcomm/xexpr.h"

using namespace xspcomm;
using detail::CoverageState;

TEST_CASE("Coverage samples independently of the trigger engine", "[coverage]") {
    ExprEngine expr;
    XData value(8, XData::InOut), gate(1, XData::InOut), abort(1, XData::InOut);
    value = 7;
    gate = 1;
    abort = 0;
    XCoverageItem item;
    item.signal = &value;
    XCoverageBin bin;
    bin.program.root = expr.NewConst(1);
    CoverageState coverage(9, expr, {}, {item}, {bin},
        expr.NewSignal(&gate), expr.NewSignal(&abort), true, 32, false, 1, true);

    REQUIRE(coverage.BeginSample(expr, 1) == CoverageState::SampleStatus::Ready);
    coverage.Sample(expr, 1);
    REQUIRE(coverage.BeginSample(expr, 1) == CoverageState::SampleStatus::Skip);
    REQUIRE(coverage.Snapshot(1, false).counters.back() == 1);

    gate = 0;
    REQUIRE(coverage.BeginSample(expr, 2) == CoverageState::SampleStatus::Ready);
    coverage.Sample(expr, 2);
    REQUIRE(coverage.Snapshot(2, false).counters[1] == 1);
    REQUIRE(coverage.Snapshot(2, false).counters.back() == 1);

    abort = 1;
    REQUIRE(coverage.BeginSample(expr, 3) == CoverageState::SampleStatus::Aborted);
    REQUIRE(coverage.Snapshot(3, false).counters.back() == 1);
    coverage.ResetCounters();
    const auto reset = coverage.Snapshot(3, false);
    REQUIRE(reset.generation == 9);
    REQUIRE(reset.epoch == 1);
    REQUIRE(reset.counters.back() == 0);
}

TEST_CASE("Coverage abort clears overlapping pattern history", "[coverage][pattern]") {
    ExprEngine expr;
    XData start(1, XData::InOut), done(1, XData::InOut);
    XData abort(1, XData::InOut), value(8, XData::InOut);
    start = 1;
    done = 0;
    abort = 0;
    const std::vector<XSequenceStep> steps = {
        {XSequenceStepKind::Wait, expr.NewSignal(&start)},
        {XSequenceStepKind::Within, expr.NewSignal(&done), 1, 3}};
    const detail::PatternView source{&steps};
    XCoverageItem item;
    item.signal = &value;
    XCoverageBin bin;
    bin.program.root = expr.NewConst(1);
    CoverageState coverage(1, expr, source, {item}, {bin},
        -1, expr.NewSignal(&abort), true, 32, true, 2, true);

    for (uint64_t tick = 1; tick <= 2; ++tick) {
        expr.SetCycle(tick);
        REQUIRE(coverage.BeginSample(expr, tick) == CoverageState::SampleStatus::Ready);
        coverage.SamplePattern(expr, tick);
    }
    REQUIRE(coverage.Snapshot(2, true).progress.size() == 10);
    abort = 1;
    REQUIRE(coverage.BeginSample(expr, 3) == CoverageState::SampleStatus::Aborted);
    const auto aborted = coverage.Snapshot(3, true);
    REQUIRE(aborted.progress.empty());
    REQUIRE(aborted.diagnostics[4] == 2);

    start = 0;
    done = 1;
    abort = 0;
    REQUIRE(coverage.BeginSample(expr, 4) == CoverageState::SampleStatus::Ready);
    coverage.SamplePattern(expr, 4);
    REQUIRE(coverage.Snapshot(4, true).counters[0] == 0);
}
