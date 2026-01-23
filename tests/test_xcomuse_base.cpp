#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xcomuse_base.h"
#include "xspcomm/xclock.h"
#include "xspcomm/xdata.h"

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
