#define CATCH_CONFIG_MAIN
#include "catch.hpp"

#include "xspcomm/xvpi_view.h"

#include <array>
#include <climits>
#include <map>
#include <memory>
#include <vector>

using namespace xspcomm;

namespace {

struct MockVpiObject {
    PLI_INT32 type = vpiReg;
    PLI_INT32 width = 64;
    PLI_INT32 direction = vpiOutput;
    bool scalar = false, is_signed = false, packed = false;
    std::string name;
    std::array<s_vpi_vecval, 2> value{};
    MockVpiObject *left = nullptr, *right = nullptr;
    std::vector<MockVpiObject *> ranges, members;
    std::map<int, MockVpiObject *> elements;
    size_t next = 0;
    int references = 0, releases = 0;
};

MockVpiObject *as_mock(vpiHandle handle){
    return reinterpret_cast<MockVpiObject *>(handle);
}

PLI_INT32 mock_vpi_get(PLI_INT32 property, vpiHandle handle){
    auto obj = as_mock(handle);
    if(property == vpiType) return obj->type;
    if(property == vpiSize) return obj->width;
    if(property == vpiDirection) return obj->direction;
    if(property == vpiScalar) return obj->scalar;
    if(property == vpiSigned) return obj->is_signed;
    if(property == vpiPacked) return obj->packed;
    return 0;
}

void mock_vpi_get_value(vpiHandle handle, p_vpi_value value){
    auto obj = as_mock(handle);
    if(value->format == vpiIntVal){
        value->value.integer = (PLI_INT32)obj->value[0].aval;
        return;
    }
    if(value->format == vpiScalarVal){
        value->value.scalar = obj->value[0].aval | (obj->value[0].bval << 1);
        return;
    }
    if(value->format == vpiVectorVal){
        // VPI returns simulator-owned vector storage through this pointer.
        value->value.vector = obj->value.data();
    }
}

vpiHandle mock_vpi_put_value(vpiHandle handle, p_vpi_value value,
                            p_vpi_time, PLI_INT32){
    auto obj = as_mock(handle);
    if(value->format == vpiIntVal){
        obj->value[0].aval = (PLI_UINT32)value->value.integer;
        obj->value[0].bval = 0;
    }else if(value->format == vpiScalarVal){
        obj->value[0] = {PLI_UINT32(value->value.scalar & 1), PLI_UINT32((value->value.scalar >> 1) & 1)};
    }else if(value->format == vpiVectorVal){
        for(int i = 0; i < (obj->width + 31) / 32; i++){
            obj->value[i] = value->value.vector[i];
        }
    }
    return handle;
}

vpiHandle as_handle(MockVpiObject &obj){
    return reinterpret_cast<vpiHandle>(&obj);
}

// Simulate the same function table used by a real XVPI session, including
// separately acquired handles and VPI's self-releasing exhausted iterators.
struct MockVpiModel;
MockVpiModel *active_model = nullptr;
struct MockVpiModel {
    std::vector<std::unique_ptr<MockVpiObject>> objects;
    std::map<std::string, MockVpiObject *> names;
    XVPI session;

    MockVpiModel() { active_model = this; }
    ~MockVpiModel() { session.Close(); active_model = nullptr; }
    MockVpiObject &Add(const std::string &name = "", int type = vpiReg, int width = 64) {
        objects.push_back(std::make_unique<MockVpiObject>());
        auto &obj = *objects.back();
        obj.name = name;
        obj.type = type;
        obj.width = width;
        obj.direction = vpiInput;
        if (!name.empty()) names.emplace(name, &obj);
        return obj;
    }
    void Bounds(MockVpiObject &obj, int left, int right) {
        obj.left = &Add();
        obj.right = &Add();
        obj.left->value[0].aval = PLI_UINT32(left);
        obj.right->value[0].aval = PLI_UINT32(right);
    }
    MockVpiObject &Vector(const std::string &name, int width,
                          std::initializer_list<std::pair<int, int>> ranges) {
        auto &obj = Add(name, vpiReg, width);
        for (const auto &[left, right] : ranges) {
            auto &range = Add("", vpiRange);
            Bounds(range, left, right);
            obj.ranges.push_back(&range);
        }
        return obj;
    }
    int Outstanding() const {
        int count = 0;
        for (const auto &obj : objects) count += std::abs(obj->references);
        return count;
    }
    int Releases() const {
        int count = 0;
        for (const auto &obj : objects) count += obj->releases;
        return count;
    }
    void Configure(const std::string &layouts = "{}");
};

vpiHandle acquire(MockVpiObject *obj) {
    if (!obj) return nullptr;
    ++obj->references;
    return as_handle(*obj);
}
PLI_INT32 mock_vpi_release(vpiHandle handle) {
    auto obj = as_mock(handle);
    --obj->references;
    ++obj->releases;
    return 1;
}
vpiHandle mock_vpi_by_name(char *name, vpiHandle parent) {
    if (parent) {
        for (auto member : as_mock(parent)->members)
            if (member->name == name) return acquire(member);
        return nullptr;
    }
    const auto found = active_model->names.find(name);
    return found == active_model->names.end() ? nullptr : acquire(found->second);
}
vpiHandle mock_vpi_by_index(vpiHandle handle, PLI_INT32 index) {
    const auto &elements = as_mock(handle)->elements;
    const auto found = elements.find(index);
    return found == elements.end() ? nullptr : acquire(found->second);
}
vpiHandle mock_vpi_handle(PLI_INT32 type, vpiHandle handle) {
    auto obj = as_mock(handle);
    return acquire(type == vpiLeftRange ? obj->left : type == vpiRightRange ? obj->right : nullptr);
}
vpiHandle mock_vpi_iterate(PLI_INT32 type, vpiHandle handle) {
    auto obj = as_mock(handle);
    const auto &items = type == vpiRange ? obj->ranges : obj->members;
    if (items.empty()) return nullptr;
    auto &iter = active_model->Add();
    iter.members = items;
    return acquire(&iter);
}
vpiHandle mock_vpi_scan(vpiHandle handle) {
    auto iter = as_mock(handle);
    if (iter->next < iter->members.size()) return acquire(iter->members[iter->next++]);
    mock_vpi_release(handle);
    return nullptr;
}
PLI_BYTE8 *mock_vpi_get_str(PLI_INT32, vpiHandle handle) {
    return as_mock(handle)->name.data();
}
void MockVpiModel::Configure(const std::string &layouts) {
    session.Configure(reinterpret_cast<uint64_t>(mock_vpi_get),
        reinterpret_cast<uint64_t>(mock_vpi_get_value), reinterpret_cast<uint64_t>(mock_vpi_put_value),
        reinterpret_cast<uint64_t>(mock_vpi_by_name), reinterpret_cast<uint64_t>(mock_vpi_by_index),
        reinterpret_cast<uint64_t>(mock_vpi_handle), reinterpret_cast<uint64_t>(mock_vpi_iterate),
        reinterpret_cast<uint64_t>(mock_vpi_scan), reinterpret_cast<uint64_t>(mock_vpi_get_str),
        reinterpret_cast<uint64_t>(mock_vpi_release), "", layouts);
}

} // namespace

TEST_CASE("VPI reads 64-bit vectors from returned storage", "[xvpi]") {
    MockVpiObject obj;
    obj.value[0] = {0x89abcdefU, 0};
    obj.value[1] = {0x01234567U, 0x00000010U};

    std::unique_ptr<XData> data(XData::FromVPI(
        as_handle(obj), mock_vpi_get, mock_vpi_get_value, mock_vpi_put_value,
        "vpi_read64"));

    REQUIRE(data != nullptr);
    REQUIRE(data->W() == 64);
    REQUIRE(data->U() == 0x0123456789abcdefULL);
    REQUIRE_FALSE(data->DataValid());
}

TEST_CASE("VPI writes 64-bit vectors as two words", "[xvpi]") {
    MockVpiObject obj;
    obj.direction = vpiInput;

    std::unique_ptr<XData> data(XData::FromVPI(
        as_handle(obj), mock_vpi_get, mock_vpi_get_value, mock_vpi_put_value,
        "vpi_write64"));

    REQUIRE(data != nullptr);
    *data = 0xfedcba9876543210ULL;
    REQUIRE(obj.value[0].aval == 0x76543210U);
    REQUIRE(obj.value[1].aval == 0xfedcba98U);
    REQUIRE(obj.value[0].bval == 0);
    REQUIRE(obj.value[1].bval == 0);
}

TEST_CASE("XVPI discovers scalars and single packed dimensions as bits", "[xvpi][discover]") {
    for (const auto &range : std::vector<std::pair<int, int>>{{0, 0}, {7, 7}, {63, 0}, {79, 16}, {-9, 54}}) {
        CAPTURE(range.first, range.second);
        const int width = int(std::abs(int64_t(range.first) - range.second)) + 1;
        for (int type : {vpiReg, vpiBitVar, vpiPackedArrayVar}) {
            MockVpiModel model;
            auto &obj = model.Vector("plain", width, {range});
            obj.type = type;
            obj.is_signed = true;
            model.Configure();
            const auto view = model.session.Get("plain");
            REQUIRE(view.Kind() == "bits");
            REQUIRE(view.IsPacked());
            REQUIRE(view.IsSigned());
            REQUIRE(view.IsFourState() == (type != vpiBitVar));
            const auto data = view.Data();
            REQUIRE(data->W() == (width == 1 ? 0 : width));
            REQUIRE(data->HasLayout());
            const auto explicit_array = data->ArrayView();
            REQUIRE(explicit_array->Left() == range.first);
            REQUIRE(explicit_array->Right() == range.second);
            REQUIRE(explicit_array->PackedData() == data);
            const uint64_t expected = width == 1 ? 1 : 0xfedcba9876543210ULL;
            data->Set(expected);
            REQUIRE(data->U() == expected);
            REQUIRE(obj.value[0].aval == PLI_UINT32(expected));
            REQUIRE(model.session.Get("plain").Data() == data);
            if (width == 64) {
                REQUIRE(data->S() == int64_t(expected));
                REQUIRE(obj.value[1].aval == PLI_UINT32(expected >> 32));
                if (type != vpiBitVar) {
                    data->Set("0x1234567x89abcdez");
                    REQUIRE_FALSE(data->DataValid());
                    REQUIRE(obj.value[0].bval == 0xf);
                    REQUIRE(obj.value[1].bval == 0xf);
                }
            }
            model.session.Close();
            REQUIRE(model.Outstanding() == 0);
            REQUIRE_THROWS_AS(data->U(), std::runtime_error);
            REQUIRE_THROWS_AS(view.Data(), std::runtime_error);
            REQUIRE_THROWS_AS(model.session.Get("plain"), std::runtime_error);
            const auto releases = model.Releases();
            model.session.Close();
            REQUIRE(model.Releases() == releases);
        }
    }
    for (bool scalar : {false, true}) {
        MockVpiModel model;
        auto &obj = model.Add("scalar", vpiReg, 1);
        obj.scalar = scalar; // Simulators may omit vpiRange even for a vector.
        model.Configure();
        const auto view = model.session.Get("scalar");
        REQUIRE(view.Kind() == "bits");
        view.Data()->Set("z");
        REQUIRE_FALSE(view.Data()->DataValid());
        REQUIRE(obj.value[0].bval == 1);
        model.session.Close();
        REQUIRE(model.Outstanding() == 0);
    }
}

TEST_CASE("XVPI discovers unpacked arrays with wide XData elements", "[xvpi][discover]") {
    for (const auto &range : std::vector<std::pair<int, int>>{{3, 2}, {-2, -1}}) {
        MockVpiModel model;
        auto &array = model.Add("memory", vpiRegArray);
        model.Bounds(array, range.first, range.second);
        array.elements[range.first] = &model.Vector("memory[" + std::to_string(range.first) + "]", 64, {{79, 16}});
        array.elements[range.second] = &model.Vector("memory[" + std::to_string(range.second) + "]", 64, {{79, 16}});
        model.Configure();
        const auto view = model.session.Get("memory");
        REQUIRE(view.Kind() == "array");
        REQUIRE_FALSE(view.IsPacked());
        auto values = view.Array();
        REQUIRE(values->Left() == range.first);
        REQUIRE(values->Right() == range.second);
        REQUIRE(values->Shape() == std::vector<int>{2});
        REQUIRE(values->At(range.first).Kind() == "bits");
        REQUIRE(values->At(range.second).Kind() == "bits");
        values->Data(range.first)->Set(uint64_t(0xfedcba9876543210ULL));
        REQUIRE(values->Data(range.first)->U() == 0xfedcba9876543210ULL);
        REQUIRE(values->Data(range.second)->U() == 0);
        REQUIRE(model.session.Get("memory[" + std::to_string(range.first) + "]").Kind() == "bits");
        REQUIRE_THROWS_AS(values->At(range.first + (range.first > range.second ? 1 : -1)), std::out_of_range);
        REQUIRE_THROWS_AS(view.PackedData(), std::invalid_argument);
        model.session.Close();
        REQUIRE(model.Outstanding() == 0);
    }
}

TEST_CASE("XVPI discovery preserves all dimensions of packed multidimensional values", "[xvpi][discover]") {
    MockVpiModel model;
    auto &obj = model.Vector("matrix", 8, {{1, 0}, {-2, 1}});
    obj.type = vpiPackedArrayVar;
    obj.value[0].aval = 0x81;
    model.Configure();
    const auto view = model.session.Get("matrix");
    REQUIRE(view.Kind() == "array");
    REQUIRE(view.IsPacked());
    auto matrix = view.Array();
    REQUIRE(matrix->Shape() == std::vector<int>{2, 4});
    REQUIRE(matrix->LeftBounds() == std::vector<int>{1, -2});
    REQUIRE(matrix->RightBounds() == std::vector<int>{0, 1});
    REQUIRE(matrix->At(1).Kind() == "array");
    REQUIRE(matrix->Array(1)->At(-2).Kind() == "bits");
    REQUIRE(matrix->Array(1)->Data(-2)->U() == 1);
    REQUIRE(matrix->Array(0)->Data(1)->U() == 1);
    matrix->Array(1)->Data(-1)->Set(1);
    REQUIRE(obj.value[0].aval == 0xc1);
    model.session.Close();
    REQUIRE(model.Outstanding() == 0);
}

TEST_CASE("XVPI preserves explicit one-dimensional packed aggregate and struct layouts", "[xvpi][discover]") {
    const std::string array_layout = R"({"kind":"array","packed":true,"width":8,"left":11,"right":4,
        "element":{"kind":"bits","packed":true,"width":0}})";
    const std::string struct_layout = R"({"kind":"struct","packed":true,"width":8,"fields":[
        {"name":"payload","offset":0,"type":)" + array_layout + "}]}";
    XData backing(8, IOType::Input);
    REQUIRE(XView::FromData(backing, array_layout).Kind() == "array");
    REQUIRE(XView::FromData(backing, struct_layout).Struct()->Field("payload").Kind() == "array");

    MockVpiModel model;
    model.Vector("declared", 8, {{11, 4}});
    auto &record = model.Add("record", vpiStructVar, 8);
    record.packed = true;
    model.Configure("{\"declared\":" + array_layout + ",\"record\":" + struct_layout + "}");
    const auto declared = model.session.Get("declared");
    REQUIRE(declared.Kind() == "array");
    REQUIRE(declared.Array()->Left() == 11);
    REQUIRE(declared.Array()->Right() == 4);
    declared.Array()->Data(11)->Set(1);
    REQUIRE(declared.PackedData()->U() == 0x80);
    const auto packed = model.session.Get("record");
    REQUIRE(packed.Kind() == "struct");
    REQUIRE(packed.Struct()->Field("payload").Kind() == "array");
    REQUIRE(model.session.Get("record.payload").Kind() == "array");
    packed.Struct()->Array("payload")->Data(4)->Set(1);
    REQUIRE(record.value[0].aval == 1);
    model.session.Close();
    REQUIRE(model.Outstanding() == 0);
}

TEST_CASE("XVPI discovers unpacked struct vector members without extra array nodes", "[xvpi][discover]") {
    MockVpiModel model;
    auto &record = model.Add("record", vpiStructVar);
    record.members.push_back(&model.Vector("payload", 64, {{-9, 54}}));
    model.Configure();
    const auto view = model.session.Get("record");
    REQUIRE(view.Kind() == "struct");
    REQUIRE_FALSE(view.IsPacked());
    REQUIRE(view.Struct()->Field("payload").Kind() == "bits");
    view.Struct()->Data("payload")->Set(uint64_t(0x123456789abcdef0ULL));
    REQUIRE(view.Struct()->Data("payload")->U() == 0x123456789abcdef0ULL);
    model.session.Close();
    REQUIRE(model.Outstanding() == 0);
}

TEST_CASE("XVPI discovery still validates widths and releases failed lookup handles", "[xvpi][discover]") {
    MockVpiModel model;
    model.Vector("valid", 64, {{63, 0}});
    model.Vector("mismatch", 64, {{62, 0}});
    model.Vector("multidim_mismatch", 64, {{1, 0}, {30, 0}});
    model.Vector("overflow", 64, {{INT_MIN, INT_MAX}});
    model.Vector("multidim_overflow", 64, {{65535, 0}, {65535, 0}});
    model.Add("zero_width", vpiReg, 0);
    auto &missing_bound = model.Vector("missing_bound", 64, {{63, 0}});
    missing_bound.ranges[0]->right = nullptr;
    auto &packed = model.Add("packed_struct", vpiStructVar, 64);
    packed.packed = true;
    model.Configure();
    auto cached = model.session.Get("valid").Data();
    const auto outstanding = model.Outstanding();
    REQUIRE_THROWS_WITH(model.session.Get("mismatch"), "VPI dimensions do not match whole packed width");
    REQUIRE_THROWS_WITH(model.session.Get("multidim_mismatch"), "VPI dimensions do not match whole packed width");
    REQUIRE_THROWS_WITH(model.session.Get("overflow"), "VPI packed dimensions overflow");
    REQUIRE_THROWS_WITH(model.session.Get("multidim_overflow"), "VPI packed dimensions overflow");
    REQUIRE_THROWS_WITH(model.session.Get("zero_width"), "VPI value has no positive bit width");
    REQUIRE_THROWS_WITH(model.session.Get("missing_bound"), "Simulator did not expose the requested VPI member or index");
    REQUIRE_THROWS_WITH(model.session.Get("packed_struct"), "Packed VPI struct requires RTL field offsets from Picker");
    REQUIRE_FALSE(model.session.Get("absent").HasValue());
    REQUIRE(model.Outstanding() == outstanding);
    REQUIRE(model.session.Get("valid").Data() == cached);
    cached->Set(uint64_t(0x123456789abcdef0ULL));
    REQUIRE(cached->U() == 0x123456789abcdef0ULL);
    model.session.Close();
    REQUIRE(model.Outstanding() == 0);
}
