#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xexpr.h"
#include "xspcomm/monitor/expr.h"
#include "xspcomm/xdata.h"
#include "xspcomm/xsignal_cfg.h"

using namespace xspcomm;

TEST_CASE("Expression collection preserves state and releases unreachable wide constants", "[xexpr]") {
    ExprEngine engine;
    engine.NewConst(999); // force retained roots to move
    XData signal(8, XData::InOut), wide(128, XData::InOut);
    signal = 1; wide = 1;
    const auto hold = engine.NewWithin(engine.NewSignal(&signal), 3);
    std::vector<unsigned char> value(16, 0); value[0] = 1;
    const auto comparison = engine.NewCompareSigConstBytes(ExprOp::EQ, &wide, value);
    engine.NewCompareSigConstBytes(ExprOp::EQ, &wide, value);
    engine.SetCycle(1); REQUIRE(engine.Eval(hold) == 1);
    signal = 0; engine.SetCycle(2);
    REQUIRE(engine.ConstantCount() == 2);
    const auto mapping = engine.Collect({hold, comparison});
    REQUIRE(engine.NodeCount() == 3);
    REQUIRE(engine.ConstantCount() == 1);
    REQUIRE(engine.Eval(mapping[hold]) == 1);
    REQUIRE(engine.Eval(mapping[comparison]) == 1);
    REQUIRE_THROWS(engine.Collect({999}));
    REQUIRE(engine.NodeCount() == 3);
    engine.Collect({});
    REQUIRE(engine.NodeCount() == 0);
    REQUIRE(engine.ConstantCount() == 0);
}

TEST_CASE("Typed expressions reject inconsistent native descriptors", "[xexpr]") {
    ExprEngine engine;
    std::vector<unsigned char> ff{255}, one{1}, wide(16, 0);
    wide[15] = 128;
    const int a = engine.NewBitsConst(8, false, ff);
    const int b = engine.NewBitsConst(8, false, one);
    const int s = engine.NewBits(ExprOp::CAST, a, -1, 8, true);
    const int w = engine.NewBitsConst(128, false, wide);
    REQUIRE(engine.EvalBits(engine.NewBits(ExprOp::ADD, a, b, 8, false)) == std::vector<unsigned char>{0});
    auto sign_extended = engine.EvalBits(engine.NewBits(ExprOp::CAST, s, -1, 128, true));
    REQUIRE(sign_extended == std::vector<unsigned char>(16, 255));
    REQUIRE_THROWS(engine.NewBits(ExprOp::ADD, a, s, 8, false));
    REQUIRE_THROWS(engine.NewBits(ExprOp::ADD, a, w, 8, false));
    REQUIRE_THROWS(engine.NewBits(ExprOp::EQ, a, b, 8, false));
    REQUIRE_THROWS(engine.NewBits(ExprOp::WITHIN, a, -1, 8, false));
    REQUIRE_THROWS(engine.NewBitsConst(7, false, ff));
    const int truth = engine.NewBool(true);
    REQUIRE_THROWS(engine.NewBits(ExprOp::ADD, truth, truth, 1, false));
    REQUIRE_THROWS(engine.NewBits(ExprOp::LT, truth, truth, 1, false));
    REQUIRE(engine.EvalBits(engine.NewBits(ExprOp::EQ, truth, truth, 1, false)) == std::vector<unsigned char>{1});
}

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

TEST_CASE("XExprCheck reports every expression hit in one call",
          "[xexpr][trigger]") {
    XExprCheck checker;
    checker.SetExpr("first", checker.ExprNewConst(1));
    checker.SetExpr("second", checker.ExprNewConst(1));

    checker.Call();
    const auto keys = checker.GetTriggeredExprKeys();

    REQUIRE(keys.size() == 2);
    REQUIRE(keys[0] == "first");
    REQUIRE(keys[1] == "second");
    REQUIRE(checker.GetCbCount() == 1);
}


TEST_CASE("ExprEngine masked comparison preserves wide bits and unknowns", "[xexpr]") {
    ExprEngine engine;
    XData signal(65, XData::InOut);
    std::vector<unsigned char> value(9, 0), mask(9, 0);
    value[0] = 1; value[8] = 1;
    mask[0] = 3; mask[8] = 1;
    const int root = engine.NewMaskedCompareSigConstBytes(&signal, value, mask);
    REQUIRE(root >= 0);
    signal = "0x10000000000000005";
    REQUIRE(engine.IsKnown(root));
    REQUIRE(engine.Eval(root) == 1);
    signal = "0x5";
    REQUIRE(engine.Eval(root) == 0);
    signal = "0x10000000000000006";
    REQUIRE(engine.Eval(root) == 0);
    signal = "0bx0000000000000000000000000000000000000000000000000000000000000001";
    REQUIRE_FALSE(engine.IsKnown(root));
    REQUIRE(engine.Eval(root) == 0);
    mask[8] = 2;
    REQUIRE(engine.NewMaskedCompareSigConstBytes(&signal, value, mask) == -1);
    mask.pop_back();
    REQUIRE(engine.NewMaskedCompareSigConstBytes(&signal, value, mask) == -1);
    engine.Clear();
    REQUIRE_FALSE(engine.ValidRoot(root));
}

TEST_CASE("Typed DAG reads shared signals once and refreshes every evaluation", "[xexpr][typed]") {
    XData signal(129, XData::InOut);
    unsigned current = 3, reads = 0, writes = 0;
    signal.BindDPIRW([&](void *storage) {
        ++reads;
        auto *words = static_cast<xsvLogicVecVal *>(storage);
        for (size_t i = 0; i < 5; ++i) words[i] = {i == 0 ? current : 0U, 0};
    }, [&](void *) { ++writes; });
    ExprEngine engine;
    engine.NewConst(999);
    const int value = engine.NewBitsSignal(&signal, false);
    const int doubled = engine.NewBits(ExprOp::ADD, value, value, 129, false);
    int root = engine.NewBits(ExprOp::MUL, doubled, doubled, 129, false);
    reads = writes = 0;
    REQUIRE(engine.Eval(root) == 36);
    REQUIRE(reads == 1);
    current = 4;
    REQUIRE(engine.Eval(root) == 64); // same cycle, different signal value
    REQUIRE(reads == 2);
    const auto mapping = engine.Collect({root});
    root = mapping[root];
    REQUIRE(engine.NodeCount() == 3);
    current = 5;
    REQUIRE(engine.Eval(root) == 100);
    REQUIRE(reads == 3);
    REQUIRE(writes == 0);
    engine.Clear();
}

TEST_CASE("Typed evaluation short circuits and recovers after arithmetic errors", "[xexpr][typed]") {
    ExprEngine engine;
    XData numerator(9, XData::InOut), denominator(9, XData::InOut);
    numerator = 12; denominator = 0;
    const int a = engine.NewBitsSignal(&numerator, true), b = engine.NewBitsSignal(&denominator, true);
    const int quotient = engine.NewBits(ExprOp::DIV, a, b, 9, true);
    const int zero = engine.NewBool(false), one = engine.NewBool(true);
    REQUIRE(engine.Eval(engine.NewBits(ExprOp::LAND, zero, quotient, 1, false)) == 0);
    REQUIRE(engine.Eval(engine.NewBits(ExprOp::LOR, one, quotient, 1, false)) == 1);
    REQUIRE_THROWS_AS(engine.Eval(quotient), std::domain_error);
    denominator = 2;
    REQUIRE(engine.Eval(quotient) == 6);
    denominator = 511; // -1 in signed 9-bit arithmetic
    REQUIRE(engine.Eval(quotient) == 500); // raw bits for -12
    const int shift = engine.NewBits(ExprOp::SHR, a, b, 9, true);
    REQUIRE_THROWS_AS(engine.Eval(shift), std::domain_error);
    denominator = 9;
    REQUIRE(engine.Eval(shift) == 0);
}

TEST_CASE("Typed evaluation uses an explicit stack for deep shared programs", "[xexpr][typed]") {
    ExprEngine engine;
    std::vector<unsigned char> one{1};
    int root = engine.NewBitsConst(8, false, one);
    for (int i = 0; i < 10000; ++i) root = engine.NewBits(ExprOp::ADD, root, root, 8, false);
    REQUIRE(engine.Eval(root) == 0);
    const auto mapping = engine.Collect({root});
    REQUIRE(engine.Eval(mapping[root]) == 0);
}
