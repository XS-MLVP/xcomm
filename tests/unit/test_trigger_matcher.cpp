#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/detail/trigger/matcher.h"
#include "xspcomm/xexpr.h"

using namespace xspcomm;
using detail::PatternMatcher;
using detail::PatternView;

TEST_CASE("Trigger matchers retain expression event modes across unknown samples", "[matcher]") {
    ExprEngine expr;
    XData value(1, XData::InOut);
    PatternView program;
    program.root = expr.NewSignal(&value);
    PatternMatcher enter(program);
    program.mode = XConditionMode::EachSample;
    PatternMatcher each(program);
    program.mode = XConditionMode::Change;
    PatternMatcher change(program);

    const unsigned samples[] = {0, 1, 1, 0, 1};
    const unsigned enters[] = {0, 1, 0, 0, 1};
    const unsigned changes[] = {0, 1, 0, 1, 1};
    for (size_t i = 0; i < 5; ++i) {
        value = samples[i];
        REQUIRE(enter.Advance(expr).completed == enters[i]);
        REQUIRE(each.Advance(expr).completed == samples[i]);
        REQUIRE(change.Advance(expr).completed == changes[i]);
    }
    value = "x";
    REQUIRE(enter.Advance(expr).completed == 0);
    REQUIRE(each.Advance(expr).completed == 0);
    REQUIRE(change.Advance(expr).completed == 0);
    value = 1;
    REQUIRE(enter.Advance(expr).completed == 0);
    REQUIRE(each.Advance(expr).completed == 1);
    REQUIRE(change.Advance(expr).completed == 0);
    enter.Clear();
    REQUIRE(enter.Advance(expr).completed == 1);
}

TEST_CASE("Trigger executions share a program without sharing history", "[matcher]") {
    ExprEngine expr;
    XData start(1, XData::InOut), done(1, XData::InOut);
    const std::vector<XSequenceStep> steps = {
        {XSequenceStepKind::Wait, expr.NewSignal(&start)},
        {XSequenceStepKind::Within, expr.NewSignal(&done), 1, 3}};
    PatternMatcher first({&steps}), second({&steps});
    start = 1;
    done = 0;
    REQUIRE(first.Advance(expr).started == 1);
    REQUIRE(first.States().size() == 1);
    REQUIRE(second.States().empty());
    start = 0;
    done = 1;
    REQUIRE(first.Advance(expr).completed == 1);
    REQUIRE(second.Advance(expr).completed == 0);
    REQUIRE(first.States().empty());
}

TEST_CASE("Trigger overlap reports capacity exhaustion and retains pending executions", "[matcher]") {
    ExprEngine expr;
    XData start(1, XData::InOut), done(1, XData::InOut);
    const std::vector<XSequenceStep> steps = {
        {XSequenceStepKind::Wait, expr.NewSignal(&start)},
        {XSequenceStepKind::Within, expr.NewSignal(&done), 1, 5}};
    PatternMatcher matcher({&steps});
    start = 1;
    done = 0;
    REQUIRE(matcher.Advance(expr, true, 2).peak_active == 1);
    REQUIRE(matcher.Advance(expr, true, 2).peak_active == 2);
    const auto exhausted = matcher.Advance(expr, true, 2);
    REQUIRE(exhausted.capacity_exhausted);
    REQUIRE(exhausted.started == 0);
    REQUIRE(matcher.States().size() == 2);
    matcher.Clear();
    REQUIRE(matcher.States().empty());
    start = 0;
    done = 1;
    REQUIRE(matcher.Advance(expr, true, 2).completed == 0);
}

TEST_CASE("Trigger FSM execution preserves completion counts for every terminal", "[matcher]") {
    ExprEngine expr;
    XData request(1, XData::InOut), response(1, XData::InOut);
    const auto req = expr.NewSignal(&request), rsp = expr.NewSignal(&response);
    const std::vector<XFsmTransition> transitions = {
        {0, req, 1, 0, false}, {1, rsp, 0, 7, true},
        {1, -1, 2, 0, false}, {2, rsp, 0, 8, true}};
    PatternMatcher matcher({nullptr, &transitions, 0});
    request = 1;
    response = 0;
    for (unsigned i = 0; i < 3; ++i) {
        REQUIRE(matcher.Advance(expr, true, 3).completed == 0);
        REQUIRE(matcher.States().size() == i + 1);
    }
    request = 0;
    response = 1;
    REQUIRE(matcher.Advance(expr, true, 3).completed == 3);
    REQUIRE(matcher.Count({7}) == 1);
    REQUIRE(matcher.Count({8}) == 2);
    REQUIRE(matcher.Count({7, 8}) == 3);
    REQUIRE(matcher.Count() == 3);
    REQUIRE(matcher.States().empty());
    matcher.Clear();
    REQUIRE(matcher.Count() == 0);
}

TEST_CASE("Trigger sequence expiry is separate from a failed adjacent step", "[matcher]") {
    ExprEngine expr;
    XData start(1, XData::InOut), done(1, XData::InOut);
    const auto req = expr.NewSignal(&start), ack = expr.NewSignal(&done);
    const std::vector<XSequenceStep> timed = {
        {XSequenceStepKind::Wait, req}, {XSequenceStepKind::Within, ack, 1, 1}};
    const std::vector<XSequenceStep> adjacent = {
        {XSequenceStepKind::Wait, req}, {XSequenceStepKind::Next, ack}};
    PatternMatcher window({&timed}), next({&adjacent});
    start = 1;
    done = 0;
    REQUIRE(window.Advance(expr).started == 1);
    REQUIRE(next.Advance(expr).started == 1);
    start = 0;
    REQUIRE(window.Advance(expr).expired == 0);
    const auto failed = next.Advance(expr);
    REQUIRE(failed.failed == 1);
    REQUIRE(failed.expired == 0);
    const auto expired = window.Advance(expr);
    REQUIRE(expired.expired == 1);
    REQUIRE(expired.failed == 0);
    REQUIRE(window.States().empty());
    REQUIRE(next.States().empty());
}
