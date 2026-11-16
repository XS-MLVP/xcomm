#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xclock.h"

#include <functional>
#include <string>
#include <vector>

using namespace xspcomm;

namespace {
// Mirrors what the SWIG directors install: the language runtime owns a
// subclass instance, marks it forced, and the xfunction stored in the callback
// table is a sliced base copy whose target points back at that instance.
struct DirectorLike : xfunction<void, u_int64_t, void *> {
    std::function<void(u_int64_t, void *)> body;
    explicit DirectorLike(std::function<void(u_int64_t, void *)> fn) :
        body(std::move(fn))
    {
        this->set_force_callable();
    }
    void call(u_int64_t cycle, void *args) override { this->body(cycle, args); }
};
} // namespace

TEST_CASE("StepHalf exposes stable falling and rising phases", "[xclock]") {
    std::vector<std::string> callbacks;
    XClock clk([](bool){ return 0; });
    clk.StepFal([&](uint64_t, void*) { callbacks.push_back("fall"); });
    clk.StepRis([&](uint64_t, void*) { callbacks.push_back("rise"); });

    REQUIRE(clk.GetHalfTick() == 0);
    REQUIRE(clk.IsAtCycleBoundary());

    REQUIRE(clk.StepHalf());
    REQUIRE(clk.clk == 1);
    REQUIRE(clk.GetHalfTick() == 1);
    REQUIRE(clk.GetPhase() == XPhase::FallingStable);
    REQUIRE_FALSE(clk.IsAtCycleBoundary());
    REQUIRE(callbacks == std::vector<std::string>{"fall"});

    REQUIRE(clk.StepHalf());
    REQUIRE(clk.clk == 1);
    REQUIRE(clk.GetHalfTick() == 2);
    REQUIRE(clk.GetPhase() == XPhase::RisingStable);
    REQUIRE(clk.IsAtCycleBoundary());
    REQUIRE(callbacks == std::vector<std::string>{"fall", "rise"});
}

TEST_CASE("StepHalf observes Disable between halves", "[xclock]") {
    std::vector<std::string> callbacks;
    XClock clk([](bool){ return 0; });
    clk.StepFal([&](uint64_t, void*) {
        callbacks.push_back("fall");
        clk.Disable();
    });
    clk.StepRis([&](uint64_t, void*) { callbacks.push_back("rise"); });

    REQUIRE(clk.StepHalf());
    REQUIRE_FALSE(clk.StepHalf());
    REQUIRE(callbacks == std::vector<std::string>{"fall"});
    REQUIRE(clk.GetHalfTick() == 1);

    clk.Enable();
    REQUIRE(clk.StepHalf());
    REQUIRE(callbacks == std::vector<std::string>{"fall", "rise"});
    REQUIRE(clk.IsAtCycleBoundary());
}

TEST_CASE("legacy Step still completes both halves before observing Disable",
          "[xclock]") {
    std::vector<std::string> callbacks;
    XClock clk([](bool){ return 0; });
    clk.StepFal([&](uint64_t, void*) {
        callbacks.push_back("fall");
        clk.Disable();
    });
    clk.StepRis([&](uint64_t, void*) { callbacks.push_back("rise"); });

    clk.Step(2);
    REQUIRE(callbacks == std::vector<std::string>{"fall", "rise"});
    REQUIRE(clk.clk == 1);
    REQUIRE(clk.GetHalfTick() == 2);
    REQUIRE(clk.IsAtCycleBoundary());
}

TEST_CASE("stop_on_rise false reverses half order", "[xclock]") {
    std::vector<std::string> callbacks;
    XClock clk([](bool){ return 0; });
    clk.default_stop_on_rise(false);
    clk.StepFal([&](uint64_t, void*) { callbacks.push_back("fall"); });
    clk.StepRis([&](uint64_t, void*) { callbacks.push_back("rise"); });

    REQUIRE(clk.StepHalf());
    REQUIRE(clk.GetPhase() == XPhase::RisingStable);
    REQUIRE(clk.StepHalf());
    REQUIRE(clk.GetPhase() == XPhase::FallingStable);
    REQUIRE(callbacks == std::vector<std::string>{"rise", "fall"});
}

TEST_CASE("Callbacks may modify the callback table while it runs", "[xclock]") {
    // A callback that re-registers itself used to append into the vector being
    // iterated by reference; a reallocation invalidated the loop. Clearing from
    // inside a callback destroyed the entries mid-dispatch. Both must now be
    // safe, with new callbacks deferred to the next edge.
    XClock clk([](bool){ return 0; });
    int inner_calls = 0;
    int added_once  = 0;

    clk.StepRis([&](uint64_t, void*) {
        if (added_once++ == 0) {
            // Appending during dispatch: must not run in this same edge.
            clk.StepRis([&](uint64_t, void*) { ++inner_calls; });
        }
    });

    clk.Step(1);
    REQUIRE(added_once == 1);
    REQUIRE(inner_calls == 0);   // deferred to the next rising edge

    clk.Step(1);
    REQUIRE(inner_calls == 1);   // now it runs

    // Clearing from inside a callback must not free the entries being walked.
    // It also cancels the callbacks that had not run yet, which is what
    // "clear" means for the caller.
    std::vector<std::string> order;
    XClock cleared([](bool){ return 0; });
    cleared.StepRis([&](uint64_t, void*) {
        order.push_back("first");
        cleared.ClearRisCallBacks();
    });
    cleared.StepRis([&](uint64_t, void*) { order.push_back("second"); });

    cleared.Step(1);
    REQUIRE(order == std::vector<std::string>{"first"});
    REQUIRE(cleared.StepRisQueueSize() == 0);   // cleared for good
    cleared.Step(1);
    REQUIRE(order == std::vector<std::string>{"first"});
}

TEST_CASE("Self-removal from a callback is deferred and safe", "[xclock]") {
    // Removing the list from inside one of its callbacks clears every entry,
    // including the ones that have not run yet, without freeing them mid-walk.
    XClock clk([](bool){ return 0; });
    int calls = 0;
    clk.StepRis([&](uint64_t, void*) {
        ++calls;
        clk.ClearRisCallBacks();
    });
    clk.StepRis([&](uint64_t, void*) { ++calls; });

    clk.Step(1);
    REQUIRE(calls == 1);
    REQUIRE(clk.StepRisQueueSize() == 0);
    clk.Step(5);
    REQUIRE(calls == 1);
}

TEST_CASE("Clearing the other edge's callbacks from a callback is deferred", "[xclock]") {
    XClock clk([](bool){ return 0; });
    int fall_calls = 0;
    clk.StepFal([&](uint64_t, void*) { ++fall_calls; });
    clk.StepRis([&](uint64_t, void*) { clk.ClearFalCallBacks(); });

    // One Step covers both edges; the falling edge of this cycle runs before
    // the rising callback asks for the fall list to be cleared.
    clk.Step(1);
    REQUIRE(fall_calls == 1);
    REQUIRE(clk.StepFalQueueSize() == 0);
    clk.Step(2);              // later falling edges have nothing left to run
    REQUIRE(fall_calls == 1);
}

TEST_CASE("Nested dispatch keeps the callback guard active", "[xclock]") {
    // A callback may re-enter _call_back (for example through _ris_refresh).
    // The inner dispatch must restore the flag it found instead of clearing
    // it, or the outer callback would look non-nested and could let Step()
    // advance the clock (or a clear erase the list being walked).
    XClock clk([](bool){ return 0; });
    int depth = 0;
    bool step_ignored = false;
    clk.StepRis([&](uint64_t, void*) {
        if (depth++ > 0) { return; }
        clk._ris_refresh();   // nested dispatch of the same callback list
        clk.Step(1);          // still inside a callback: must be ignored
        step_ignored = true;
    });

    clk.Step(1);
    REQUIRE(depth == 2);              // outer plus nested invocation
    REQUIRE(step_ignored);
    REQUIRE(clk.GetHalfTick() == 2);  // no extra edge was stepped
}

TEST_CASE("A director callback can clear the queue while dispatching", "[xclock]") {
    // force_callable makes operator bool true regardless of func, so a cleared
    // director used to keep running and never compacted. Clearing from the
    // first callback must cancel the rest and leave an empty queue.
    XClock clk([](bool){ return 0; });
    std::vector<std::string> order;
    DirectorLike first([&](uint64_t, void*) {
        order.push_back("first");
        clk.ClearRisCallBacks();
    });
    DirectorLike second([&](uint64_t, void*) { order.push_back("second"); });

    // SWIG installs a director as the xfunction base (`arg2 = *temp` in the
    // generated wrapper), so pass the base reference rather than letting the
    // forwarding constructor wrap a copy of the derived object.
    xfunction<void, u_int64_t, void *> &first_cb = first;
    xfunction<void, u_int64_t, void *> &second_cb = second;
    clk.StepRis(first_cb);
    clk.StepRis(second_cb);
    REQUIRE(clk.StepRisQueueSize() == 2);

    clk.Step(1);
    REQUIRE(order == std::vector<std::string>{"first"});
    REQUIRE(clk.StepRisQueueSize() == 0);   // compaction dropped both entries

    clk.Step(1);
    REQUIRE(order == std::vector<std::string>{"first"});
}

TEST_CASE("A director callback can remove a later director callback", "[xclock]") {
    // RemoveStepRisCbByDesc takes the deferred in-callback branch. For a
    // director entry the nulled callback must also stop counting as callable,
    // otherwise it runs anyway and the queue never shrinks.
    XClock clk([](bool){ return 0; });
    std::vector<std::string> order;
    int removed = -1;
    DirectorLike first([&](uint64_t, void*) {
        order.push_back("first");
        removed = clk.RemoveStepRisCbByDesc("second");
    });
    DirectorLike second([&](uint64_t, void*) { order.push_back("second"); });

    xfunction<void, u_int64_t, void *> &first_cb = first;
    xfunction<void, u_int64_t, void *> &second_cb = second;
    clk.StepRis(first_cb, nullptr, "first");
    clk.StepRis(second_cb, nullptr, "second");
    REQUIRE(clk.StepRisQueueSize() == 2);

    clk.Step(1);
    REQUIRE(removed == 1);
    REQUIRE(order == std::vector<std::string>{"first"});
    REQUIRE(clk.StepRisQueueSize() == 1);   // only "first" survives compaction

    clk.Step(1);
    REQUIRE(order == std::vector<std::string>{"first", "first"});
}

TEST_CASE("FreqDivWith rejects divider cycles", "[xclock]") {
    // div_clk edges point from the driving clock to the driven one, so adding
    // this -> clk is unsafe when clk can already reach this. A cycle would let
    // the clocks advance each other's cycle counters on every edge.
    XClock a([](bool){ return 0; });
    XClock b([](bool){ return 0; });

    a.FreqDivWith(1, &b);
    REQUIRE(a.div_clk.count(&b) == 1);

    b.FreqDivWith(1, &a);                   // closes a -> b -> a
    REQUIRE(b.div_clk.count(&a) == 0);
    REQUIRE(a.div_clk.count(&b) == 1);      // the existing edge is untouched

    b.FreqDivWith(1, &b);                   // still rejected directly
    REQUIRE(b.div_clk.count(&b) == 0);

    // Longer chains are detected as well: a -> b -> c, then c -> a.
    XClock c([](bool){ return 0; });
    b.FreqDivWith(1, &c);
    REQUIRE(b.div_clk.count(&c) == 1);
    c.FreqDivWith(1, &a);
    REQUIRE(c.div_clk.count(&a) == 0);
}
