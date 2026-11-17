#include "catch.hpp"
#include "xspcomm/xcomm.h"
#include <memory>
#include <stdexcept>
#include <sstream>

using namespace xspcomm;

namespace {
const std::string record_type = R"({"kind":"struct","packed":true,"width":9,"fields":[
 {"name":"payload","offset":1,"type":{"kind":"array","packed":true,"width":8,"left":7,"right":0,
  "element":{"kind":"bits","packed":true,"width":0}}},
 {"name":"valid","offset":0,"type":{"kind":"bits","packed":true,"width":0}}]})";
}

TEST_CASE("Data owns an immutable type and explicit views preserve values and leaf identity", "[layout]") {
    XData data(9, IOType::Input, "packet");
    REQUIRE_FALSE(data.HasLayout());
    REQUIRE_THROWS_AS(data.ArrayView(), std::invalid_argument);
    REQUIRE_THROWS_AS(data.StructView(), std::invalid_argument);
    data.SetLayout(record_type);
    REQUIRE(data.HasLayout());
    REQUIRE_THROWS_AS(data.ArrayView(), std::invalid_argument);
    auto payload = data.StructView()->Data("payload");
    REQUIRE(payload->HasLayout());
    REQUIRE(payload == data.StructView()->Data("payload"));
    REQUIRE(payload->ArrayView()->Left() == 7);
    REQUIRE(payload->ArrayView()->Data(7) == payload->ArrayView()->Data(7));
    payload->Set(0xa5);
    data.At(0).Set(1); // Existing Data indexing remains a zero-based bit offset.
    REQUIRE(data.U() == 0x14b);
    payload->ArrayView()->Data(7)->Set(0);
    REQUIRE(data.U() == 0x4b);
    REQUIRE_THROWS_AS(data.SetLayout(R"({"kind":"bits","packed":true,"width":8})"), std::invalid_argument);
    REQUIRE(data.StructView()->Data("payload") == payload);
    XData snapshot(data);
    REQUIRE(snapshot.HasLayout());
    snapshot.StructView()->Data("valid")->Set(0);
    REQUIRE(snapshot.U() == 0x4a);
    REQUIRE(data.U() == 0x4b);
}

TEST_CASE("ReInit invalidates associated types and previously selected fields even at the same width", "[layout]") {
    XData data(9, IOType::Input);
    data.SetLayout(record_type);
    auto view = data.StructView();
    auto field = view->Data("payload");
    REQUIRE_FALSE(data.SubDataRef(0, 3)->HasLayout());
    data.ReInit(9, IOType::Input);
    REQUIRE_FALSE(data.HasLayout());
    REQUIRE_THROWS_AS(data.StructView(), std::invalid_argument);
    REQUIRE_THROWS_AS(view->Data("payload"), std::runtime_error);
    REQUIRE_THROWS_AS(field->U(), std::runtime_error);
    data.SetLayout(record_type);
    REQUIRE_NOTHROW(data.StructView());
}

TEST_CASE("a full-width child retains its own type without replacing the parent type", "[layout]") {
    XData data(9, IOType::Input);
    data.SetLayout("{\"kind\":\"struct\",\"packed\":true,\"width\":9,\"fields\":["
        "{\"name\":\"child\",\"offset\":0,\"type\":" + record_type + "}]}");
    auto child = data.StructView()->Data("child");
    REQUIRE(child.get() != &data);
    REQUIRE(child->HasLayout());
    child->StructView()->Data("valid")->Set(1);
    REQUIRE(data.U() == 1);
    REQUIRE(data.StructView()->Has("child"));
    REQUIRE_FALSE(data.StructView()->Has("valid"));
}

TEST_CASE("native Data retains its selected type after configuration destruction", "[layout][native]") {
    uint16_t storage = 0;
    std::unique_ptr<XData> data;
    {
        XSignalCFG cfg("variables: [{name: packet, offset: 0, rtl_width: 9, mem_bytes: 2, layout_id: t}]\n"
            "types: {t: " + record_type + "}\n", reinterpret_cast<uint64_t>(&storage));
        data.reset(cfg.NewXData("packet"));
        REQUIRE(data->HasLayout());
    }
    data->AsImmWrite();
    data->StructView()->Data("payload")->Set(0x5a);
    REQUIRE(storage == 0xb4);
    REQUIRE(data->StructView()->Data("payload")->ArrayView()->Shape() == std::vector<int>{8});
}

TEST_CASE("a cached shared type cannot bypass the total layout depth limit", "[layout][native]") {
    std::ostringstream text;
    text << "types:\n  t0: {kind: bits, packed: true, width: 32}\n";
    for (unsigned i = 1; i <= 129; ++i)
        text << "  t" << i << ": {kind: array, packed: true, width: 32, left: 0, right: 0, element: t" << i - 1 << "}\n";
    text << "variables:\n"
         << "  - {name: small, offset: 0, mem_bytes: 4, rtl_width: 32, layout_id: t120}\n"
         << "  - {name: deep, offset: 0, mem_bytes: 4, rtl_width: 32, layout_id: t129}\n";
    uint32_t memory = 0;
    XSignalCFG cfg(text.str(), reinterpret_cast<uint64_t>(&memory));
    auto small = std::unique_ptr<XData>(cfg.NewXData("small"));
    REQUIRE(small->HasLayout());
    REQUIRE_THROWS_AS(cfg.NewXData("deep"), std::invalid_argument);
    REQUIRE_THROWS_AS(cfg.NewXData("deep"), std::invalid_argument);
    REQUIRE_NOTHROW(small->ArrayView());
}

TEST_CASE("plain vector types expose declared bounds only through explicit array interpretation", "[layout]") {
    for (const auto &range : std::vector<std::pair<int, int>>{{7, 4}, {0, 3}, {-2, 1}}) {
        XData value(4, IOType::Input);
        value.SetLayout("{\"kind\":\"bits\",\"packed\":true,\"width\":4,\"left\":" +
            std::to_string(range.first) + ",\"right\":" + std::to_string(range.second) + "}");
        auto view = value.ArrayView();
        REQUIRE(view->Left() == range.first);
        REQUIRE(view->Right() == range.second);
        view->Data(range.first)->Set(1);
        REQUIRE(value.U() == 8);
        REQUIRE(value.At(0).AsInt32() == 0);
        value.At(0).Set(1);
        REQUIRE(view->Data(range.second)->U() == 1);
        REQUIRE(value.ArrayView()->Data(range.first) == view->Data(range.first));
        REQUIRE_THROWS_AS(value.StructView(), std::invalid_argument);
        REQUIRE(value.ArrayView()->Data(range.first)->U() == 1);
    }
    XData scalar(0, IOType::Input);
    scalar.SetLayout(R"({"kind":"bits","packed":true,"width":0})");
    REQUIRE_THROWS_AS(scalar.ArrayView(), std::invalid_argument);
    XData vector(4, IOType::Input);
    REQUIRE_THROWS_AS(vector.SetLayout(R"({"kind":"bits","packed":true,"width":4,"left":7,"right":0})"), std::invalid_argument);
}

TEST_CASE("explicit manual views cannot associate a full-width child as the parent's type", "[layout]") {
    XData data(9, IOType::Input);
    const auto layout = "{\"kind\":\"struct\",\"packed\":true,\"width\":9,\"fields\":["
        "{\"name\":\"child\",\"offset\":0,\"type\":" + record_type + "}]}";
    auto view = data.StructView(layout);
    auto child = view->Data("child");
    REQUIRE(child.get() != &data);
    REQUIRE(child->HasLayout());
    REQUIRE_FALSE(data.HasLayout());
    REQUIRE(view->PackedData().get() == &data);
    REQUIRE(data.StructView()->Has("child"));
    child->StructView()->Data("valid")->Set(1);
    REQUIRE(data.U() == 1);
}

TEST_CASE("Data's view cache does not retain an owning source view", "[layout]") {
    uint16_t storage = 0;
    std::weak_ptr<XData> observed;
    std::shared_ptr<XData> field;
    std::shared_ptr<XStruct> explicit_view;
    {
        auto original = XView::FromNative(reinterpret_cast<uint64_t>(&storage), record_type);
        auto data = original.PackedData();
        observed = data;
        explicit_view = data->StructView();
        field = explicit_view->Data("payload");
        field->ArrayView()->Data(7)->Set(1);
        REQUIRE(storage == 0x100);
    }
    REQUIRE(observed.expired());
    REQUIRE_THROWS_AS(field->U(), std::runtime_error);
    REQUIRE_THROWS_AS(explicit_view->PackedData(), std::runtime_error);
}

TEST_CASE("associated views preserve DPI commit modes and both bit planes", "[layout][dpi]") {
    for (const auto mode : {WriteMode::Rise, WriteMode::Fall, WriteMode::Imme}) {
        xsvLogicVecVal storage{};
        unsigned reads = 0, writes = 0;
        // Scheduled inputs retain the pending local value. InOut retains its
        // existing backend-refresh policy and is exercised in immediate mode.
        XData parent(9, mode == WriteMode::Imme ? IOType::InOut : IOType::Input, "packet");
        parent.BindDPIRW([&](void *data) { ++reads; *static_cast<xsvLogicVecVal *>(data) = storage; },
                        [&](void *data) { ++writes; storage = *static_cast<xsvLogicVecVal *>(data); });
        parent.SetWriteMode(mode);
        parent.SetLayout(record_type);
        const auto before = reads;
        auto fields = parent.StructView();
        REQUIRE(reads == before);
        REQUIRE(writes == 0);
        REQUIRE(parent.GetBackendKind() == XDataBackendKind::DPI);
        REQUIRE(parent.GetWriteMode() == mode);
        fields->Data("payload")->Set(0xa5);
        fields->Data("valid")->Set(1);
        REQUIRE(parent.U() == 0x14b);
        if (mode == WriteMode::Rise) { REQUIRE(writes == 0); parent.WriteOnFall(); REQUIRE(writes == 0); parent.WriteOnRise(); }
        if (mode == WriteMode::Fall) { REQUIRE(writes == 0); parent.WriteOnRise(); REQUIRE(writes == 0); parent.WriteOnFall(); }
        REQUIRE(writes > 0);
        REQUIRE(storage.aval == 0x14b);
        fields->Data("payload")->Set("0xx5");
        if (mode == WriteMode::Rise) parent.WriteOnRise();
        if (mode == WriteMode::Fall) parent.WriteOnFall();
        REQUIRE(storage.bval == 0x1e0);
        REQUIRE((storage.aval & 0x1f) == 0xb);
        REQUIRE(parent.GetWriteMode() == mode);
    }
}
