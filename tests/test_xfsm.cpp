#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xfsm.h"
#include "xspcomm/xclock.h"

using namespace xspcomm;

TEST_CASE("FSM basic transitions and trigger", "[xfsm]") {
    const std::string prog =
        "start S0\n"
        "state S0:\n"
        "  set $flag0\n"
        "  if $flag0 == 1 goto S1\n"
        "state S1:\n"
        "  inc $counter0\n"
        "  if $counter0 >= 2 trigger\n";

    XClock clk([](bool){ return 0; });
    ComUseFsmTrigger fsm(&clk);
    fsm.LoadProgram(prog, nullptr);
    REQUIRE(fsm.GetCurrentState() == "S0");

    auto fn = (void (*)(uint64_t, void*))ComUseStepCb::GetCb();
    fn(1, &fsm);
    REQUIRE(fsm.GetCurrentState() == "S1");
    REQUIRE(fsm.IsTriggered() == false);

    fn(2, &fsm);
    REQUIRE(fsm.IsTriggered() == false);

    fn(3, &fsm);
    REQUIRE(fsm.IsTriggered() == true);
    REQUIRE(fsm.GetTriggeredState() == "S1");
    REQUIRE(clk.IsDisable() == true);
}

TEST_CASE("FSM else/goto", "[xfsm]") {
    const std::string prog =
        "state A:\n"
        "  if 0 goto B\n"
        "  else goto C\n"
        "state B:\n"
        "  trigger\n"
        "state C:\n"
        "  trigger\n";

    ComUseFsmTrigger fsm;
    fsm.LoadProgram(prog, nullptr);
    REQUIRE(fsm.GetCurrentState() == "A");

    auto fn = (void (*)(uint64_t, void*))ComUseStepCb::GetCb();
    fn(1, &fsm);
    REQUIRE(fsm.GetCurrentState() == "C");
}

TEST_CASE("FSM parse errors", "[xfsm]") {
    ComUseFsmTrigger fsm;

    REQUIRE_NOTHROW(fsm.LoadProgram("", nullptr));
    REQUIRE(fsm.ListStates().empty());
    REQUIRE(fsm.GetCurrentState().empty());

    REQUIRE_NOTHROW(fsm.LoadProgram("state A", nullptr));
    REQUIRE(fsm.ListStates().empty());
    REQUIRE(fsm.GetCurrentState().empty());

    const std::string dup =
        "state A:\n"
        "state A:\n";
    REQUIRE_NOTHROW(fsm.LoadProgram(dup, nullptr));
    REQUIRE(fsm.ListStates().empty());
    REQUIRE(fsm.GetCurrentState().empty());

    const std::string bad_start =
        "start X\n"
        "state A:\n";
    REQUIRE_NOTHROW(fsm.LoadProgram(bad_start, nullptr));
    REQUIRE(fsm.ListStates().empty());
    REQUIRE(fsm.GetCurrentState().empty());

    const std::string bad_goto =
        "state A:\n"
        "  goto B\n";
    REQUIRE_NOTHROW(fsm.LoadProgram(bad_goto, nullptr));
    REQUIRE(fsm.ListStates().empty());
    REQUIRE(fsm.GetCurrentState().empty());
}
