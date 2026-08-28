#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xexpr.h"
#include "xspcomm/xdata.h"
#include "xspcomm/xsignal_cfg.h"

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

TEST_CASE("ExprEngine parses hexadecimal integer literals", "[xexpr]") {
    ExprEngine eng;

    REQUIRE(eng.Eval(eng.CompileExpr("0xff", nullptr)) == 0xff);
    REQUIRE(eng.Eval(eng.CompileExpr("0XDEAD_BEEFULL", nullptr)) == 0xdeadbeefULL);
    REQUIRE(eng.Eval(eng.CompileExpr("0x10U + 1", nullptr)) == 0x11);
    REQUIRE_THROWS(eng.CompileExpr("0x1G", nullptr));
    REQUIRE_THROWS(eng.CompileExpr("0x1UUU", nullptr));
    REQUIRE_THROWS(eng.CompileExpr("0b", nullptr));
}

TEST_CASE("XSignalCFG parses hexadecimal const signals", "[xexpr][xsignal_cfg]") {
    const std::string cfg_text = R"yaml(
variables: []
signals:
  - name: hex_const
    kind: const
    type: IData
    rtl_width: 32
    value: "0xDEAD_BEEFU"
  - name: shifted_const
    kind: const
    type: IData
    rtl_width: 32
    value: "1U << 4U"
  - name: verilator_macro_const
    kind: const
    type: QData
    rtl_width: 64
    value: "VL_ULL(0x1234_ABCDULL)"
)yaml";

    XSignalCFG cfg(cfg_text);
    std::unique_ptr<XData> hex_signal(cfg.NewXData("hex_const"));
    std::unique_ptr<XData> shifted_signal(cfg.NewXData("shifted_const"));
    std::unique_ptr<XData> macro_signal(cfg.NewXData("verilator_macro_const"));

    REQUIRE(hex_signal != nullptr);
    REQUIRE(hex_signal->U() == 0xdeadbeefULL);
    REQUIRE(shifted_signal != nullptr);
    REQUIRE(shifted_signal->U() == 0x10);
    REQUIRE(macro_signal != nullptr);
    REQUIRE(macro_signal->U() == 0x1234abcdULL);
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
