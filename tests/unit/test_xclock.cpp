#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xclock.h"

#include <string>
#include <vector>

using namespace xspcomm;

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
