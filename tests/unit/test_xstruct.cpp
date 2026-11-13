#define CATCH_CONFIG_MAIN
#include "catch.hpp"
#include "xspcomm/xcomm.h"
#include <stdexcept>
#include <type_traits>

using namespace xspcomm;

namespace {
const std::string packet_layout = R"({"kind":"struct","packed":true,"width":9,"fields":[
 {"name":"data","offset":1,"type":{"kind":"array","packed":true,"width":8,"left":7,"right":0,
  "element":{"kind":"bits","packed":true,"width":0}}},
 {"name":"valid","offset":0,"type":{"kind":"bits","packed":true,"width":0}}]})";
const std::string matrix_layout = R"({"kind":"array","packed":true,"width":8,"left":1,"right":0,
 "element":{"kind":"array","packed":true,"width":4,"left":-2,"right":1,
 "element":{"kind":"bits","packed":true,"width":0}}})";
xsvLogicVecVal dpi_value[3]{};
int reads = 0, writes = 0;
void read_value(void *value) { ++reads; std::memcpy(value, dpi_value, sizeof(dpi_value)); }
void read_word(void *value) { ++reads; *static_cast<xsvLogicVecVal *>(value) = dpi_value[0]; }
void write_value(const void *value) { ++writes; std::memcpy(dpi_value, value, sizeof(dpi_value)); }
}

TEST_CASE("XView implicitly converts only to the matching typed handle", "[xview]") {
    static_assert(std::is_convertible_v<XView, std::shared_ptr<XData>>);
    static_assert(std::is_convertible_v<XView, std::shared_ptr<XStruct>>);
    static_assert(std::is_convertible_v<XView, std::shared_ptr<XArray>>);
    static_assert(!std::is_convertible_v<XView, uint64_t>);
    XData parent(9, IOType::Input, "request");
    auto view = XView::FromData(parent, packet_layout);
    std::shared_ptr<XStruct> record = view;
    std::shared_ptr<XData> flag = record->Field("valid");
    std::shared_ptr<XArray> data = record->Field("data");
    REQUIRE(flag == record->Data("valid"));
    REQUIRE(data->PackedData() == record->Data("data"));
    flag->Set(1);
    REQUIRE(parent.U() == 1);
    REQUIRE(view.PackedData().get() == &parent);
    REQUIRE_THROWS_AS(static_cast<std::shared_ptr<XData>>(view), std::invalid_argument);
    REQUIRE_THROWS_AS(static_cast<std::shared_ptr<XData>>(record->Field("data")), std::invalid_argument);
    REQUIRE_THROWS_AS(view.Data(), std::invalid_argument);
    const auto original = data;
    REQUIRE_THROWS_AS(data = view, std::invalid_argument);
    REQUIRE(data == original);
    try {
        (void)view.Array();
        FAIL("wrong-kind conversion accepted");
    } catch (const std::invalid_argument &error) {
        const std::string message = error.what();
        REQUIRE(message.find("request") != std::string::npos);
        REQUIRE(message.find("actual=struct") != std::string::npos);
        REQUIRE(message.find("requested=array") != std::string::npos);
    }
}

TEST_CASE("XView checks empty handles and expired sources at conversion", "[xview]") {
    XView empty;
    REQUIRE_FALSE(empty.HasValue());
    REQUIRE_FALSE(bool(empty));
    REQUIRE_THROWS_AS(empty.Kind(), std::invalid_argument);
    REQUIRE_THROWS_AS(static_cast<std::shared_ptr<XData>>(empty), std::invalid_argument);
    REQUIRE_THROWS_AS(static_cast<std::shared_ptr<XStruct>>(empty), std::invalid_argument);
    REQUIRE_THROWS_AS(static_cast<std::shared_ptr<XArray>>(empty), std::invalid_argument);
    REQUIRE_THROWS_AS(empty.PackedData(), std::invalid_argument);
    XView expired;
    {
        XData parent(9, IOType::Input, "expired");
        expired = XView::FromData(parent, packet_layout);
        REQUIRE(expired.HasValue());
    }
    REQUIRE_THROWS_AS(static_cast<std::shared_ptr<XStruct>>(expired), std::runtime_error);
    XData resized(9, IOType::Input, "resized");
    const auto view = XView::FromData(resized, packet_layout);
    resized.ReInit(8, IOType::Input);
    REQUIRE_THROWS_AS(view.Struct(), std::runtime_error);
}

TEST_CASE("unpacked conversions validate every source in the selected subtree", "[xview]") {
    const std::string layout = R"({"kind":"struct","packed":false,"width":0,"fields":[
      {"name":"a","type":{"kind":"bits","packed":true,"width":8}},
      {"name":"z","type":{"kind":"bits","packed":true,"width":8}}]})";
    XData first(8, IOType::Input);
    auto last = std::make_unique<XData>(8, IOType::Input);
    XPort port;
    port.Add("record.a", first);
    port.Add("record.z", *last);
    const auto view = XView::FromPort(port, "record", layout);
    REQUIRE_NOTHROW(view.Struct());
    last.reset();
    REQUIRE_THROWS_AS(view.Struct(), std::runtime_error);
}

TEST_CASE("checked view conversions do not read write or register signals", "[xview]") {
    XData parent(9, IOType::Output, "readonly");
    parent.BindDPIPtr(reinterpret_cast<uint64_t>(read_word), 0);
    const auto before_reads = reads, before_writes = writes;
    const auto view = XView::FromData(parent, packet_layout);
    std::shared_ptr<XStruct> record = view;
    std::shared_ptr<XArray> data = record->Field("data");
    std::shared_ptr<XData> whole = XView::FromData(parent,
        R"({"kind":"bits","packed":true,"width":9})");
    REQUIRE(whole.get() == &parent);
    REQUIRE(whole->IsOutIO());
    REQUIRE_THROWS_AS(data->At(7).Struct(), std::invalid_argument);
    REQUIRE(reads == before_reads);
    REQUIRE(writes == before_writes);
}

TEST_CASE("packed array view preserves RTL bounds and maps nested slices", "[xstruct]") {
    XData parent(8, IOType::Input, "matrix");
    parent.Set(0xa5);
    auto view = parent.ArrayView(matrix_layout);
    REQUIRE(view->Shape() == std::vector<int>{2, 4});
    REQUIRE(view->Size() == 2);
    REQUIRE(view->Array(1)->Left() == -2);
    REQUIRE(view->Array(1)->Data(-2)->mWidth == 0);
    REQUIRE(view->Array(1)->Data(-2)->U() == 1);
    REQUIRE(view->Array(0)->Data(1)->U() == 1);
    view->Array(1)->Data(-1)->Set(1);
    REQUIRE(parent.U() == 0xe5);
    REQUIRE_THROWS_AS(view->At(2), std::out_of_range);
    REQUIRE_THROWS_AS(view->Array(0)->At(-3), std::out_of_range);
}

TEST_CASE("packed struct field identity and parent value are shared", "[xstruct]") {
    XData packet(9, IOType::Input, "packet");
    packet.Set(0x101);
    auto root = packet.StructView(packet_layout);
    REQUIRE(root->Names() == std::vector<std::string>{"data", "valid"});
    REQUIRE(root->Data("valid")->mWidth == 0);
    REQUIRE(root->Data("data")->mWidth == 8);
    REQUIRE(root->Data("valid")->U() == 1);
    REQUIRE(root->Data("data") == root->Data("data"));
    root->Data("data")->Set(0x5a);
    REQUIRE(packet.U() == 0xb5);
    REQUIRE(root->Array("data")->Data(7)->U() == 0);
    REQUIRE_THROWS_AS(root->Field("missing"), std::out_of_range);
}

TEST_CASE("unpacked structs contain independent recursive XData fields", "[xstruct]") {
    const std::string layout = R"({"kind":"struct","packed":false,"width":0,"fields":[
      {"name":"flag","type":{"kind":"bits","packed":true,"width":0}},
      {"name":"rows","type":{"kind":"array","packed":false,"width":0,"left":-1,"right":0,
       "element":{"kind":"struct","packed":false,"width":0,"fields":[
        {"name":"value","type":{"kind":"bits","packed":true,"width":8}}]}}}]})";
    XPort port;
    XData flag(0, IOType::Input), a(8, IOType::Input), b(8, IOType::Input);
    port.Add("packet.flag", flag);
    port.Add("packet.rows[-1].value", a);
    port.Add("packet.rows[0].value", b);
    std::shared_ptr<XStruct> root = XView::FromPort(port, "packet", layout);
    REQUIRE_FALSE(root->IsPacked());
    REQUIRE(root->Data("flag").get() == &flag);
    root->Array("rows")->Struct(-1)->Data("value")->Set(3);
    REQUIRE(a.U() == 3);
    REQUIRE(b.U() == 0);
    REQUIRE_THROWS_AS(root->PackedData(), std::invalid_argument);
}

TEST_CASE("slices retain four-state values and parent's scheduled writes", "[xstruct]") {
    std::memset(dpi_value, 0, sizeof(dpi_value));
    writes = 0;
    XData parent(65, IOType::Input);
    parent.BindDPIPtr(reinterpret_cast<uint64_t>(read_value), reinterpret_cast<uint64_t>(write_value));
    auto a = parent.SubDataRef(31, 33);
    auto b = parent.SubDataRef(0, 1);
    a->Set("0x1xxxxxxxx");
    b->Set(1);
    REQUIRE(writes == 0);
    REQUIRE_FALSE(parent.DataValid());
    parent.WriteOnRise();
    REQUIRE(writes == 1);
    REQUIRE((dpi_value[0].aval & 1) == 1);
    REQUIRE(dpi_value[1].bval != 0);
    parent.AsImmWrite();
    dpi_value[0].aval = 0x12;
    auto low = parent.SubDataRef(0, 4);
    low->Set(5);
    REQUIRE((dpi_value[0].aval & 0xff) == 0x15);
    REQUIRE(writes == 2);
}

TEST_CASE("views validate widths and detect destroyed or resized parents", "[xstruct]") {
    XData wrong(8, IOType::Input);
    REQUIRE_THROWS_AS(wrong.StructView(packet_layout), std::invalid_argument);
    REQUIRE_THROWS_AS(wrong.SubDataRef(UINT32_MAX, 2), std::out_of_range);
    REQUIRE_THROWS_AS(wrong.SubDataRef(8, 0), std::out_of_range);
    std::shared_ptr<XStruct> view;
    std::shared_ptr<XData> field;
    {
        XData parent(9, IOType::Input);
        view = parent.StructView(packet_layout);
        field = view->Data("data");
    }
    REQUIRE_THROWS_AS(view->Data("data"), std::runtime_error);
    REQUIRE_THROWS_AS(field->U(), std::runtime_error);
    XData parent(9, IOType::Input);
    view = parent.StructView(packet_layout);
    field = view->Data("valid");
    parent.ReInit(8, IOType::Input);
    REQUIRE_THROWS_AS(view->Data("valid"), std::runtime_error);
    REQUIRE_THROWS_AS(field->U(), std::runtime_error);
}

TEST_CASE("recursive layouts reject inconsistent packed fields before creating slices", "[xstruct]") {
    XData parent(8, IOType::Input);
    for (const auto &layout : {
        R"({"kind":"bits","packed":true,"width":-1})",
        R"({"kind":"array","packed":true,"width":8,"left":-2147483648,"right":2147483647,
             "element":{"kind":"bits","packed":true,"width":0}})",
        R"({"kind":"array","packed":true,"width":8,"left":1,"right":0,
             "element":{"kind":"bits","packed":true,"width":3}})",
        R"({"kind":"struct","packed":true,"width":8,"fields":[
             {"name":"x","offset":0,"type":{"kind":"bits","packed":true,"width":4}},
             {"name":"x","offset":4,"type":{"kind":"bits","packed":true,"width":4}}]})",
        R"({"kind":"struct","packed":true,"width":8,"fields":[
             {"name":"a","offset":0,"type":{"kind":"bits","packed":true,"width":4}},
             {"name":"b","offset":3,"type":{"kind":"bits","packed":true,"width":4}}]})",
        R"({"kind":"struct","packed":true,"width":8,"fields":[
             {"name":"a","offset":1,"type":{"kind":"bits","packed":true,"width":7}}]})",
        R"({"kind":"array","packed":true,"width":8,"left":0,"right":0,
             "element":{"kind":"struct","packed":false,"width":0,"fields":[
               {"name":"x","type":{"kind":"bits","packed":true,"width":8}}]}})"
    }) {
        REQUIRE_THROWS_AS(XView::FromData(parent, layout), std::invalid_argument);
    }
    XPort port;
    REQUIRE_THROWS_AS(XView::FromPort(port, "p", packet_layout), std::invalid_argument);
    port.Add("p", parent);
    REQUIRE_THROWS_AS(XView::FromPort(port, "p", packet_layout), std::invalid_argument);
}

TEST_CASE("immediate slices refresh before deduplicating and notify after merging", "[xstruct]") {
    std::memset(dpi_value, 0, sizeof(dpi_value));
    writes = 0;
    XData parent(65, IOType::Input);
    parent.BindDPIPtr(reinterpret_cast<uint64_t>(read_value), reinterpret_cast<uint64_t>(write_value));
    parent.AsImmWrite();
    auto low = parent.SubDataRef(0, 4);
    uint64_t seen = 0;
    int notifications = 0;
    parent.OnChange([&](bool, XData *, uint64_t, void *) {
        seen = low->U();
        ++notifications;
    });
    dpi_value[0].aval = 0x12;
    low->Set(5);
    REQUIRE(dpi_value[0].aval == 0x15);
    REQUIRE(seen == 5);
    REQUIRE(notifications == 1);
    dpi_value[0].aval = 0x22;
    low->Set(5);
    REQUIRE(dpi_value[0].aval == 0x25);
    REQUIRE(writes == 2);
    REQUIRE(notifications == 2);
    dpi_value[0].aval = 0x22;
    low->Set(5);
    REQUIRE(dpi_value[0].aval == 0x25);
    REQUIRE(writes == 3);
}

TEST_CASE("slice word boundaries preserve both bit planes and siblings", "[xstruct]") {
    for (uint32_t offset = 0; offset < 97; ++offset) {
        for (uint32_t width : {0U, 1U, 2U, 31U, 32U, 33U, 64U, 65U, 96U}) {
            const auto bits = width ? width : 1;
            if (offset + bits > 97) continue;
            XData parent(97, IOType::Input);
            parent.Set("0x1x1234567z89abcdef01234567");
            std::vector<xsvLogicVecVal> before(parent.pVecData, parent.pVecData + 4);
            auto slice = parent.SubDataRef(offset, width);
            for (uint32_t i = 0; i < bits; ++i) {
                const auto p = offset + i;
                const uint32_t aval = (before[p / 32].aval >> (p % 32)) & 1;
                const uint32_t bval = (before[p / 32].bval >> (p % 32)) & 1;
                const auto actual = width ? slice->At(int(i)).AsInt32() : int(slice->U());
                REQUIRE(actual == int(aval | (bval << 1)));
            }
            slice->Set(width ? "0x5xzx9" : "x");
            for (uint32_t p = 0; p < 97; ++p) {
                const auto word = parent.pVecData[p / 32];
                if (p < offset || p >= offset + bits) {
                    REQUIRE(((word.aval ^ before[p / 32].aval) & (uint32_t(1) << (p % 32))) == 0);
                    REQUIRE(((word.bval ^ before[p / 32].bval) & (uint32_t(1) << (p % 32))) == 0);
                }
            }
            auto nested = slice->SubDataRef(0, 0);
            nested->Set("z");
            REQUIRE(((parent.pVecData[offset / 32].aval >> (offset % 32)) & 1) == 0);
            REQUIRE(((parent.pVecData[offset / 32].bval >> (offset % 32)) & 1) == 1);
        }
    }
}
