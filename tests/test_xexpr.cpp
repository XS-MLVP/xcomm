#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xexpr.h"
#include "xspcomm/xdata.h"

using namespace xspcomm;

TEST_CASE("ExprEngine arithmetic and precedence", "[xexpr]") {
    XData a(32, XData::InOut);
    XData b(32, XData::InOut);
    a = 3;
    b = 4;

    ExprEngine eng;
    eng.RegisterExternalSignal("a", &a);
    eng.RegisterExternalSignal("b", &b);

    int root = eng.CompileExpr("a + 2 * b", nullptr);
    REQUIRE(eng.Eval(root) == 11);

    int root2 = eng.CompileExpr("(a + 2) * b", nullptr);
    REQUIRE(eng.Eval(root2) == 20);
}

TEST_CASE("ExprEngine logical ops and keywords", "[xexpr]") {
    XData a(1, XData::InOut);
    XData b(1, XData::InOut);
    a = 0;
    b = 1;

    ExprEngine eng;
    eng.RegisterExternalSignal("a", &a);
    eng.RegisterExternalSignal("b", &b);

    int root = eng.CompileExpr("a || b", nullptr);
    REQUIRE(eng.Eval(root) == 1);

    int root2 = eng.CompileExpr("not a && b", nullptr);
    REQUIRE(eng.Eval(root2) == 1);
}

TEST_CASE("ExprEngine within/hold", "[xexpr]") {
    XData a(1, XData::InOut);
    ExprEngine eng;
    eng.RegisterExternalSignal("a", &a);

    int within_root = eng.CompileExpr("within(2, a)", nullptr);
    int hold_root = eng.CompileExpr("hold(2, a)", nullptr);

    a = 1;
    eng.SetCycle(1);
    REQUIRE(eng.Eval(within_root) == 1);
    REQUIRE(eng.Eval(hold_root) == 0);

    a = 0;
    eng.SetCycle(2);
    REQUIRE(eng.Eval(within_root) == 1);
    REQUIRE(eng.Eval(hold_root) == 0);

    a = 1;
    eng.SetCycle(3);
    REQUIRE(eng.Eval(within_root) == 1);
    REQUIRE(eng.Eval(hold_root) == 0);

    a = 1;
    eng.SetCycle(4);
    REQUIRE(eng.Eval(hold_root) == 1);

    a = 0;
    eng.SetCycle(5);
    REQUIRE(eng.Eval(hold_root) == 0);
}

TEST_CASE("ExprEngine wide signal compare", "[xexpr]") {
    XData w(128, XData::InOut);
    ExprEngine eng;
    eng.RegisterExternalSignal("w", &w);

    int root = eng.CompileExpr("w == 1", nullptr);
    w = "0x1";
    REQUIRE(eng.Eval(root) == 1);

    REQUIRE_THROWS(eng.CompileExpr("w + 1", nullptr));
}

TEST_CASE("ExprEngine parser errors", "[xexpr]") {
    ExprEngine eng;
    REQUIRE_THROWS(eng.CompileExpr("", nullptr));
    REQUIRE_THROWS(eng.CompileExpr("a @ b", nullptr));
}
