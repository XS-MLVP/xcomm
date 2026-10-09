#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xcomuse.h"
#include "xspcomm/xclock.h"
#include "xspcomm/xdata.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <memory>
#include <utility>

using namespace xspcomm;

namespace {
struct StepCbProbe : public ComUseStepCb {
    int calls = 0;
    void Call() override {
        calls++;
        IncCbCount();
    }
};
}

TEST_CASE("ComUseStepCb basic behavior", "[xcomuse_base]") {
    StepCbProbe cb;
    REQUIRE(cb.IsDisable() == false);
    REQUIRE(cb.GetCbCount() == 0);

    cb.Disable();
    REQUIRE(cb.IsDisable() == true);
    cb.Enable();
    REQUIRE(cb.IsDisable() == false);

    cb.SetMaxCbs(2);
    auto fn = (void (*)(uint64_t, void*))ComUseStepCb::GetCb();
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

TEST_CASE("ComUseCondCheck xdata conditions", "[xcomuse_base]") {
    XData a(8, XData::InOut);
    XData b(8, XData::InOut);
    XClock clk([](bool){ return 0; });

    a = 1;
    b = 1;
    ComUseCondCheck checker(&clk);
    checker.SetCondition("eq", &a, &b, ComUseCondCmp::EQ);
    auto fn = (void (*)(uint64_t, void*))ComUseStepCb::GetCb();
    fn(1, &checker);

    auto keys = checker.GetTriggeredConditionKeys();
    REQUIRE(keys.size() == 1);
    REQUIRE(keys[0] == "eq");
    REQUIRE(checker.ListCondition()["eq"] == true);
    REQUIRE(clk.IsDisable() == true);
}

TEST_CASE("ComUseCondCheck valid gating", "[xcomuse_base]") {
    XData a(8, XData::InOut);
    XData b(8, XData::InOut);
    XData valid(1, XData::InOut);
    XData valid_val(1, XData::InOut);

    a = 1;
    b = 1;
    valid = 0;
    valid_val = 1;

    ComUseCondCheck checker;
    checker.SetCondition("eq", &a, &b, ComUseCondCmp::EQ, &valid, &valid_val);
    auto fn = (void (*)(uint64_t, void*))ComUseStepCb::GetCb();

    fn(1, &checker);
    REQUIRE(checker.GetTriggeredConditionKeys().empty());

    valid = 1;
    fn(2, &checker);
    REQUIRE(checker.GetTriggeredConditionKeys().size() == 1);
}

TEST_CASE("ComUseCondCheck pointer conditions", "[xcomuse_base]") {
    uint32_t lhs = 5;
    uint32_t rhs = 7;
    ComUseCondCheck checker;
    checker.SetCondition("gt", (uint64_t)&rhs, (uint64_t)&lhs, ComUseCondCmp::GT, sizeof(uint32_t));

    auto fn = (void (*)(uint64_t, void*))ComUseStepCb::GetCb();
    fn(1, &checker);
    REQUIRE(checker.GetTriggeredConditionKeys().size() == 1);

    int8_t neg = -1;
    int8_t pos = 1;
    ComUseCondCheck checker2;
    checker2.SetCondition("lt", (uint64_t)&neg, (uint64_t)&pos, ComUseCondCmp::LT, sizeof(int8_t));
    fn(2, &checker2);
    REQUIRE(checker2.GetTriggeredConditionKeys().size() == 1);
}

TEST_CASE("ComUseDataArray and helpers", "[xcomuse_base]") {
    ComUseDataArray arr(10);
    REQUIRE(arr.Size() == 10);
    arr.SetZero();
    auto bytes = arr.AsBytes();
    REQUIRE(bytes.size() == 10);

    std::vector<unsigned char> input = {1, 2, 3, 4, 5};
    arr.FromBytes(input);
    auto out = arr.AsBytes();
    REQUIRE(out[0] == 1);
    REQUIRE(out[4] == 5);

    auto copy = std::unique_ptr<ComUseDataArray>(arr.Copy());
    REQUIRE((*copy) == arr);

    unsigned char buf[4] = {9, 8, 7, 6};
    arr.SyncFrom((uint64_t)buf, 4);
    auto out2 = arr.AsBytes();
    REQUIRE(out2[0] == 9);

    uint32_t u32s[2] = {0, 0};
    SetU32Array((uint64_t)u32s, 1, 0x1234);
    REQUIRE(GetFromU32Array((uint64_t)u32s, 1) == 0x1234);
}

TEST_CASE("ComUseRangeCheck", "[xcomuse_base]") {
    REQUIRE(ComUseRangeCheck::cmp(10, 12, 2) == true);
    REQUIRE(ComUseRangeCheck::cmp(10, 12, -2) == false);

    uint64_t a = 10;
    uint64_t b = 12;
    ComUseRangeCheck rc(2, 8);
    auto fn = rc.GetArrayCmp();
    REQUIRE(fn((uint64_t)&a, (uint64_t)&b, rc.CSelf()) == true);
}

TEST_CASE("ComUseRangeCheck reads exactly its byte count", "[xcomuse_base]") {
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
        ComUseRangeCheck rc(2, bytes);
        auto compare = rc.GetArrayCmp();
        REQUIRE(compare(lhs, rhs, rc.CSelf()));
        REQUIRE(ComUseRangeCheck::ArrayCmp(lhs, rhs, rc.CSelf()));
        b[1] = 7;
        REQUIRE_FALSE(compare(lhs, rhs, rc.CSelf()));
        REQUIRE_FALSE(ComUseRangeCheck::ArrayCmp(lhs, rhs, rc.CSelf()));
        b[1] = 4;
        REQUIRE(compare(lhs, rhs, rc.CSelf()));
    }
}

TEST_CASE("ComUseRangeCheck does not wrap at integer boundaries", "[xcomuse_base]") {
    REQUIRE(ComUseRangeCheck::cmp(0, 0, 2));
    REQUIRE(ComUseRangeCheck::cmp(0, 1, 2));
    REQUIRE_FALSE(ComUseRangeCheck::cmp(UINT64_MAX, 0, 2));
    REQUIRE(ComUseRangeCheck::cmp(UINT64_MAX, UINT64_MAX, -2));
    REQUIRE_FALSE(ComUseRangeCheck::cmp(0, UINT64_MAX, -2));
    REQUIRE(ComUseRangeCheck::cmp(uint64_t(1) << 31, 0, std::numeric_limits<int>::min()));
    REQUIRE_FALSE(ComUseRangeCheck::cmp((uint64_t(1) << 31) + 1, 0, std::numeric_limits<int>::min()));
}

TEST_CASE("ComUseRangeCheck compares full wide buffers and signals", "[xcomuse_base]") {
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
            ComUseRangeCheck check(test.range, bytes);
            const auto x = reinterpret_cast<uint64_t>(lhs.get() + 1);
            const auto y = reinterpret_cast<uint64_t>(rhs.get() + 1);
            auto array_compare = check.GetArrayCmp();
            auto signal_compare = check.GetXDataCmp();
            REQUIRE(array_compare(x, y, check.CSelf()) == test.expected);
            REQUIRE(ComUseRangeCheck::ArrayCmp(x, y, check.CSelf()) == test.expected);
            REQUIRE(signal_compare(&a, &b, check.CSelf()) == test.expected);
        }
        // Borrow must propagate through every word, including a partial last word.
        std::memset(lhs.get() + 1, 0xFF, bytes);
        std::memset(rhs.get() + 1, 0, bytes);
        lhs[bytes] = 0;
        rhs[bytes] = 1;
        ComUseRangeCheck check(1, bytes);
        auto compare = check.GetArrayCmp();
        const auto x = reinterpret_cast<uint64_t>(lhs.get() + 1);
        const auto y = reinterpret_cast<uint64_t>(rhs.get() + 1);
        REQUIRE(compare(x, y, check.CSelf()));
        rhs[bytes] = 2;
        REQUIRE_FALSE(compare(x, y, check.CSelf()));
        std::memset(lhs.get() + 1, 0xFF, bytes);
        std::memset(rhs.get() + 1, 0, bytes);
        REQUIRE_FALSE(compare(x, y, check.CSelf()));
        ComUseRangeCheck above(-1, bytes);
        REQUIRE_FALSE(ComUseRangeCheck::ArrayCmp(y, x, above.CSelf()));
    }
}

TEST_CASE("ComUseRangeCheck uses signal width and rejects unknowns", "[xcomuse_base]") {
    // The byte count configures pointer comparisons; XData uses its actual width.
    ComUseRangeCheck check(1, 8);
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

TEST_CASE("ComUseRangeCheck refreshes each wide backend once without writing", "[xcomuse_base]") {
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
    ComUseRangeCheck check(1, 17);
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

TEST_CASE("CString basic", "[xcomuse_base]") {
    CString s("abc");
    REQUIRE(s.Get() == "abc");
    s.Set("def");
    REQUIRE(s.Get() == "def");
    s.AssignFrom("xyz");
    REQUIRE(s.Get() == "xyz");
    REQUIRE(s.CharAddress() != 0);
}

TEST_CASE("ComUseEcho smoke", "[xcomuse_base]") {
    XData valid(1, XData::InOut);
    XData data(8, XData::InOut);
    valid = 1;
    data = 'A';
    ComUseEcho echo(valid.CSelf(), data.CSelf(), false, "%c", 0);
    echo.Call();
}


TEST_CASE("Pointer conditions order signed values at every buffer width", "[xcomuse_base][compare]") {
    for (int bytes : {1, 2, 3, 4, 5, 7, 8, 9, 16, 33, 65}) {
        auto lhs = std::make_unique<unsigned char[]>(bytes + 1);
        auto rhs = std::make_unique<unsigned char[]>(bytes + 1);
        auto store = [bytes](unsigned char* data, int value) {
            std::fill_n(data, bytes, value < 0 ? 0xff : 0);
            const uint64_t bits = static_cast<uint64_t>(static_cast<int64_t>(value));
            std::memcpy(data, &bits, std::min(static_cast<size_t>(bytes), sizeof(bits)));
        };
        ComUseCondCheck checker;
        for (int a : {-128, -2, -1, 0, 1, 127}) {
            for (int b : {-128, -2, -1, 0, 1, 127}) {
                store(lhs.get() + 1, a);
                store(rhs.get() + 1, b);
                const std::pair<ComUseCondCmp, bool> cases[] = {
                    {ComUseCondCmp::EQ, a == b}, {ComUseCondCmp::NE, a != b},
                    {ComUseCondCmp::GT, a > b}, {ComUseCondCmp::GE, a >= b},
                    {ComUseCondCmp::LT, a < b}, {ComUseCondCmp::LE, a <= b},
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

TEST_CASE("XData equality preserves zero extension and four-state masks", "[xcomuse_base][compare]") {
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
        ComUseCondCheck checker;
        checker.SetCondition("eq", &wide, &peer, ComUseCondCmp::EQ);
        checker.SetCondition("ne", &wide, &peer, ComUseCondCmp::NE);
        checker.Call();
        REQUIRE(checker.ListCondition().at("eq"));
        REQUIRE_FALSE(checker.ListCondition().at("ne"));
        peer[width - 1] = "z";
        checker.Call();
        REQUIRE_FALSE(checker.ListCondition().at("eq"));
        REQUIRE(checker.ListCondition().at("ne"));
    }
}
