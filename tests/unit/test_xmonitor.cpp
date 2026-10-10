#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xmonitor.h"
#include "xspcomm/xclock.h"
#include "xspcomm/xdata.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

using namespace xspcomm;

namespace {
struct StepCbProbe : public XStepCallback {
    int calls = 0;
    void Call() override {
        calls++;
        IncCbCount();
    }
};
}

TEST_CASE("XStepCallback basic behavior", "[xmonitor_base]") {
    StepCbProbe cb;
    REQUIRE(cb.IsDisable() == false);
    REQUIRE(cb.GetCbCount() == 0);

    cb.Disable();
    REQUIRE(cb.IsDisable() == true);
    cb.Enable();
    REQUIRE(cb.IsDisable() == false);

    cb.SetMaxCbs(2);
    auto fn = (void (*)(uint64_t, void*))XStepCallback::GetCb();
    fn(1, &cb);
    REQUIRE(cb.calls == 1);
    REQUIRE(cb.GetCbCount() == 1);
    REQUIRE(cb.IsDisable() == false);

    fn(2, &cb);
    REQUIRE(cb.calls == 2);
    REQUIRE(cb.GetCbCount() == 2);
    REQUIRE(cb.IsDisable() == true);

    fn(3, &cb);
    REQUIRE(cb.calls == 2);
    REQUIRE(cb.GetCbCount() == 2);
}

TEST_CASE("XConditionCheck xdata conditions", "[xmonitor_base]") {
    XData a(8, XData::InOut);
    XData b(8, XData::InOut);
    XClock clk([](bool){ return 0; });

    a = 1;
    b = 1;
    XConditionCheck checker(&clk);
    checker.SetCondition("eq", &a, &b, CompareOp::EQ);
    auto fn = (void (*)(uint64_t, void*))XStepCallback::GetCb();
    fn(1, &checker);

    auto keys = checker.GetTriggeredConditionKeys();
    REQUIRE(keys.size() == 1);
    REQUIRE(keys[0] == "eq");
    REQUIRE(checker.ListCondition()["eq"] == true);
    REQUIRE(clk.IsDisable() == true);
}

TEST_CASE("XConditionCheck valid gating", "[xmonitor_base]") {
    XData a(8, XData::InOut);
    XData b(8, XData::InOut);
    XData valid(1, XData::InOut);
    XData valid_val(1, XData::InOut);

    a = 1;
    b = 1;
    valid = 0;
    valid_val = 1;

    XConditionCheck checker;
    checker.SetCondition("eq", &a, &b, CompareOp::EQ, &valid, &valid_val);
    auto fn = (void (*)(uint64_t, void*))XStepCallback::GetCb();

    fn(1, &checker);
    REQUIRE(checker.GetTriggeredConditionKeys().empty());

    valid = 1;
    fn(2, &checker);
    REQUIRE(checker.GetTriggeredConditionKeys().size() == 1);
}

TEST_CASE("XConditionCheck pointer conditions", "[xmonitor_base]") {
    uint32_t lhs = 5;
    uint32_t rhs = 7;
    XConditionCheck checker;
    checker.SetCondition("gt", (uint64_t)&rhs, (uint64_t)&lhs, CompareOp::GT, sizeof(uint32_t));

    auto fn = (void (*)(uint64_t, void*))XStepCallback::GetCb();
    fn(1, &checker);
    REQUIRE(checker.GetTriggeredConditionKeys().size() == 1);

    int8_t neg = -1;
    int8_t pos = 1;
    XConditionCheck checker2;
    checker2.SetCondition("lt", (uint64_t)&neg, (uint64_t)&pos, CompareOp::LT, sizeof(int8_t));
    fn(2, &checker2);
    REQUIRE(checker2.GetTriggeredConditionKeys().size() == 1);
}

TEST_CASE("XByteBuffer and helpers", "[xmonitor_base]") {
    XByteBuffer arr(10);
    REQUIRE(arr.Size() == 10);
    arr.SetZero();
    auto bytes = arr.AsBytes();
    REQUIRE(bytes.size() == 10);

    std::vector<unsigned char> input = {1, 2, 3, 4, 5};
    arr.FromBytes(input);
    auto out = arr.AsBytes();
    REQUIRE(out[0] == 1);
    REQUIRE(out[4] == 5);

    auto copy = std::unique_ptr<XByteBuffer>(arr.Copy());
    REQUIRE((*copy) == arr);

    unsigned char buf[4] = {9, 8, 7, 6};
    arr.SyncFrom((uint64_t)buf, 4);
    auto out2 = arr.AsBytes();
    REQUIRE(out2[0] == 9);

    uint32_t u32s[2] = {0, 0};
    SetU32Array((uint64_t)u32s, 1, 0x1234);
    REQUIRE(GetFromU32Array((uint64_t)u32s, 1) == 0x1234);
}

TEST_CASE("XRangeCheck", "[xmonitor_base]") {
    REQUIRE(XRangeCheck::cmp(10, 12, 2) == true);
    REQUIRE(XRangeCheck::cmp(10, 12, -2) == false);

    uint64_t a = 10;
    uint64_t b = 12;
    XRangeCheck rc(2, 8);
    auto fn = rc.GetArrayCmp();
    REQUIRE(fn((uint64_t)&a, (uint64_t)&b, rc.CSelf()) == true);
}

TEST_CASE("XRangeCheck reads exactly its byte count", "[xmonitor_base]") {
    for (int bytes = 1; bytes <= 8; ++bytes) {
        // Exact allocations expose overreads; the offset also tests alignment.
        auto a = std::make_unique<unsigned char[]>(bytes + 1);
        auto b = std::make_unique<unsigned char[]>(bytes + 1);
        std::memset(a.get(), 0x80, bytes + 1);
        std::memset(b.get(), 0x80, bytes + 1);
        a[1] = 4;
        b[1] = 6;
        auto lhs = reinterpret_cast<uint64_t>(a.get() + 1);
        auto rhs = reinterpret_cast<uint64_t>(b.get() + 1);
        XRangeCheck rc(2, bytes);
        auto compare = rc.GetArrayCmp();
        REQUIRE(compare(lhs, rhs, rc.CSelf()));
        REQUIRE(XRangeCheck::ArrayCmp(lhs, rhs, rc.CSelf()));
        b[1] = 7;
        REQUIRE_FALSE(compare(lhs, rhs, rc.CSelf()));
        REQUIRE_FALSE(XRangeCheck::ArrayCmp(lhs, rhs, rc.CSelf()));
        b[1] = 4;
        REQUIRE(compare(lhs, rhs, rc.CSelf()));
    }
}

TEST_CASE("XRangeCheck does not wrap at integer boundaries", "[xmonitor_base]") {
    REQUIRE(XRangeCheck::cmp(0, 0, 2));
    REQUIRE(XRangeCheck::cmp(0, 1, 2));
    REQUIRE_FALSE(XRangeCheck::cmp(UINT64_MAX, 0, 2));
    REQUIRE(XRangeCheck::cmp(UINT64_MAX, UINT64_MAX, -2));
    REQUIRE_FALSE(XRangeCheck::cmp(0, UINT64_MAX, -2));
    REQUIRE(XRangeCheck::cmp(uint64_t(1) << 31, 0, std::numeric_limits<int>::min()));
    REQUIRE_FALSE(XRangeCheck::cmp((uint64_t(1) << 31) + 1, 0, std::numeric_limits<int>::min()));
}

TEST_CASE("XRangeCheck compares full wide buffers and signals", "[xmonitor_base]") {
    struct Case { const char *target; const char *center; int range; bool expected; };
    const Case cases[] = {
        {"0x0", "0x10000000000000000", 0, false},
        {"0xffffffffffffffff", "0x10000000000000000", 1, true},
        {"0xfffffffffffffffe", "0x10000000000000000", 1, false},
        {"0x10000000000000000", "0xffffffffffffffff", -1, true},
        {"0x10000000000000001", "0xffffffffffffffff", -1, false},
        {"0x10000000000000001", "0x10000000000000000", 2, false},
        {"0x10000000000000001", "0x10000000000000001", 0, true},
        {"0x0", "0x1", 2, true},
        {"0x80000000", "0x0", std::numeric_limits<int>::min(), true},
        {"0x80000001", "0x0", std::numeric_limits<int>::min(), false},
    };
    for (int bytes = 9; bytes <= 33; ++bytes) {
        auto lhs = std::make_unique<unsigned char[]>(bytes + 1);
        auto rhs = std::make_unique<unsigned char[]>(bytes + 1);
        XData a(bytes * 8, XData::InOut), b(bytes * 8, XData::InOut);
        for (const auto &test : cases) {
            a.Set(test.target);
            b.Set(test.center);
            const auto av = a.GetBytes(), bv = b.GetBytes();
            std::copy_n(av.begin(), bytes, lhs.get() + 1);
            std::copy_n(bv.begin(), bytes, rhs.get() + 1);
            XRangeCheck check(test.range, bytes);
            const auto x = reinterpret_cast<uint64_t>(lhs.get() + 1);
            const auto y = reinterpret_cast<uint64_t>(rhs.get() + 1);
            auto array_compare = check.GetArrayCmp();
            auto signal_compare = check.GetXDataCmp();
            REQUIRE(array_compare(x, y, check.CSelf()) == test.expected);
            REQUIRE(XRangeCheck::ArrayCmp(x, y, check.CSelf()) == test.expected);
            REQUIRE(signal_compare(&a, &b, check.CSelf()) == test.expected);
        }
        // Borrow must propagate through every word, including a partial last word.
        std::memset(lhs.get() + 1, 0xFF, bytes);
        std::memset(rhs.get() + 1, 0, bytes);
        lhs[bytes] = 0;
        rhs[bytes] = 1;
        XRangeCheck check(1, bytes);
        auto compare = check.GetArrayCmp();
        const auto x = reinterpret_cast<uint64_t>(lhs.get() + 1);
        const auto y = reinterpret_cast<uint64_t>(rhs.get() + 1);
        REQUIRE(compare(x, y, check.CSelf()));
        rhs[bytes] = 2;
        REQUIRE_FALSE(compare(x, y, check.CSelf()));
        std::memset(lhs.get() + 1, 0xFF, bytes);
        std::memset(rhs.get() + 1, 0, bytes);
        REQUIRE_FALSE(compare(x, y, check.CSelf()));
        XRangeCheck above(-1, bytes);
        REQUIRE_FALSE(XRangeCheck::ArrayCmp(y, x, above.CSelf()));
    }
}

TEST_CASE("XRangeCheck uses signal width and rejects unknowns", "[xmonitor_base]") {
    // The byte count configures pointer comparisons; XData uses its actual width.
    XRangeCheck check(1, 8);
    auto compare = check.GetXDataCmp();
    XData narrow(8, XData::InOut);
    narrow.Set(1);
    for (unsigned width : {65U, 127U, 128U, 129U, 256U}) {
        XData a(width, XData::InOut), b(width, XData::InOut);
        a.Set("0xffffffffffffffff");
        b.Set("0x10000000000000000");
        REQUIRE((a < b));
        REQUIRE(compare(&a, &b, check.CSelf()));
        a.Set("0x10000000000000001");
        b.Set(1);
        REQUIRE((a > b));
        REQUIRE_FALSE(compare(&a, &b, check.CSelf()));
        a.Set(0);
        REQUIRE(compare(&a, &narrow, check.CSelf()));
        const std::string maximum = "0x" + std::string((width + 3) / 4, 'f');
        a.Set(maximum.c_str());
        b.Set(0);
        REQUIRE_FALSE(compare(&a, &b, check.CSelf()));
        a.Set(0);
        a.pVecData[(width - 1) / 32].bval = 1;
        REQUIRE_FALSE(compare(&a, &b, check.CSelf()));
    }
    narrow.Set("x");
    REQUIRE_FALSE(compare(&narrow, &narrow, check.CSelf()));
}

TEST_CASE("XRangeCheck refreshes each wide backend once without writing", "[xmonitor_base]") {
    XData a(129, XData::InOut), b(129, XData::InOut);
    uint32_t av[] = {UINT32_MAX, UINT32_MAX, 0, 0, 0};
    uint32_t bv[] = {0, 0, 1, 0, 0};
    int reads_a = 0, reads_b = 0, writes = 0;
    auto bind = [&](XData &signal, uint32_t *words, int &reads) {
        signal.BindDPIRW([words, &reads](void *data){
            ++reads;
            auto *vec = (xsvLogicVecVal*)data;
            for (int i = 0; i < 5; ++i) vec[i] = {words[i], 0};
        }, [&](void *){ ++writes; });
    };
    bind(a, av, reads_a);
    bind(b, bv, reads_b);
    reads_a = reads_b = 0;
    XRangeCheck check(1, 17);
    auto compare = check.GetXDataCmp();
    REQUIRE(compare(&a, &b, check.CSelf()));
    REQUIRE(reads_a == 1);
    REQUIRE(reads_b == 1);
    REQUIRE(writes == 0);
    bv[4] = 1;
    REQUIRE_FALSE(compare(&a, &b, check.CSelf()));
    REQUIRE(reads_a == 2);
    REQUIRE(reads_b == 2);
    REQUIRE(writes == 0);
}

TEST_CASE("CString basic", "[xmonitor_base]") {
    CString s("abc");
    REQUIRE(s.Get() == "abc");
    s.Set("def");
    REQUIRE(s.Get() == "def");
    s.AssignFrom("xyz");
    REQUIRE(s.Get() == "xyz");
    REQUIRE(s.CharAddress() != 0);
}

TEST_CASE("XEcho smoke", "[xmonitor_base]") {
    XData valid(1, XData::InOut);
    XData data(8, XData::InOut);
    valid = 1;
    data = 'A';
    XEcho echo(valid.CSelf(), data.CSelf(), false, "%c", 0);
    echo.Call();
}


TEST_CASE("Pointer conditions order signed values at every buffer width", "[xmonitor_base][compare]") {
    for (int bytes : {1, 2, 3, 4, 5, 7, 8, 9, 16, 33, 65}) {
        auto lhs = std::make_unique<unsigned char[]>(bytes + 1);
        auto rhs = std::make_unique<unsigned char[]>(bytes + 1);
        auto store = [bytes](unsigned char* data, int value) {
            std::fill_n(data, bytes, value < 0 ? 0xff : 0);
            const uint64_t bits = static_cast<uint64_t>(static_cast<int64_t>(value));
            std::memcpy(data, &bits, std::min(static_cast<size_t>(bytes), sizeof(bits)));
        };
        XConditionCheck checker;
        for (int a : {-128, -2, -1, 0, 1, 127}) {
            for (int b : {-128, -2, -1, 0, 1, 127}) {
                store(lhs.get() + 1, a);
                store(rhs.get() + 1, b);
                const std::pair<CompareOp, bool> cases[] = {
                    {CompareOp::EQ, a == b}, {CompareOp::NE, a != b},
                    {CompareOp::GT, a > b}, {CompareOp::GE, a >= b},
                    {CompareOp::LT, a < b}, {CompareOp::LE, a <= b},
                };
                for (auto [operation, expected] : cases) {
                    INFO("bytes=" << bytes << ", a=" << a << ", b=" << b << ", op=" << static_cast<int>(operation));
                    checker.SetCondition("compare", reinterpret_cast<uint64_t>(lhs.get() + 1),
                                         reinterpret_cast<uint64_t>(rhs.get() + 1), operation, bytes);
                    checker.Call();
                    REQUIRE(checker.ListCondition().at("compare") == expected);
                }
            }
        }
    }
}

TEST_CASE("XData equality preserves zero extension and four-state masks", "[xmonitor_base][compare]") {
    for (unsigned width : {64U, 65U, 127U, 129U, 513U}) {
        XData wide(width, XData::InOut), peer(width, XData::InOut), narrow(8, XData::InOut);
        wide.Set(0x5a); peer.Set(0x5a); narrow.Set(0x5a);
        REQUIRE((wide == peer));
        REQUIRE((wide == narrow));
        REQUIRE((narrow == wide));
        REQUIRE((wide == uint64_t(0x5a)));
        wide[width - 1] = 1;
        REQUIRE_FALSE((wide == peer));
        REQUIRE_FALSE((wide == narrow));
        REQUIRE_FALSE((wide == uint64_t(0x5a)));
        wide[width - 1] = "x";
        peer[width - 1] = "x";
        REQUIRE((wide == peer));
        REQUIRE_FALSE((wide == narrow));
        REQUIRE_FALSE((wide == uint64_t(0x5a)));
        XConditionCheck checker;
        checker.SetCondition("eq", &wide, &peer, CompareOp::EQ);
        checker.SetCondition("ne", &wide, &peer, CompareOp::NE);
        checker.Call();
        REQUIRE(checker.ListCondition().at("eq"));
        REQUIRE_FALSE(checker.ListCondition().at("ne"));
        peer[width - 1] = "z";
        checker.Call();
        REQUIRE_FALSE(checker.ListCondition().at("eq"));
        REQUIRE(checker.ListCondition().at("ne"));
    }
}

TEST_CASE("Managed Monitor callbacks detach with either lifetime order", "[xmonitor_base][lifetime]") {
    XClock clock([](bool) { return 0; });
    {
        auto callback = std::make_unique<StepCbProbe>();
        callback->Attach(&clock);
        callback->Attach(&clock);
        REQUIRE(clock.StepRisQueueSize() == 1);
        clock.Step();
        REQUIRE(callback->calls == 1);
    }
    REQUIRE(clock.StepRisQueueSize() == 0);
    clock.Step();

    StepCbProbe callback;
    {
        XClock temporary([](bool) { return 0; });
        callback.Attach(&temporary, false);
        temporary.Step();
        REQUIRE(callback.calls == 1);
    }
    callback.Detach();
}

TEST_CASE("Managed callback removal during sampling preserves iteration", "[xmonitor_base][lifetime]") {
    XClock clock([](bool) { return 0; });
    auto callback = std::make_unique<StepCbProbe>();
    clock.StepRis([&](uint64_t, void *) { callback.reset(); });
    callback->Attach(&clock);
    clock.Step();
    REQUIRE_FALSE(callback);
    REQUIRE(clock.StepRisQueueSize() == 1);
    clock.Step();
}

TEST_CASE("Copied clocks cannot retain destroyed managed callbacks", "[xmonitor_base][lifetime]") {
    auto original = std::make_unique<XClock>([](bool) { return 0; });
    auto callback = std::make_unique<StepCbProbe>();
    callback->Attach(original.get());
    XClock copy = *original;
    original.reset();
    copy.Step();
    REQUIRE(callback->calls == 1);
    callback.reset();
    copy.Step();
}

TEST_CASE("Expression and comparison checkers share registration and hit handling", "[xmonitor_base][xexpr]") {
    XClock clock([](bool) { return 0; });
    XData left(129, XData::InOut), right(129, XData::InOut);
    left = right = "0x100000000000000000000000000000001";
    XExprCheck checker(&clock);
    checker.SetCondition("wide", &left, &right, CompareOp::EQ);
    checker.SetExpr("expression", checker.ExprNewCompareSigSig(static_cast<int>(ExprOp::EQ), &left, &right));
    checker.Call();
    REQUIRE(checker.GetTriggeredExprKeys().size() == 2);
    REQUIRE(checker.GetCbCount() == 1);
    REQUIRE(clock.IsDisable());

    checker.SetExpr("wide", checker.ExprNewConst(0));
    checker.Call();
    REQUIRE_FALSE(checker.ListCondition().at("wide"));
    REQUIRE(checker.GetTriggeredExprKeys().size() == 1);

    int8_t negative = -1, positive = 1;
    checker.SetCondition("expression", reinterpret_cast<uint64_t>(&negative),
                         reinterpret_cast<uint64_t>(&positive), CompareOp::LT, 1);
    checker.Call();
    REQUIRE(checker.ListExpr().at("expression"));
    checker.RemoveExpr("expression");
    REQUIRE(checker.ListCondition().size() == 1);
    checker.ClearExpr();
    REQUIRE(checker.ListCondition().empty());
}

TEST_CASE("Expression hits clear when evaluated again in the same cycle", "[xmonitor_base][xexpr]") {
    XExprCheck checker;
    checker.SetExpr("condition", checker.ExprNewConst(1));
    checker.Call();
    REQUIRE(checker.ListExpr().at("condition"));
    checker.SetExpr("condition", checker.ExprNewConst(0));
    checker.Call();
    REQUIRE_FALSE(checker.ListExpr().at("condition"));
    REQUIRE(checker.GetTriggeredExprKeys().empty());
}

TEST_CASE("Unified checks retain four-state equality and first-hit clock behavior", "[xmonitor_base][xexpr]") {
    XClock clock([](bool) { return 0; });
    XData left(129, XData::InOut), right(129, XData::InOut);
    left = right = "x";
    XExprCheck checker(&clock);
    checker.SetCondition("compare", &left, &right, CompareOp::EQ);
    checker.SetExpr("expression", checker.ExprNewCompareSigSig(static_cast<int>(ExprOp::EQ), &left, &right));
    bool observed_disabled = false;
    checker.SetCondition("callback", uint64_t(0), uint64_t(0), CompareOp::EQ, 0, 0, 0, 1,
                         [&](uint64_t, uint64_t, uint64_t) { observed_disabled = clock.IsDisable(); return true; });
    checker.Call();
    REQUIRE(checker.GetTriggeredExprKeys().size() == 3);
    REQUIRE(observed_disabled);
    REQUIRE(checker.GetCbCount() == 1);
}

TEST_CASE("Invalid raw comparison registration is rejected before sampling", "[xmonitor_base]") {
    XConditionCheck checker;
    uint64_t left = 1, right = 2;
    REQUIRE_THROWS_AS(checker.SetCondition("invalid", reinterpret_cast<uint64_t>(&left),
        reinterpret_cast<uint64_t>(&right), CompareOp::EQ, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(checker.SetCondition("invalid", reinterpret_cast<uint64_t>(&left),
        reinterpret_cast<uint64_t>(&right), static_cast<CompareOp>(99), 8), std::invalid_argument);
}

TEST_CASE("Raw array helpers preserve unaligned accesses and signed offsets", "[xmonitor_base][memory]") {
    unsigned char storage[18];
    std::memset(storage, 0xA5, sizeof(storage));
    const auto middle = reinterpret_cast<uint64_t>(storage + 9);
    SetU64Array(middle, -1, UINT64_C(0x123456789ABCDEF0));
    REQUIRE(GetFromU64Array(middle, -1) == UINT64_C(0x123456789ABCDEF0));
    REQUIRE(storage[0] == 0xA5);
    REQUIRE(storage[9] == 0xA5);
    SetU32Array(middle, 0, 0x12345678);
    REQUIRE(GetFromU32Array(middle, 0) == 0x12345678);
    REQUIRE(storage[13] == 0xA5);
    REQUIRE(storage[17] == 0xA5);
}
