#include "catch.hpp"
#include "xspcomm/xcomm.h"
#include <cstddef>
#include <sstream>

using namespace xspcomm;

namespace {
struct NativeRecord { uint8_t tag; uint32_t payload[3]; uint32_t packed; };

std::string bindings() {
    std::ostringstream out;
    out << "[";
    for (const auto &field : std::vector<std::tuple<std::string, int, size_t, size_t>>{
            {".tag", 8, sizeof(uint8_t), offsetof(NativeRecord, tag)},
            {".payload", 65, sizeof(NativeRecord::payload), offsetof(NativeRecord, payload)},
            {".packed", 32, sizeof(uint32_t), offsetof(NativeRecord, packed)}}) {
        if (out.str().size() > 1) out << ',';
        out << "{\"path\":\"[]" << std::get<0>(field) << "\",\"rtl_width\":" << std::get<1>(field)
            << ",\"mem_bytes\":" << std::get<2>(field) << ",\"offset\":" << std::get<3>(field)
            << ",\"array_dimensions\":[2],\"array_strides\":[" << sizeof(NativeRecord) << "]}";
    }
    return out.str() + "]";
}
const std::string layout = R"({"kind":"array","packed":false,"width":0,"left":3,"right":2,
 "element":{"kind":"struct","packed":false,"width":0,"fields":[
 {"name":"tag","type":{"kind":"bits","packed":true,"width":8}},
 {"name":"payload","type":{"kind":"bits","packed":true,"width":65}},
 {"name":"packed","type":{"kind":"struct","packed":true,"width":32,"fields":[
  {"name":"high","offset":24,"type":{"kind":"bits","packed":true,"width":8}},
  {"name":"low","offset":0,"type":{"kind":"bits","packed":true,"width":24}}]}}
 ]}})";
}

TEST_CASE("native recursive views bind struct fields across arrays without flattening", "[xview][native]") {
    NativeRecord storage[2]{};
    auto view = XView::FromNativeBindings(reinterpret_cast<uint64_t>(storage), layout, bindings());
    REQUIRE(view.Array()->Left() == 3);
    REQUIRE(view.Array()->Right() == 2);
    auto first = view.Array()->Struct(3);
    auto last = view.Array()->Struct(2);
    first->Data("tag")->Set(0xab);
    first->Data("payload")->Set("0x10000000012345678");
    first->Struct("packed")->Data("high")->Set(0xcd);
    first->Struct("packed")->Data("low")->Set(0x123456);
    REQUIRE(storage[1].tag == 0xab);
    REQUIRE(storage[1].payload[0] == 0x12345678);
    REQUIRE(storage[1].payload[2] == 1);
    REQUIRE(storage[1].packed == 0xcd123456);
    REQUIRE(storage[0].packed == 0);
    last->Data("tag")->Set(0xef);
    REQUIRE(storage[0].tag == 0xef);
    REQUIRE(first->Data("tag") == view.Array()->Struct(3)->Data("tag"));
    REQUIRE_THROWS_AS(view.PackedData(), std::invalid_argument);
    REQUIRE_THROWS_AS(view.Array()->At(1), std::out_of_range);
    REQUIRE_THROWS_AS(first->Data("missing"), std::out_of_range);
}

TEST_CASE("native type references are shared and preserve storage-coordinate APIs", "[xview][native]") {
    uint32_t words[2][3][3]{};
    const std::string yaml = R"(
types:
  value: {kind: bits, packed: true, width: 65, signed: true}
  row: {kind: array, packed: false, width: 0, left: -1, right: 1, element: value}
  memory: {kind: array, packed: false, width: 0, left: 3, right: 2, element: row}
variables:
  - name: Top.memory
    type: VlWide<3>
    mem_bytes: 12
    rtl_width: 65
    offset: 0
    array_size: 6
    array_dimensions: [2, 3]
    array_strides: [36, 12]
    layout_id: memory
    bindings:
      - {path: "[][]", rtl_width: 65, mem_bytes: 12, offset: 0, array_dimensions: [2, 3], array_strides: [36, 12]}
)";
    XSignalCFG cfg(yaml, reinterpret_cast<uint64_t>(words));
    auto rtl = cfg.NewXView("Top.memory").Array();
    REQUIRE(rtl->LeftBounds() == std::vector<int>{3, -1});
    auto leaf = rtl->Array(3)->Data(1);
    leaf->Set("0x10000000011223344");
    REQUIRE(words[1][2][0] == 0x11223344);
    REQUIRE(words[1][2][2] == 1);
    REQUIRE(cfg.NewXView("Top.memory[3][1]").Data() == leaf);
    REQUIRE(cfg.NewXView("Top.memory").Array()->Array(3)->At(1).IsSigned());
    auto storage = cfg.NewXArray("Top.memory");
    REQUIRE(storage->LeftBounds() == std::vector<int>{0, 0});
    REQUIRE(storage->Array(1)->Data(2)->U() == 0x11223344);
    REQUIRE_FALSE(cfg.NewXView("Top.absent").HasValue());
    REQUIRE_THROWS_AS(cfg.NewXView("Top.memory[3][5]"), std::out_of_range);
    REQUIRE_THROWS_AS(cfg.NewXView("Top.memory[3][x]"), std::invalid_argument);
}

TEST_CASE("native metadata rejects invalid type references and unsafe storage bindings", "[xview][native]") {
    uint32_t value = 0x12345678;
    const auto base = reinterpret_cast<uint64_t>(&value);
    const std::string bits = R"({"kind":"bits","packed":true,"width":32})";
    REQUIRE_THROWS_AS(XView::FromNativeBindings(base, bits, "[]"), std::invalid_argument);
    REQUIRE_THROWS_AS(XView::FromNativeBindings(base, bits,
        R"([{"path":"","rtl_width":32,"mem_bytes":2,"offset":0,"array_dimensions":[],"array_strides":[]}])"), std::invalid_argument);
    REQUIRE_THROWS_AS(XView::FromNativeBindings(base, bits,
        R"([{"path":"","rtl_width":32,"mem_bytes":4,"offset":0,"array_dimensions":[1],"array_strides":[4]}])"), std::invalid_argument);
    REQUIRE_THROWS_AS(XView::FromNativeBindings(0, bits, "[]"), std::invalid_argument);
    REQUIRE_THROWS_AS(XView::FromNativeBindings(UINT64_MAX - 1, bits,
        R"([{"path":"","rtl_width":32,"mem_bytes":4,"offset":4,"array_dimensions":[],"array_strides":[]}])"), std::overflow_error);
    const std::string config = R"(
variables:
  - {name: x, offset: 0, mem_bytes: 4, rtl_width: 32, layout_id: t}
types:
  t: {kind: array, packed: false, width: 0, left: 0, right: 1, element: t}
)";
    XSignalCFG cyclic(config, base);
    REQUIRE_THROWS_AS(cyclic.NewXView("x"), std::invalid_argument);
    XSignalCFG unknown(R"(variables: [{name: x, offset: 0, mem_bytes: 4, rtl_width: 32, layout_id: missing}])", base);
    REQUIRE_THROWS_AS(unknown.NewXView("x"), std::invalid_argument);
    REQUIRE(value == 0x12345678);
}

TEST_CASE("native direct aliases retain a verified type instead of erasing it", "[xview][native]") {
    uint32_t value = 0x12345678;
    const std::string config = R"(
types:
  byte: {kind: bits, packed: true, width: 8}
  data: {kind: bits, packed: true, width: 24}
  packet:
    kind: struct
    packed: true
    width: 32
    fields:
      - {name: tag, offset: 24, type: byte}
      - {name: data, offset: 0, type: data}
variables:
  - {name: Top.packet, offset: 0, mem_bytes: 4, rtl_width: 32, layout_id: packet}
signals:
  - {name: Top.packet, kind: direct, offset: 0, mem_bytes: 4, rtl_width: 32, source: Top.packet}
)";
    XSignalCFG cfg(config, reinterpret_cast<uint64_t>(&value));
    auto packet = cfg.NewXView("Top.packet");
    REQUIRE(packet.Kind() == "struct");
    packet.Struct()->Data("tag")->Set(0xab);
    REQUIRE(value == 0xab345678);
    REQUIRE(cfg.NewXView("Top.packet.tag").Data() == packet.Struct()->Data("tag"));
    const std::string mismatch = R"(
types:
  t: {kind: bits, packed: true, width: 64}
variables:
  - {name: x, offset: 0, mem_bytes: 4, rtl_width: 32, layout_id: t}
)";
    XSignalCFG bad(mismatch, reinterpret_cast<uint64_t>(&value));
    REQUIRE_THROWS_AS(bad.NewXView("x"), std::invalid_argument);
    XSignalCFG unavailable(R"(variables: []
signals: [{name: missing, kind: unavailable}])", reinterpret_cast<uint64_t>(&value));
    REQUIRE(unavailable.NewXData("missing") == nullptr);
}
