#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "xspcomm/xcomm.h"
#include "xspcomm/detail/array_binding.h"
#include <climits>
#include <cstring>
#include <stdexcept>

using namespace xspcomm;
using detail::ArrayBinding;

namespace {
xsvLogicVecVal dpi_values[18][3]{};
xsvLogic dpi_bits[6]{};
int reads = 0, writes = 0, selected = -1;
void get_vector(int index, void *value) {
    ++reads; selected = index;
    std::memcpy(value, dpi_values[index], sizeof(dpi_values[index]));
}
void set_vector(int index, const void *value) {
    ++writes; selected = index;
    std::memcpy(dpi_values[index], value, sizeof(dpi_values[index]));
}
void get_bit(int index, void *value) { *static_cast<xsvLogic *>(value) = dpi_bits[index]; }
void set_bit(int index, xsvLogic value) { dpi_bits[index] = value; }
}

TEST_CASE("array bounds and mixed-direction multidimensional indexing", "[xarray]") {
    ArrayBinding array(8, {-1, 3, 4}, {1, 2, 6});
    REQUIRE(array.Size() == 18);
    REQUIRE(array.Shape() == std::vector<int>{3, 2, 3});
    REQUIRE(array.FlatIndex({0, 2, 5}) == 10);
    for (uint64_t i = 0; i < array.Size(); ++i) REQUIRE(array.FlatIndex(array.Indices(i)) == i);
    REQUIRE_THROWS_AS(array.FlatIndex({0, 2}), std::invalid_argument);
    REQUIRE_THROWS_AS(array.FlatIndex({0, 1, 5}), std::out_of_range);
    REQUIRE_THROWS_AS(array.Indices(18), std::out_of_range);
    REQUIRE_THROWS_AS(ArrayBinding(8, {}, {}), std::invalid_argument);
    REQUIRE_THROWS_AS(ArrayBinding(8, {0}, {1, 2}), std::invalid_argument);
    REQUIRE_THROWS_AS(ArrayBinding(8, {INT_MIN}, {INT_MAX}), std::invalid_argument);
    REQUIRE_THROWS_AS(ArrayBinding(8, {0, 0, 0}, {INT_MAX-1, INT_MAX-1, INT_MAX-1}), std::overflow_error);
}

TEST_CASE("indexed DPI is shared, four-state, and respects write timing", "[xarray]") {
    std::memset(dpi_values, 0, sizeof(dpi_values));
    reads = writes = 0;
    ArrayBinding array(65, {-1, 3, 4}, {1, 2, 6});
    array.BindDPIPtr(reinterpret_cast<uint64_t>(get_vector), reinterpret_cast<uint64_t>(set_vector));
    auto leaf = array.At({0, 2, 5});
    REQUIRE(leaf == array.AtFlat(10));
    REQUIRE(leaf->IsArrayBound());
    REQUIRE(leaf->GetBackendKind() == XDataBackendKind::DPI);
    leaf->Set(uint64_t(42));
    REQUIRE(writes == 0);
    leaf->WriteOnRise();
    REQUIRE(writes == 1);
    REQUIRE(selected == 10);
    REQUIRE(dpi_values[10][0].aval == 42);
    leaf->WriteOnRise();
    REQUIRE(writes == 1);
    leaf->AsFallWrite(); leaf->Set(43); leaf->WriteOnRise();
    REQUIRE(writes == 1);
    leaf->WriteOnFall();
    REQUIRE(writes == 2);
    leaf->AsImmWrite(); leaf->Set(44);
    REQUIRE(writes == 3);
    leaf->At(64).Set(1);
    REQUIRE(dpi_values[10][2].aval == 1);
    auto slice = leaf->SubDataRef(4, 4);
    slice->Set(15);
    REQUIRE((dpi_values[10][0].aval & 0xf0) == 0xf0);
    dpi_values[10][0].bval = 2;
    REQUIRE(leaf->XMask() == 2);
    REQUIRE_FALSE(leaf->DataValid());
    ArrayBinding readonly(65, {0}, {17});
    readonly.BindDPIPtr(reinterpret_cast<uint64_t>(get_vector), 0);
    REQUIRE(readonly.AtFlat(10)->IsReadonly());
    REQUIRE(readonly.AtFlat(10)->mIOType == IOType::Output);
}

TEST_CASE("indexed scalar DPI preserves scalar calling convention", "[xarray]") {
    ArrayBinding array(0, {2, -1}, {3, 1});
    array.BindDPIPtr(reinterpret_cast<uint64_t>(get_bit), reinterpret_cast<uint64_t>(set_bit));
    for (uint64_t i = 0; i < 6; ++i) {
        auto leaf = array.AtFlat(i);
        leaf->AsImmWrite(); leaf->Set(int(i % 4));
        REQUIRE(dpi_bits[i] == i % 4);
        REQUIRE(leaf->U() == i % 4);
    }
}

TEST_CASE("wide array value watchers refresh validity without an XPort", "[xarray][xtrigger]") {
    std::memset(dpi_values, 0, sizeof(dpi_values));
    dpi_values[0][0].aval = 7;
    dpi_values[0][2].aval = 1;
    dpi_values[0][2].bval = 1;
    ArrayBinding array(65, {3}, {2});
    array.BindDPIPtr(reinterpret_cast<uint64_t>(get_vector), 0);
    auto leaf = array.At({3});
    REQUIRE_FALSE(leaf->DataValid());

    XClock clock([](bool) { return 0; });
    XTriggerEngine engine(clock);
    engine.AcquireClock();
    std::vector<unsigned char> expected(9, 0);
    expected[0] = 7;
    expected[8] = 1;
    auto watcher = engine.ArmValueEqBytes(leaf.get(), expected,
        XPhase::RisingStable, XConditionMode::EachSample, 0);
    REQUIRE(watcher.IsValid());

    // Change only simulator-owned storage, leaving the leaf's cache unknown.
    dpi_values[0][2].bval = 0;
    auto known = engine.RunUntil(2);
    REQUIRE(known.hits.size() == 1);
    REQUIRE(known.hits[0].tick == 2);
    REQUIRE(leaf->DataValid());

    REQUIRE(engine.Rearm(watcher));
    dpi_values[0][2].bval = 1;
    REQUIRE(engine.RunUntil(2).hits.empty());
    REQUIRE_FALSE(leaf->DataValid());
    dpi_values[0][2].bval = 0;
    REQUIRE(engine.RunUntil(2).hits.size() == 1);
    REQUIRE(engine.Disarm(watcher));
}

TEST_CASE("native arrays use actual strides and RTL direction", "[xarray]") {
    uint8_t bytes[3][2][3]{};
    ArrayBinding array(8, {-1, 3, 4}, {1, 2, 6});
    array.BindNativeData(reinterpret_cast<uint64_t>(bytes), {sizeof(bytes[0]), sizeof(bytes[0][0]), 1});
    REQUIRE(array.Address(10) == reinterpret_cast<uint64_t>(&bytes[1][0][1]));
    auto leaf = array.At({0, 2, 5});
    leaf->AsImmWrite(); leaf->Set(0xa5);
    REQUIRE(bytes[1][0][1] == 0xa5);
    REQUIRE(leaf->IsArrayBound());
    REQUIRE(leaf->GetBackendKind() == XDataBackendKind::MemDirect);

    std::vector<unsigned char> wide(141, 0);
    std::shared_ptr<XData> survivor;
    {
        ArrayBinding padded(65, {0, 0}, {1, 2});
        padded.BindNativeData(reinterpret_cast<uint64_t>(wide.data() + 1), {80, 24});
        survivor = padded.AtFlat(5);
        survivor->AsImmWrite(); survivor->Set(uint64_t(0x123456789ULL));
        uint64_t value = 0;
        std::memcpy(&value, wide.data() + 1 + 80 + 48, sizeof(value));
        REQUIRE(value == 0x123456789ULL);
        REQUIRE(wide[0] == 0);
        REQUIRE(wide[1 + 80 + 48 + 12 - 1] == 0);
    }
    survivor->Set(99); // Backend state outlives the array view; model storage is still live.
    REQUIRE(wide[1 + 80 + 48] == 99);
    uint32_t separate[3]{};
    survivor->BindNativeData(reinterpret_cast<uint64_t>(separate));
    REQUIRE_FALSE(survivor->IsArrayBound());
    survivor->Set(123);
    REQUIRE(separate[0] == 123);
    REQUIRE(wide[1 + 80 + 48] == 99);
}

TEST_CASE("native array widths preserve existing bval and do not overrun", "[xarray]") {
    for (uint32_t width : {0U, 1U, 8U, 9U, 16U, 17U, 32U, 33U, 64U, 65U, 96U, 512U, 577U}) {
        const size_t bytes = width <= 8 ? 1 : width <= 16 ? 2 : ((width + 31) / 32) * 4;
        std::vector<unsigned char> memory(2 * bytes + 2, 0);
        ArrayBinding array(width, {0}, {1});
        array.BindNativeData(reinterpret_cast<uint64_t>(memory.data() + 1));
        auto leaf = array.AtFlat(1);
        leaf->AsImmWrite(); leaf->Set(width <= 1 ? 1 : 0x5a);
        REQUIRE(memory[1 + bytes] == (width <= 1 ? 1 : 0x5a));
        REQUIRE(memory.front() == 0);
        REQUIRE(memory.back() == 0);
        if (width) {
            leaf->pVecData[0].bval = 1;
            REQUIRE(leaf->XMask() == 1);
        }
        leaf->ReInit(width, IOType::InOut);
        REQUIRE_FALSE(leaf->IsArrayBound());
    }
}

TEST_CASE("array leaves preserve notifications and standalone copies", "[xarray]") {
    uint32_t memory[2]{1, 2};
    ArrayBinding array(32, {0}, {1});
    array.BindNativeData(reinterpret_cast<uint64_t>(memory));
    auto leaf = array.AtFlat(0);
    int notifications = 0;
    leaf->OnChange([&](bool valid, XData *source, uint64_t value, void *) {
        REQUIRE(valid);
        REQUIRE(source == leaf.get());
        REQUIRE(value == 7);
        ++notifications;
    });
    leaf->AsImmWrite(); leaf->Set(7);
    REQUIRE(notifications == 1);
    XData snapshot(*leaf);
    REQUIRE_FALSE(snapshot.IsArrayBound());
    snapshot.Set(9);
    REQUIRE(memory[0] == 7);
    leaf->ClearOnChangeCbs();
    uint32_t other[2]{11, 12};
    ArrayBinding replacement(32, {0}, {1});
    replacement.BindNativeData(reinterpret_cast<uint64_t>(other));
    replacement.BindElement(*leaf, 1);
    REQUIRE(leaf->U() == 12);
    leaf->Set(13);
    REQUIRE(other[1] == 13);
    REQUIRE(memory[0] == 7);
    ArrayBinding singleton(32, {0, 0}, {0, 1});
    singleton.BindNativeData(reinterpret_cast<uint64_t>(other), {0, 4});
    REQUIRE(singleton.At({0, 1})->U() == 13);
}

TEST_CASE("array bindings reject invalid or overflowing layouts", "[xarray]") {
    uint32_t data[6]{};
    ArrayBinding array(32, {0, 0}, {1, 2});
    REQUIRE_THROWS_AS(array.AtFlat(0), std::logic_error);
    REQUIRE_THROWS_AS(array.BindNativeData(0), std::invalid_argument);
    REQUIRE_THROWS_AS(array.BindNativeData(reinterpret_cast<uint64_t>(data), {4}), std::invalid_argument);
    REQUIRE_THROWS_AS(array.BindNativeData(reinterpret_cast<uint64_t>(data), {4, 4}), std::invalid_argument);
    REQUIRE_THROWS_AS(array.BindNativeData(UINT64_MAX, {12, 4}), std::overflow_error);
    array.BindNativeData(reinterpret_cast<uint64_t>(data));
    REQUIRE_THROWS_AS(array.BindNativeData(reinterpret_cast<uint64_t>(data)), std::logic_error);
    XData wrong(8, IOType::InOut);
    REQUIRE_THROWS_AS(array.BindElement(wrong, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(array.AtFlat(6), std::out_of_range);
    ArrayBinding huge(8, {0, 0}, {65535, 65535});
    REQUIRE_THROWS_AS(huge.BindDPIPtr(reinterpret_cast<uint64_t>(get_vector), 0), std::invalid_argument);
}

TEST_CASE("internal arrays expose cached recursive XArray nodes", "[xarray]") {
    uint32_t memory[2]{11, 12};
    XSignalCFG cfg(R"(
variables:
  - name: mem
    offset: 0
    rtl_width: 32
    mem_bytes: 4
    array_size: 2
    array_dimensions: [2]
    array_strides: [4]
)", reinterpret_cast<uint64_t>(memory));
    auto first = cfg.NewXArray("mem");
    auto second = cfg.NewXArray("mem");
    REQUIRE(first == second);
    REQUIRE(first->Data(0) == second->Data(0));
    REQUIRE(first->Shape() == std::vector<int>{2});
    first->Data(1)->AsImmWrite(); first->Data(1)->Set(99);
    REQUIRE(second->Data(1)->U() == 99);
}

TEST_CASE("XSignalCFG requires explicit array dimensions and strides", "[xarray]") {
    uint32_t memory[2]{};
    const std::string yaml = R"(
variables:
  - name: mem
    offset: 0
    rtl_width: 32
    mem_bytes: 4
    array_size: 2
)";
    for (const auto &layout : {
            "",
            "    array_strides: [4]\n",
            "    array_dimensions: [2]\n",
            "    array_dimensions: []\n    array_strides: []\n",
            "    array_dimensions: [2]\n    array_strides: []\n",
            "    array_dimensions: [3]\n    array_strides: [4]\n",
            "    array_dimensions: [0]\n    array_strides: [4]\n",
            "    array_dimensions: [2]\n    array_strides: [8, 4]\n"}) {
        INFO(layout);
        XSignalCFG cfg(yaml + layout, reinterpret_cast<uint64_t>(memory));
        REQUIRE(cfg.At("mem").is_empty);
        REQUIRE_FALSE(cfg.NewXArray("mem"));
        REQUIRE(cfg.NewXData("mem", 0) == nullptr);
    }
}

TEST_CASE("XSignalCFG consumes multidimensional native layout without closures", "[xarray]") {
    unsigned char bytes[140]{};
    const std::string yaml = R"(
variables:
  - name: Top.mem
    offset: 0
    rtl_width: 65
    mem_bytes: 12
    array_size: 6
    array_dimensions: [2, 3]
    array_strides: [80, 24]
)";
    XSignalCFG cfg(yaml, reinterpret_cast<uint64_t>(bytes));
    auto array = cfg.NewXArray("Top.mem");
    REQUIRE(array);
    REQUIRE(array == cfg.NewXArray("Top.mem"));
    REQUIRE(array->Shape() == std::vector<int>{2, 3});
    auto leaf = array->Array(1)->Data(2);
    REQUIRE(array->Size() == 2);
    REQUIRE(leaf->IsArrayBound());
    REQUIRE(leaf->mName == "Top.mem[1][2]");
    leaf->AsImmWrite(); leaf->Set(77);
    REQUIRE(bytes[128] == 77);
    REQUIRE(array->Array(1)->Data(2)->U() == 77);
    auto selected_leaf = std::unique_ptr<XData>(cfg.NewXData("Top.mem", 5));
    REQUIRE(selected_leaf->IsArrayBound());
    REQUIRE(selected_leaf->U() == 77);
    REQUIRE_THROWS_AS(cfg.NewXData("Top.mem", -1), std::out_of_range);
    REQUIRE_THROWS_AS(cfg.NewXData("Top.mem", 6), std::out_of_range);
}

TEST_CASE("public XArray factories preserve recursive indexing and shared leaf identity", "[xarray]") {
    uint8_t bytes[3][2][3]{};
    auto array = XArray::FromNative(8, {-1, 3, 4}, {1, 2, 6},
        reinterpret_cast<uint64_t>(bytes), {6, 3, 1});
    REQUIRE(array->Left() == -1);
    REQUIRE(array->Right() == 1);
    REQUIRE(array->Size() == 3);
    REQUIRE(array->Shape() == std::vector<int>{3, 2, 3});
    std::shared_ptr<XArray> row = array->At(0);
    auto leaf = row->Array(2)->Data(5);
    REQUIRE(leaf == array->Array(0)->Array(2)->Data(5));
    leaf->AsImmWrite(); leaf->Set(0xa5);
    REQUIRE(bytes[1][0][1] == 0xa5);
    REQUIRE_THROWS_AS(array->Data(0), std::invalid_argument);
    REQUIRE_THROWS_AS(array->At(-2), std::out_of_range);
    array.reset(); row.reset();
    leaf->Set(0x5a);
    REQUIRE(bytes[1][0][1] == 0x5a);
    auto dpi = XArray::FromDPI(65, {3}, {2}, reinterpret_cast<uint64_t>(get_vector), 0);
    REQUIRE(dpi->Data(3)->IsReadonly());
    REQUIRE(dpi->Data(3)->GetBackendKind() == XDataBackendKind::DPI);
}

TEST_CASE("public XArray factories validate bounds and backend contracts", "[xarray]") {
    uint8_t bytes[16]{};
    const auto base = reinterpret_cast<uint64_t>(bytes);
    REQUIRE_THROWS_AS(XArray::FromNative(8, {}, {}, base), std::invalid_argument);
    REQUIRE_THROWS_AS(XArray::FromNative(8, {0}, {1, 2}, base), std::invalid_argument);
    REQUIRE_THROWS_AS(XArray::FromNative(8, {0}, {1}, 0), std::invalid_argument);
    REQUIRE_THROWS_AS(XArray::FromNative(8, {0, 0}, {1, 2}, base, {2, 1}), std::invalid_argument);
    REQUIRE_THROWS_AS(XArray::FromDPI(8, {0}, {1}, 0, 0), std::invalid_argument);
    auto array = XArray::FromNative(8, {INT_MIN, INT_MAX}, {INT_MIN + 1, INT_MAX - 1}, base);
    REQUIRE(array->Shape() == std::vector<int>{2, 2});
    auto row = array->Array(INT_MIN);
    array.reset();
    auto leaf = row->Data(INT_MAX);
    leaf->AsImmWrite(); leaf->Set(42);
    REQUIRE(bytes[1] == 42);
}
